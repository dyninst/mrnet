/****************************************************************************
 *  Copyright � 2011 The Regents of the University of New Mexico            *
 *  All Rights Reserved                                                     *
 *  Created by Josh Goehner and Dorian Arnold                               *
 *  Department of Computer Science                                          *
 *  Detailed usage rights in "LICENSE" file.                                *
 ****************************************************************************/

#include <cassert>
#include <vector>
#include <map>
#include <set>
#include <queue>
#include <stdarg.h>
#include <stdlib.h>

#include "libi/debug.h"
#include "libi/ProcessGroup.h"
#include "xplat/SocketUtils.h"
#include "xplat/Process.h"
#include "xplat/NetUtils.h"
#include "xplat/Thread.h"
#include "xplat/Monitor.h"

using namespace LIBI;
using namespace XPlat;
using namespace std;

#define MAX_CHILDREN_RESOURCE_LIMIT 1000

//const float REMOTE_LAUNCH = 0.117;  //from cache
const float REMOTE_LAUNCH = 0.257;  //from file system
const float LOCAL_LAUNCH = 0.029;
const float SEQUENTIAL_WAIT = 0.007;  //0.009



typedef struct _host_desc {
    unsigned int count;
    int * dist;
} host_desc;

typedef struct _topo_node {
    string hn;                       //hostname
    int dist;                        //dist of first proc on node
    host_desc* coloc;                //pointer to host_desc, which describes all of the processes on this node
    vector<_topo_node *> children;   //children to launch, in order from begin() to end()
} topo_node;

typedef struct _possible_position {
    float start_time;
    topo_node* parent;
} possible_position;

struct priority_order{
  bool operator()(possible_position* p1, possible_position* p2) const
  {
    return (p1->start_time > p2->start_time);
  }
};

struct list_order{
  bool operator()(possible_position* p1, possible_position* p2) const
  {
    return (p1->start_time < p2->start_time);
  }
};

typedef struct _accept_args{
    int listener;
    int* master;
    topo_state* report;
    Monitor * master_connected;
    debug* dbg;
} accept_args;


int ProcessGroup::fe_count = 0;

const int sequential = -1;
const int greedy = 0;
string create_topology(int tree_type,
                        dist_req_t reqs[],
                        int nreq,
                        int & num_procs,
                        char** & host_list,
                        float remote_time,
                        float sequential_time,
                        debug* dbg );
void extract_topology( topo_node* node, int & rank, int nreq, string & topo, char** & host_list );
void pack( void * & send_buf, int & send_buf_size, env_t *env,
           dist_req_t distributions[], unsigned int nreq, unsigned int num_procs,
           string launch_topo, debug* dbg );
void* accept_master( void* args );

ProcessGroup::ProcessGroup(){
    dbg = new debug(true);
    dbg->getParametersFromEnvironment();
    char id[15];
    sprintf(id, "FE%i", fe_count++);
    dbg->set_identifier(id);

    isLaunched=false;
    isFinalized=false;
    report = NONE;
}
ProcessGroup::~ProcessGroup(){
    finalize();
}

/* socket_message_type is no longer valid as of 4.0.0
void socketmsgy( XPlat::socket_message_type type, char *msg, const char *file,
                    int line, const char * func){
    //fprintf( stderr, "%s(%i) %s: %s\n", file, line, func, msg );
}
*/

char* addEnvVarToEnvList(const char* name, env_t* &env, debug* dbg){
    char* envvar = getenv( name );
    env_t* env_cur = env;
    bool has_val=false;
    if( envvar != NULL ){
        while( env_cur != NULL && !has_val){
            if( strcmp(env_cur->name,name)==0 )
                has_val=true;
            env_cur = env_cur->next;
        }
        if(!has_val){
            env_cur = (env_t*)malloc( sizeof(env_t) );
            env_cur->name = strdup(name);
            env_cur->value = strdup(envvar);
            env_cur->next = env;
            env = env_cur;
            dbg->print(FE_LAUNCH_INFO, FL, "adding %s=%s to env", name, envvar );
        }
        return strdup(envvar);
    }
    return NULL;
}

libi_rc_e ProcessGroup::launch(dist_req_t distributions[], int nreq, env_t* env){
    timeval t1,t2;
    gettimeofday(&t1,NULL);
    env_t* environment = env;
    char* envval = addEnvVarToEnvList("LIBI_DEBUG", environment, dbg);
    free(envval);
    envval = addEnvVarToEnvList("LIBI_DEBUG_DIR", environment, dbg);
    free(envval);
    envval = addEnvVarToEnvList("XPLAT_RSH", environment, dbg);
    if( envval != NULL ){
        dbg->print(FE_LAUNCH_INFO, FL, "using %s for remote shell operations.", envval);
        XPlat::Process::set_RemoteShell( string(envval) );
    } else
        dbg->print(FE_LAUNCH_INFO, FL, "using XPLAT's default remote shell program.");
    free(envval);
    envval = addEnvVarToEnvList("XPLAT_RSH_ARGS", environment, dbg);
    if( envval != NULL ){
        dbg->print(FE_LAUNCH_INFO, FL, "using %s for remote shell arguments.", envval);
        XPlat::Process::set_RemoteShellArgs( string(envval) );
    }
    free(envval);
    envval = addEnvVarToEnvList("XPLAT_REMCMD", environment, dbg);
    if( envval != NULL ){
        dbg->print(FE_LAUNCH_INFO, FL, "using %s for remote command.", envval);
        XPlat::Process::set_RemoteCommand( string(envval) );
    }
    free(envval);

    if( nreq <= 0){
        dbg->print(ERRORS, FL, "error: must have at least one distribution" );
        return LIBI_ELAUNCH;
    }
    dist_req_t* first_dist = &distributions[0];
    if( first_dist->hd == NULL ){
        dbg->print(ERRORS, FL, "error: must have at least one host" );
        return LIBI_ELAUNCH;
    }

    int tree_type=0;
    float remote_time=REMOTE_LAUNCH;
    float sequential_time=SEQUENTIAL_WAIT;
    char* LIBI_TREE_TYPE = getenv("LIBI_TREE_TYPE");
    char** endptr;
    if( LIBI_TREE_TYPE != NULL )
        tree_type=atoi( LIBI_TREE_TYPE );
    char* LIBI_REMOTE_TIME = getenv("LIBI_REMOTE_TIME");
    if( LIBI_REMOTE_TIME != NULL){
        endptr=&LIBI_REMOTE_TIME;
        remote_time=strtof( LIBI_REMOTE_TIME, endptr );
        if( remote_time == 0 && endptr==&LIBI_REMOTE_TIME )
            remote_time = REMOTE_LAUNCH;
    }
    char* LIBI_SEQUENTIAL_TIME = getenv("LIBI_SEQUENTIAL_TIME");
    if( LIBI_SEQUENTIAL_TIME != NULL){
        endptr=&LIBI_SEQUENTIAL_TIME;
        sequential_time=strtof( LIBI_SEQUENTIAL_TIME, endptr );
        if( sequential_time == 0 && endptr==&LIBI_SEQUENTIAL_TIME )
            sequential_time = SEQUENTIAL_WAIT;
    }
    dbg->print(FE_LAUNCH_INFO, FL, "Using remote time:%f sequetial time:%f", remote_time, sequential_time);

    //bind to a listening port
    XPlat_Socket listener;
    XPlat_Port listen_port = 0;
    //XPlat::SocketUtils::setCallback( &socketmsgy );
    if ( false == XPlat::SocketUtils::CreateListening( listener, listen_port, false )){
		dbg->print(FE_LAUNCH_INFO, FL, "CreateListening Failure.");
	}

    XPlat::Monitor all_connected;
    all_connected.RegisterCondition( ALL_CONNECTED );

    //launch master
    int rc;
    vector<string> args;
    string cmd = first_dist->proc_path;
    args.push_back( cmd );

    string hn;
    XPlat::NetUtils::GetLocalHostName( hn );
    args.push_back( string( "--libi-phost=" ) + hn );

    char* arg = (char*)malloc( 50 );
    snprintf( arg, 50, "--libi-pport=%i", listen_port );
    args.push_back( string(arg) );
    args.push_back( string( "--libi-pposi=0" ) );


    string master_host = first_dist->hd->hostname;
    dbg->print(FE_LAUNCH_INFO, FL, "root:%s proc:%s", master_host.c_str(), cmd.c_str() );

    XPlat::Thread::Id accept_thread_id;

    XPlat::Monitor master_connected;
    master_connected.RegisterCondition( MASTER_CONNECTED );


    dbg->print(FE_LAUNCH_INFO, FL, "creating separate thread for accepting the masters connection" );
    accept_args aargs;
    master_fd = -1;
    report = NONE;
    aargs.listener = listener;
    aargs.report = &report;
    aargs.master = &master_fd;
    aargs.master_connected = &master_connected;
    aargs.dbg = dbg;
    //  &accept_master is a function, &accept_thread_id is a long int, and &aargs
    XPlat::Thread::Create( &accept_master, (void*)&aargs, &accept_thread_id );

    dbg->print(FE_LAUNCH_INFO, FL, "launching master." );
    rc = XPlat::Process::Create( master_host, cmd, args );
    if( rc != 0 ){
        int err = XPlat::Process::GetLastError();
        dbg->print(ERRORS, FL, "error: failed to launch master on %s: '%s'", master_host.c_str(), strerror(err) );
        vector<string>::iterator args_iter = args.begin();
        int args_i=0;
        while( args_iter != args.end() ){
            dbg->print(ERRORS, FL, "arg[%i]=%s", args_i, (*args_iter).c_str() );
            args_iter++;
            args_i++;
        }
        return LIBI_ELAUNCH;
    }

    //create the topology
    dbg->print(FE_LAUNCH_INFO, FL, "creating topology of type: %i", tree_type );
    string launch_topo = create_topology( tree_type, distributions, nreq, num_procs, host_list, remote_time, sequential_time, dbg );
    dbg->print(FE_LAUNCH_INFO, FL, "topology:%s", launch_topo.c_str() );

    //pack everything into a buffer
    void* send_buf;
    int send_buf_size;

	// Look at packto see what it does, it seems like it also initiates the connect back
    pack( send_buf, send_buf_size, environment, distributions, nreq, num_procs, launch_topo, dbg );

    //wait for connect back
    dbg->print(FE_LAUNCH_INFO, FL, "waiting for master to connect to %s:%u", hn.c_str(), listen_port );
    master_connected.Lock();
    while( report == NONE )
        master_connected.WaitOnCondition( MASTER_CONNECTED );
    master_connected.Unlock();
    if( master_fd == -1 )
        return LIBI_ELAUNCH;

    //send topology
    dbg->print(FE_LAUNCH_INFO, FL, "sending topology of size:%u to master", send_buf_size );
    rc = write( master_fd, &send_buf_size, sizeof( unsigned int ) );
    if( rc < 0){
        dbg->print(ERRORS, FL, "error: could not send arg size" );
        return LIBI_ELAUNCH;
    }
    rc = write( master_fd, send_buf, send_buf_size );
    if( rc < 0){
        dbg->print(ERRORS, FL, "error: could not send args" );
        return LIBI_ELAUNCH;
    }
    gettimeofday(&t2,NULL);
    double d1 = ((double)t2.tv_sec + (double)t2.tv_usec / 1000000.0) - ((double)t1.tv_sec + (double)t1.tv_usec / 1000000.0);
    dbg->print(FE_LAUNCH_INFO, FL, "pre-launch time:%f", d1 );

    //wait for report of ready
    dbg->print(FE_LAUNCH_INFO, FL, "waiting for master report ready. report=%i MASTER_CONNECTED=%i", report, MASTER_CONNECTED );
    fd_set master_fds;
    FD_ZERO( &master_fds );
    FD_SET( master_fd, &master_fds );
    fd_set ready_fds = master_fds;
    int num_waiting;
    while( report == MASTER_CONNECTED){
        num_waiting = select( master_fd+1, &ready_fds, NULL, NULL, NULL );
        if( num_waiting < 0 )
            report = FAILURE;
        else if( num_waiting > 0 && FD_ISSET( master_fd, &ready_fds ) ){
            report = ALL_CONNECTED;
            isLaunched=true;
        }
    }
    if( report == FAILURE ){
        dbg->print(ERRORS, FL, "error: could not recieve acknolwedgement from master" );
        return LIBI_ELAUNCH;
    }
    char done_msg;
    rc = read( master_fd, &done_msg, 1 );  //receive an r
    dbg->print(FE_LAUNCH_INFO, FL, "done_msg:%c. repor=%i ALL_CONNECTED=%i", done_msg, report, ALL_CONNECTED );
    return LIBI_OK;
}

libi_rc_e ProcessGroup::sendUsrData(void *msgbuf, int msgbuflen){
    if( !isLaunched )
        return LIBI_EINIT;

    dbg->print(FE_LAUNCH_INFO, FL, "sending size %i", msgbuflen );
    errno = 0;
    if( write( master_fd, &msgbuflen, sizeof(int) ) < 0 ){
        dbg->print(ERRORS, FL, "error(%i): could not write msg size to master: %s", errno, strerror(errno) );
        return LIBI_EIO;
    }
    if( msgbuflen > 0){
        errno = 0;
        if( write( master_fd, msgbuf, msgbuflen ) < 0 ){
            dbg->print(ERRORS, FL, "error(%i): could not write msg to master: %s", errno, strerror(errno) );
            return LIBI_EIO;
        }
        dbg->print(FE_LAUNCH_INFO, FL, "sent %i bytes", msgbuflen );
    }

    return LIBI_OK;
}

//caller responsible for freeing msgbuf and msgbuflen
libi_rc_e ProcessGroup::recvUsrData(void** msgbuf, int* msgbuflen){
    if( !isLaunched )
        return LIBI_EINIT;

    dbg->print(FE_LAUNCH_INFO, FL, "receiving from master" );
    errno = 0;
    if( read( master_fd, msgbuflen, sizeof(int) ) < 0 ){
        dbg->print(ERRORS, FL, "error(%i): could not read msg size from master: %s", errno, strerror(errno) );
        return LIBI_EIO;
    }
    dbg->print(FE_LAUNCH_INFO, FL, "receiving size %i", (*msgbuflen) );
    if( (*msgbuflen) > 0 ){
        (*msgbuf) = malloc( (*msgbuflen) );
        dbg->print(FE_LAUNCH_INFO, FL, "allocated size %i", (*msgbuflen) );
        errno = 0;
        if( read( master_fd, (*msgbuf), (*msgbuflen) ) < 0 ){
            dbg->print(ERRORS, FL, "error(%i): could not read msg from master: %s", errno, strerror(errno) );
            return LIBI_EIO;
        }
        dbg->print(FE_LAUNCH_INFO, FL, "received %i bytes", (*msgbuflen) );
    }
    dbg->print(FE_LAUNCH_INFO, FL, "received" );

    return LIBI_OK;
}

//caller responsible for freeing hostlist
libi_rc_e ProcessGroup::getHostlist(char*** hostlist, unsigned int* size){
    if( !isLaunched )
        return LIBI_EINIT;
    
    (*size) = num_procs;
    if( num_procs > 0 ){
        (*hostlist) = (char**)malloc( num_procs * sizeof(char*)  );
        char** hl = (*hostlist);
        for( int i = 0; i < num_procs; i++ )
            hl[i] = strdup(host_list[i]);
    } else
        (*hostlist)=NULL;

    return LIBI_OK;
}


libi_rc_e ProcessGroup::finalize(){

    SocketUtils::Close( master_fd );
    report = NONE;
    num_procs = 0;
    if( host_list != NULL ){
        for( int i = 0; i < num_procs; i++ )
            free( host_list[i] );
        delete[] host_list;
        host_list = NULL;
    }
    isLaunched=false;
    isFinalized=true;

    return LIBI_OK;
}

/*
 front end helper functions
 */
string create_topology(int tree_type, dist_req_t reqs[], int nreq, int & num_procs, char** & host_list, float remote_time, float sequential_time, debug* dbg ){
    //TODO: consider resource limits (cpus, threads, sockets)

    map<string, host_desc * > hosts;
    map<string, host_desc * >::iterator host_iter;
    host_t* cur;
    host_desc * desc;
    possible_position* pos;
    float child_start;
    topo_node* root;
    topo_node* cur_node;
    int i;

    //create distinct host list
    num_procs = 0;
    for(i=0; i<nreq; i++){
        cur = reqs[i].hd;
        while( cur != NULL ){
            host_iter = hosts.find( string(cur->hostname) );
            if( host_iter == hosts.end() ){
                desc = (host_desc*)malloc( sizeof(host_desc) );
                desc->count = cur->nproc;
                desc->dist = (int*)malloc( sizeof(int) * nreq );
                memset( (void*)desc->dist, 0, (sizeof(int) * nreq) );
                desc->dist[i] = cur->nproc;
                hosts[ string( strdup(cur->hostname) ) ] = desc;
            }else{
                host_iter->second->count += cur->nproc;
                host_iter->second->dist[i] += cur->nproc;
            }
            num_procs += cur->nproc;
            cur = cur->next;
        }
    }


    root = new topo_node;
    cur = reqs[0].hd;
    if( cur != NULL && cur->hostname != NULL )
        root->hn = string( cur->hostname );  //first host is root: consider changing this
    else
        dbg->print(ERRORS, FL, "error: root is not set." );
    root->dist = 0;
    host_iter = hosts.find( root->hn );
    root->coloc = host_iter->second;

    if( tree_type == greedy || tree_type < sequential ){
        priority_queue<possible_position*, vector<possible_position*>, priority_order> possible_positions; //sorted by launch_time
        pos = (possible_position*)malloc( sizeof(possible_position) );
        pos->start_time = 0;
        pos->parent = root;
        possible_positions.push( pos );

        host_iter = hosts.begin();
        while( host_iter != hosts.end() ){
            if( host_iter->first == root->hn ){
                host_iter++;
            } else {
                //get position with fastest start time
                pos = possible_positions.top();
                possible_positions.pop();

                //find first distribution to use this host
                for(i=0; i < nreq && host_iter->second->dist[i]==0; i++ );

                cur_node = new topo_node;
                cur_node->hn = host_iter->first;
                cur_node->dist = i;
                cur_node->coloc = NULL;
                cur_node->coloc = host_iter->second;

                //fprintf(stderr, "%s(%i) start:%f parent:%s sibling:%f, grand-child:%f.\n", __FUNCTION__, __LINE__, pos->start_time, pos->parent->hn.c_str(), pos->start_time + SEQUENTIAL_WAIT, pos->start_time + REMOTE_LAUNCH );

                pos->parent->children.push_back( cur_node );
                child_start = pos->start_time + remote_time;

                //add the sibling of currrent node, reuse pos
                pos->start_time += sequential_time;
                if( pos->parent->children.size() + pos->parent->coloc->count < MAX_CHILDREN_RESOURCE_LIMIT )
                    possible_positions.push( pos );

                //add the child of current node
                pos = (possible_position*)malloc( sizeof(possible_position) );
                pos->start_time = child_start;
                pos->parent = cur_node;
                if( pos->parent->coloc->count < MAX_CHILDREN_RESOURCE_LIMIT )
                    possible_positions.push( pos );

                host_iter++;
            }
        }
    } else if( tree_type > 0 ){
        queue<topo_node*> available_positions;
        topo_node* cur_parent = root;
        host_iter = hosts.begin();

        if( tree_type > MAX_CHILDREN_RESOURCE_LIMIT)
            fprintf(stderr, "%s(%i) warning: Max number of children may exceed resource limitations.");

        while( host_iter != hosts.end() ){
            if( host_iter->first == root->hn ){
                host_iter++;
            } else {
                if( cur_parent->children.size() >= tree_type  ){
                    cur_parent = available_positions.front();
                    available_positions.pop();
                }

                cur_node = new topo_node;
                cur_node->hn = host_iter->first;
                desc = host_iter->second;
                host_iter++;

                for(i=0; i < nreq && desc->dist[i]==0; i++ ); //find first distribution to use this host
                cur_node->dist = i;
                cur_node->coloc = NULL;
                cur_node->coloc = desc;

               cur_parent->children.push_back( cur_node );
               available_positions.push( cur_node );
            }
        }
    } else {
        //available_positions.push(root);
        host_iter = hosts.begin();
        while( host_iter != hosts.end() ){
            if( host_iter->first == root->hn ){
                host_iter++;
            } else {
                cur_node = new topo_node;
                cur_node->hn = host_iter->first;
                desc = host_iter->second;
                host_iter++;

                for(i=0; i < nreq && desc->dist[i]==0; i++ ); //find first distribution to use this host
                cur_node->dist = i;
                cur_node->coloc = NULL;
                cur_node->coloc = desc;
                root->children.push_back( cur_node );
                if( root->children.size() >= MAX_CHILDREN_RESOURCE_LIMIT ){
                    fprintf(stderr, "%s(%i) Resource limitations have been exceeded. Cannot launch all of the requested processes.");
                    break;
                }
            }
        }
    }

    string topo;
    int r=0;
    host_list = (char**)malloc( num_procs * sizeof( char * ) );
    extract_topology( root, r, nreq, topo, host_list );
    return topo;
}

void extract_topology( topo_node* node, int & rank, int nreq, string & topo, char** & host_list ){
    string ret;
    char* temp = (char*)malloc(25);
    vector<topo_node*>::iterator child;
    
    
    topo = node->hn + "|";
    const char* hn = node->hn.c_str();
    int hn_sz = strlen(hn)+1;

    //colocated procs
    int i,j,count;
    for(i = node->dist; i < nreq; i++){
        count = node->coloc->dist[i];
        if(count > 0){
            snprintf( temp, 25, "%i;", node->dist );
            topo += string( temp );
            for( j=1; j<count; j++){
                host_list[rank] = strdup(hn);
                snprintf( temp, 25, "%i:", rank++ );
                topo += string( temp );
            }
            host_list[rank] = strdup(hn);
            snprintf( temp, 25, "%i|", rank++ );
            topo += string( temp );
        }
    }

    //remote nodes
    if( !node->children.empty() ){
        topo += "{";
        child = node->children.begin();
        while( child != node->children.end() ){
            ret.clear();
            extract_topology( (*child), rank, nreq, ret, host_list );
            topo += ret + "," ;
            child++;
        }
        topo.erase( topo.size() - 1 );
        topo += "}";
    }
    //fprintf(stderr, "%s(%i) topo_part:%s\n", __FUNCTION__, __LINE__, ret.c_str() );
}

void* accept_master( void* args ){
    accept_args* aargs = (accept_args*)args;

    aargs->master_connected->Lock();

    int s = accept( aargs->listener, NULL, NULL );

    if( s < 0 ){
        (*aargs->report) = FAILURE;
        aargs->dbg->print(ERRORS, FL, "error: problems accepting master: %s", strerror( errno ) );
    }else{
        int temp = -1;
		//Taylor: I changed this to < 0 rather than <= 0
		//0 is returned on an EOF rather than error (-1)
        int retval = -1;
		retval = read(s, &temp, sizeof( int ) );
		if( retval < 0){
            aargs->dbg->print(ERRORS, FL, "error: problems reading the master position: %s, errno: %i", strerror( errno ), errno );
        }
		if( retval == 0){
            aargs->dbg->print(ERRORS, FL, "error: read returned 0 (EOF) when trying to read master position: %s, errno: %i", strerror( errno ), errno );
        }

        (*aargs->report) = MASTER_CONNECTED;
        (*aargs->master) = s;
        aargs->dbg->print( FE_LAUNCH_INFO, FL, "accepted master on socket %i position %i", s, temp );
    }

    aargs->master_connected->SignalCondition( MASTER_CONNECTED );

    aargs->master_connected->Unlock();

    return NULL;
}

void pack( void * & send_buf, int & send_buf_size, env_t *env, 
           dist_req_t distributions[], unsigned int nreq, unsigned int num_procs,
           string launch_topo, debug* dbg ){
    //compute size
    // env count
    // cstrings, name, value
    // proc count
    // proc_path
    // arg count
    // cstrings, argvs
    // launch_topo


    fprintf( stderr, "%s(%i) Packing \n", __FUNCTION__, __LINE__ );
    unsigned int i,sz,num;
    char** arg;

    env_t* env_cur = env;
    send_buf_size = sizeof( unsigned int );
    while( env_cur != NULL ){
        send_buf_size += strlen(env_cur->name)+1;
        send_buf_size += strlen(env_cur->value)+1;
        env_cur = env_cur->next;
    }
    fprintf(stderr, "%s(%i) env args size:%u\n", __FUNCTION__, __LINE__, send_buf_size);
    send_buf_size += sizeof( unsigned int );
    for( i = 0; i < nreq; i++ ){
        send_buf_size += strlen(distributions[i].proc_path)+1;

        send_buf_size += sizeof( unsigned int );
        if( distributions[i].proc_argv != NULL ){
            arg = distributions[i].proc_argv;
            while( arg != NULL && (*arg) != NULL ){
                send_buf_size += strlen( (*arg) )+1;
                arg++;
            }
        }
    }
    send_buf_size += sizeof( unsigned int );   //num procs
    fprintf(stderr, "%s(%i) base args size:%u\n", __FUNCTION__, __LINE__, send_buf_size);
    send_buf_size += sizeof( char );           //master or slave
    send_buf_size += strlen( launch_topo.c_str() )+1;     //launch topology
    fprintf(stderr, "%s(%i) total args size:%u\n", __FUNCTION__, __LINE__, send_buf_size);
    send_buf = malloc( send_buf_size );

    fprintf( stderr, "%s(%i) creating buffer of size %i \n", __FUNCTION__, __LINE__, send_buf_size );

    env_cur = env;
    char* cur = (char*)send_buf;
    char* count = cur;
    num = 0;
    fprintf(stderr, "%s(%i) cur:%u\n", __FUNCTION__, __LINE__, cur);
    cur += sizeof( unsigned int );
    while( env_cur != NULL ){
        num++;

    fprintf(stderr, "%s(%i) env name:%s size:%s\n", __FUNCTION__, __LINE__, env_cur->name, env_cur->value);
        sz = strlen(env_cur->name)+1;
        memcpy((void*)cur, (void*)env_cur->name, sz);
        cur += sz;

        sz = strlen(env_cur->value)+1;
        memcpy((void*)cur, (void*)env_cur->value, sz);
        cur += sz;

        env_cur = env_cur->next;
    }
    memcpy((void*)count, (void*)&num, sizeof( unsigned int ));
    fprintf(stderr, "%s(%i) cur:%u env-num:%u\n", __FUNCTION__, __LINE__, count, num);

    fprintf(stderr, "%s(%i) cur:%u nreq:%u\n", __FUNCTION__, __LINE__, cur, nreq);
    memcpy((void*)cur, (void*)&nreq, sizeof( unsigned int ));
    cur+=sizeof( unsigned int );
    for( i = 0; i < nreq; i++ ){
        sz = strlen(distributions[i].proc_path)+1;
        memcpy((void*)cur, (void*)distributions[i].proc_path, sz);
        cur += sz;

        count = cur;
        num = 0;
        cur += sizeof( unsigned int );
        if( distributions[i].proc_argv != NULL ){
            arg = distributions[i].proc_argv;
            while( arg !=  NULL && (*arg) != NULL ){
                num++;
                sz = strlen((*arg))+1;
                memcpy((void*)cur, (void*)(*arg), sz);
                cur += sz;
                arg++;
            }
        }
        fprintf(stderr, "%s(%i) cur:%u num args:%u\n", __FUNCTION__, __LINE__, count, nreq);
        memcpy((void*)count, (void*)&num, sizeof( unsigned int ));
    }
  fprintf(stderr, "%s(%i) cur:%u num procs:%u\n", __FUNCTION__, __LINE__, cur, nreq);
    memcpy((void*)cur, (void*)&num_procs, sizeof( unsigned int ));
    cur += sizeof(unsigned int);

    cur[0] = 'm';  //master proc
    cur++;

    sz = launch_topo.size()+1;
    memcpy((void*)cur, (void*)launch_topo.c_str(), sz);
    cur+=sz;
    dbg->print(FE_LAUNCH_INFO, FL, "done packing. begin:%u end:%u allocated:%u", send_buf, cur, send_buf_size);
}

