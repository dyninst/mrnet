/****************************************************************************
 *  Copyright © 2011 The Regents of the University of New Mexico            *
 *  All Rights Reserved                                                     *
 *  Created by Josh Goehner and Dorian Arnold                               *
 *  Department of Computer Science                                          *
 *  Detailed usage rights in "LICENSE" file.                                *
 ****************************************************************************/

#include <cassert>
#include <string.h>
#include <sys/stat.h>
#include "libi/ProcessGroupMember.h"
#include "libi/debug.h"
#include "xplat/Thread.h"
#include "xplat/Monitor.h"
#include "xplat/SocketUtils.h"
#include "xplat/Process.h"
#include "xplat/Error.h"
#include <unistd.h>

using namespace LIBI;
using namespace XPlat;
using namespace std;


typedef struct _PackBuf {
    int len;
    void* buf;
} PackBuf;

typedef struct _child_conncetion{
    char* hostname;
    int dist;
    int rank;
    int sock;
    int descendants;
    char* topo;
    int topo_length;
} child_connection;

typedef struct _respond_args{
    int listener;
    Monitor * all_ready;
    vector<child_connection*> * children;
    int current;
    void * args_buf;
    int args_length;
    int rank;
} respond_args;

typedef struct _dist_desc {
    string cmd;
    vector<string> args;
} dist_desc;

enum{ ALL_CHILDREN_READY };

bool parse_topology( char* topo, vector<child_connection*> & children, int & rank );
void launch_children( string phost, int pport, std::vector<child_connection*> & children, dist_desc distributions[] );
void unpack( void * recv_buf, int recv_buf_size, vector< pair<char*,char*> > & env,
           dist_desc * & distributions, unsigned int & nreq, unsigned int & args_size,
           bool & isMaster, unsigned int & num_procs, char * & launch_topo );
void* respond_with_topo( void* rargs );

static double * seq_time;
static double * individual_time;
static debug dbg(false);
static int first_local;
int libi_read( int fd, void* buf, int buf_size ){
    int rc=0;
    int read_size=0;
    char* cur = (char*)buf;

    while( read_size < buf_size ){
        errno=0;
        rc = read( fd, (void*)cur, buf_size - read_size );
        if( rc < 0 ){
            if( errno == EINTR || errno == EAGAIN ){
                dbg.print(INCLUDE_COMMUNICATION_INFO, FL, "error(%i): read interrupted: %s", errno, strerror(errno) );
                continue;
            } else if( errno == EFAULT )
                dbg.print(ERRORS, FL, "error(%i): bad recieve buf address: %s", errno, strerror(errno) );
            else if( errno == ECONNRESET )
                dbg.print(ERRORS, FL, "error(%i): Crash or disconnect or remote proc: %s", errno, strerror(errno) );
            else
                dbg.print(ERRORS, FL, "error(%i): problem reading: %s", errno, strerror(errno) );
            return -1;
        } else if ( rc ==0 ){
            dbg.print(ERRORS, FL, "error: unexpected return code of 0 while reading. Read %i of %i bytes.", read_size, buf_size );
            return -1;
        } else {
            read_size += rc;
            cur += rc;
        }
    }
    return rc;
}
int libi_write( int fd, void* buf, int buf_size ){
    int rc=0;
    int write_size=0;
    char* cur = (char*)buf;

    while( write_size < buf_size ){
        errno=0;
        rc = write( fd, (void*)cur, buf_size - write_size );
        if( rc < 0 ){
            if( errno == EINTR || errno == EAGAIN ){
                dbg.print(INCLUDE_COMMUNICATION_INFO, FL, "error(%i): write interrupted: %s", errno, strerror(errno) );
                continue;
            } else if( errno == EFAULT )
                dbg.print(ERRORS, FL, "error(%i): bad send buf address: %s", errno, strerror(errno) );
            else if( errno == ECONNRESET )
                dbg.print(ERRORS, FL, "error(%i): Crash or disconnect or remote proc: %s", errno, strerror(errno) );
            else
                dbg.print(ERRORS, FL, "error(%i): problem writing: %s", errno, strerror(errno) );
            return -1;
        } else if ( rc ==0 ){
            dbg.print(ERRORS, FL, "error: unexpected return code of 0 while writing.  Wrote %i of %i bytes.", write_size, buf_size );
            return -1;
        } else {
            write_size += rc;
            cur += rc;
        }
    }
    return rc;
}
ProcessGroupMember::ProcessGroupMember(){
    isInit=false;
    isMaster=false;
    numProcs=0;
    myRank=-1;
    parent_fd=-1;
    dbg.getParametersFromEnvironment();
}
ProcessGroupMember::~ProcessGroupMember(){
    finalize();
}

/* socket_message_type is no longer valid as of 4.0.0
void socketmsgs( XPlat::socket_message_type type, char *msg, const char *file,
                    int line, const char * func){
    
    int level=0;
    switch(type){
        case XPlat::ERROR_SOCKET_FAILURE:
        case XPlat::ERROR_CONNECT_FAILURE:
        case XPlat::ERROR_CONNECT_TIME_OUT:
        case XPlat::INFO_F_GETFD_FAILURE:
        case XPlat::INFO_F_SETFD_FAILURE:
        case XPlat::INFO_NODELAY_FAILURE:
        case XPlat::INFO_F_GETFL_FAILURE:
        case XPlat::INFO_F_SETFL_FAILURE:
        case XPlat::INFO_SETSOCKOPT_FAILURE:
        case XPlat::ERROR_BIND_FAILURE:
        case XPlat::ERROR_LISTEN_FAILURE:
        case XPlat::ERROR_GETPORTFROMSOCKET_FAILURE:
        case XPlat::INFO_ACCEPT_FAILURE:
        case XPlat::INFO_BLOCKING_FAILURE:
        case XPlat::ERROR_GETSOCKNAME_FAILURE:
            level=ERRORS;
            break;
        case XPlat::INFO_CONNECT_TIME_OUT:
            level=ALL_LAUNCH_INFO;
            break;
        case XPlat::INFO_CONNECT_START:
        case XPlat::INFO_CONNECT_FINISH:
        case XPlat::INFO_BIND_START:
        case XPlat::INFO_BIND_FINISH:
        case XPlat::INFO_GETSOCKETCONNECTION_START:
        case XPlat::INFO_GETSOCKETCONNECTION_FINISH:
        case XPlat::INFO_SOCKET_SUCCESS:
        case XPlat::INFO_BIND_NOBLOCK:
        case XPlat::INFO_ACCEPT_NOBLOCK:
            level=INCLUDE_COMMUNICATION_INFO;
            break;
        default:
            level=ERRORS;
            break;
    }

    if( type==ERROR_SOCKET_FAILURE )
        dbg.print(level, FL, "%s(%i) %s: %s\n", file, line, func, msg);
}
*/

libi_rc_e ProcessGroupMember::init(int *argc, char ***argv ){
    char** args = (*argv);
    char* phost;
    XPlat_Port pport;
    int pposition;
    int i;
    void* args_buf;
    unsigned int args_buf_size;
    int rc;

    if( isInit )
        return LIBI_EINIT;
    
    dbg.print(ALL_LAUNCH_INFO, FL, "starting");

    for( i=0 ; i < (*argc); i++ ){
        if( strncmp( args[i],"--libi-phost=", 13) == 0 ){
            phost = strdup( &args[i][13] );
        }
        else if( strncmp(args[i],"--libi-pport=", 13) == 0 ){
            pport = atoi( &args[i][13] );
        }
        else if( strncmp(args[i],"--libi-pposi=", 13) == 0 ){
            pposition = atoi( &args[i][13] );
        }
    }

    //connect
    dbg.print(ALL_LAUNCH_INFO, FL, "connecting to %s:%i position:%i ", phost, pport, pposition );
	// parent_fd is a member of the class libi/include/libi/ProcessGroupMember.h
    parent_fd = -1;
    //SocketUtils::setCallback(&socketmsgs);
    
	char* name = new char[256];
	if( false == SocketUtils::Connect( phost, pport, parent_fd, 10 ) ){
		gethostname(name, 256);
        dbg.print(ERRORS, FL, "libi: error: %s failed to connect to %s:%i position:%i.\n", name,  phost, pport, pposition );
        return LIBI_EINIT;
    }
	else{
		dbg.print(ERRORS, FL, "libi:%s connected to %s:%i position:%i.\n", name,  phost, pport, pposition );
	}
	delete [] name;
    //send our position to our parent
    rc = libi_write( parent_fd, &pposition, sizeof(int) );
    if( rc < 0 ){
        dbg.print(ALL_LAUNCH_INFO, FL, "error: could not send position to parent: %i", pposition );
        return LIBI_EINIT;
    }

    //recieve args
    args_buf_size=0;
    rc = libi_read( parent_fd, &args_buf_size, sizeof( unsigned int ) );
    if( rc < 0 ){
        dbg.print(ERRORS, FL, "error: could not receive arg size" );
        return LIBI_EINIT;
    }
//    fprintf(stderr, "%s(%i) read %i of %i bytes.  receive args of size:%u ", rc, sizeof(unsigned int), args_buf_size );
    args_buf=malloc(args_buf_size);
    rc = libi_read( parent_fd, args_buf, args_buf_size );
    if( rc < 0 ){
        dbg.print(ERRORS, FL, "error: could not receive args" );
        return LIBI_EINIT;
    }
    dbg.print(ALL_LAUNCH_INFO, FL, "Unpacking args of size:%u ", args_buf_size );

    timeval t1,t2;
    gettimeofday(&t1,NULL);

    vector< pair<char*,char*> > env;
    dist_desc *distributions;
    unsigned int nreq;
    char* topo;
    unsigned int args_size;
    unpack( args_buf, args_buf_size, env, distributions, nreq, args_size, isMaster, numProcs, topo );
    dbg.print(ALL_LAUNCH_INFO, FL, "env:%i nreq:%u isMaster:%i numProcs:%i topo:%s ", env.size(), nreq, isMaster, numProcs, topo );

    //set environment variables
    vector< pair<char*,char*> >::iterator env_iter = env.begin();
    while( env_iter != env.end() ){
        setenv( (*env_iter).first, (*env_iter).second, 1 );
        env_iter++;
    }
    dbg.getParametersFromEnvironment();

    //parse topology for children
//    fprintf(stderr, "%s(%i) parsing topo. " );
    std::vector<child_connection*> children;
    first_local = 0;
    if( !parse_topology( topo, children, myRank ) ){
        dbg.print(ERRORS, FL, "error: could not parse topology" );
        return LIBI_EINIT;
    }
    dbg.print(ALL_LAUNCH_INFO, FL, "%i children", children.size() );

    num_children = children.size();
    if( num_children > 0 ){
        seq_time = (double*)malloc( sizeof(double) * children.size() );
        individual_time = (double*)malloc( sizeof(double) * children.size() );
        //bind to a listening port
        XPlat_Socket listener=-1;
        XPlat_Port listen_port = 0;
        SocketUtils::CreateListening( listener, listen_port, false ); //non-blocking

        Monitor children_ready;
        children_ready.RegisterCondition( ALL_CHILDREN_READY );

        respond_args rargs;
        rargs.listener = listener;
        rargs.all_ready = &children_ready;
        rargs.children = &children;
        rargs.current = 0;
        rargs.args_buf = args_buf;
        rargs.args_length = args_size;
        rargs.rank = myRank;

        Thread::Id respond_thread_id;
		respond_thread_id = -1;
        //create a separate thread for sending out topology, and receiving word that children are ready
        Thread::Create( &respond_with_topo, (void*)&rargs, &respond_thread_id );

        gettimeofday(&t2,NULL);
        double d1 = ((double)t2.tv_sec + (double)t2.tv_usec / 1000000.0) - ((double)t1.tv_sec + (double)t1.tv_usec / 1000000.0);
        dbg.print(ALL_LAUNCH_INFO, FL, "parse args time:%f", d1 );

        //launch children
        string local_host;
        NetUtils::GetLocalHostName( local_host );
        launch_children( local_host, listen_port, children, distributions );

        //wait for all children to report ready
        dbg.print(ALL_LAUNCH_INFO, FL, "waiting for children" );
        children_ready.Lock();
        while( rargs.current < children.size() || rargs.current == -1 ){
            children_ready.WaitOnCondition( ALL_CHILDREN_READY );
        }
        children_ready.Unlock();

        if( rargs.current == -1 ){
            return LIBI_EINIT;
        }
        SocketUtils::Close(listener);

        dbg.print(ALL_LAUNCH_INFO, FL, "%i remote children", first_local );
        struct stat fstat;

        for(i=0; i<first_local; i++){
            stat( distributions[children[i]->dist].cmd.c_str(), &fstat );
            dbg.print(ALL_LAUNCH_INFO, FL, "remote launch time:%lf size:%i", individual_time[i], fstat.st_size );
        }
        for(i=first_local; i<num_children; i++){
            dbg.print(ALL_LAUNCH_INFO, FL, "colocated launch time:%lf", individual_time[i] );
        }
        for(i=1; i<num_children-1; i++){
            dbg.print(ALL_LAUNCH_INFO, FL, "sequential wait time:%lf", seq_time[i] );
        }
    }

    //save what we need, free everything else
    child_connection* child;
    subtree_size=1;
    if( num_children > 0 ){
        child_fds = (int*)malloc( num_children * sizeof(int) );
        child_rank = (int*)malloc( num_children * sizeof(int) );
        descendants = (int*)malloc( num_children * sizeof(int) );
        for(i=0; i<num_children; i++){
            //dbg.print(ALL_LAUNCH_INFO, FL, "saving/freeing child %i of %i ", i, num_children );
            //find lowest rank;
            std::vector<child_connection*>::iterator chiter = children.begin();
            std::vector<child_connection*>::iterator lowest = chiter;
            while( chiter != children.end() ){
                if( (*chiter)->rank < (*lowest)->rank)
                    lowest = chiter;
                chiter++;
            }
            child = (*lowest);
            children.erase( lowest );
            
            child_fds[i] = child->sock;
            child_rank[i] = child->rank;
            descendants[i] = child->descendants;
            subtree_size += child->descendants;
            dbg.print(ALL_LAUNCH_INFO, FL, "rank %i %s has %i descendants", child->rank, child->topo, descendants[i] );
            free( child );
        }
    }else{
        dbg.print(ALL_LAUNCH_INFO, FL, "no children" );
        child_fds = NULL;
        child_rank = NULL;
        descendants = NULL;
    }
    dbg.print(ALL_LAUNCH_INFO, FL, "waiting for all procs to finish" );
    delete[] distributions;
    free( args_buf );

    isInit=true;
    if( barrier() != LIBI_OK)
        return LIBI_EINIT;

    if(isMaster){
        //report to the front end that we are ready
        dbg.print(ALL_LAUNCH_INFO, FL, "reporting ready" );
        char ready='r';
        libi_write( parent_fd, &ready, sizeof(char) );
    }
    dbg.print(ALL_LAUNCH_INFO, FL, "init done" );
    return LIBI_OK;
}



libi_rc_e ProcessGroupMember::finalize(){
    if( !isInit )
        return LIBI_EINIT;

    if( parent_fd > 0 ){
        SocketUtils::Close( parent_fd );
        parent_fd = -1;
    }

    if( child_fds != NULL ){
        for( int i = 0; i<num_children; i++){
            if( child_fds[i] > 0 ){
                SocketUtils::Close( child_fds[i] );
                child_fds[i] = -1;
            }
        }
        delete[] child_fds;
    }

    if( descendants != NULL )
        delete[] descendants;
    
    isInit = false;

    return LIBI_OK;
}

libi_rc_e ProcessGroupMember::recvUsrData(void** msgbuf, int* msgbuflen){
    if( !isInit )
        return LIBI_EINIT;

    if( isMaster ){

        dbg.print(INCLUDE_COMMUNICATION_INFO, FL, "receiving", (*msgbuflen) );
        if( libi_read( parent_fd, msgbuflen, sizeof(int) ) <= 0 ){
            dbg.print(ERRORS, FL, "error: could not read msg size from FE." );
            return LIBI_EIO;
        }

        dbg.print(INCLUDE_COMMUNICATION_INFO, FL, "receiving size %i", (*msgbuflen) );
        if( (*msgbuflen) > 0 ){
            (*msgbuf) = malloc( (*msgbuflen) );
            if( libi_read( parent_fd, (*msgbuf), (*msgbuflen) ) <= 0 ){
                dbg.print(ERRORS, FL, "error: could not read msg from FE." );
                return LIBI_EIO;
            }
            dbg.print(INCLUDE_COMMUNICATION_INFO, FL, "received %i bytes", (*msgbuf) );
        }
    }

    return barrier();
}

libi_rc_e ProcessGroupMember::sendUsrData(void *msgbuf, int *msgbuflen){
    if( !isInit )
        return LIBI_EINIT;

    if( isMaster ){
        dbg.print(INCLUDE_COMMUNICATION_INFO, FL, "sending size %i", (*msgbuflen) );
        if( libi_write( parent_fd, msgbuflen, sizeof(int) ) <= 0 ){
            dbg.print(ERRORS, FL, "error: could not write msg size to FE." );
            return LIBI_EIO;
        }
        if( (*msgbuflen) > 0 ){
            if( libi_write( parent_fd, msgbuf, (*msgbuflen) ) <= 0 ){
                dbg.print(ERRORS, FL, "error: could not write msg to FE." );
                return LIBI_EIO;
            }
            dbg.print(INCLUDE_COMMUNICATION_INFO, FL, "sent %i bytes", (*msgbuflen) );
        }
    }

    return barrier();
}

libi_rc_e ProcessGroupMember::amIMaster(){
    if( !isInit )
        return LIBI_EINIT;

    if( isMaster )
        return LIBI_YES;
    else
        return LIBI_NO;
}

libi_rc_e ProcessGroupMember::getMyRank(int* rank){
    if( !isInit )
        return LIBI_EINIT;

    (*rank) = myRank;

    return LIBI_OK;
}

libi_rc_e ProcessGroupMember::getSize(int* size){
    if( !isInit )
        return LIBI_EINIT;

    (*size) = numProcs;

    return LIBI_OK;
}

libi_rc_e ProcessGroupMember::broadcast(void* buf,int numbyte){
    if( !isInit )
        return LIBI_EINIT;

    int rc;
    
    //get buf from parent
    if( !isMaster ){
        dbg.print(INCLUDE_COMMUNICATION_INFO, FL, "getting broadcast from parent");
        rc = libi_read( parent_fd, buf, numbyte );
        if( rc < 0 ){
            dbg.print(ERRORS, FL, "error: problem reading broadcast from parent." );
            return LIBI_EIO;
        }
    }
    dbg.print(INCLUDE_COMMUNICATION_INFO, FL, "broadcast received");
    
    //send buf to children
    for(int i=0; i<num_children; i++){
        dbg.print(INCLUDE_COMMUNICATION_INFO, FL, "sending broadcast to child %i", i);
        rc = libi_write( child_fds[i], buf, numbyte );
        if( rc < 0 ){
            dbg.print(ERRORS, FL, "error: problem sending broadcast to rank:%i", child_rank[i] );
            return LIBI_EIO;
        }
    }

    return LIBI_OK;
}

libi_rc_e ProcessGroupMember::gather(void *sendbuf, int numbyte_per_elem, void* recvbuf){
    if( !isInit )
        return LIBI_EINIT;

    int i;
    char* cur;
    int child_step;
    int buf_size = subtree_size * numbyte_per_elem;
    dbg.print(INCLUDE_COMMUNICATION_INFO, FL, "subtree_size:%i", subtree_size );
    char* buf = (char*)malloc( buf_size );

    //copy this sendbuf into first position
    memcpy( buf, sendbuf, numbyte_per_elem );

    //get sendbufs from children
    cur = buf;
    cur += numbyte_per_elem;
    for(i=0; i<num_children; i++){
        child_step = numbyte_per_elem * descendants[i];
        if( isMaster )
            dbg.print(INCLUDE_COMMUNICATION_INFO, FL, "gathering from child %i rank %i\n", i, child_rank[i] );
        if( libi_read( child_fds[i], cur, child_step ) < 0 ){
            dbg.print(ERRORS, FL, "error: problem gathering data from rank %i", child_rank[i] );
            return LIBI_EIO;
        }
        cur += child_step;
    }

    if( isMaster )
        memcpy( recvbuf, buf, buf_size );
    else {
        dbg.print(INCLUDE_COMMUNICATION_INFO, FL, "sending %i bytes to parent", buf_size );
        if( libi_write( parent_fd, buf, buf_size ) < 0 ){
            dbg.print(ERRORS, FL, "error: problem sending gathered data to parent" );
            return LIBI_EIO;
        }
    }
    free( buf );
    
    return LIBI_OK;
}

libi_rc_e ProcessGroupMember::scatter(void *sendbuf, int numbyte_per_element, void* recvbuf){
    if( !isInit )
        return LIBI_EINIT;

    int rc,i;
    char * buf;
    char * cur;
    int buf_size = subtree_size * numbyte_per_element;

    //get buf from parent
    if( isMaster ){
        buf = (char*)sendbuf;
    }else{
        buf = (char*)malloc( buf_size );
        dbg.print(INCLUDE_COMMUNICATION_INFO, FL, "receiving %i scattered bytes for myself and %i children", buf_size, num_children );
        if( libi_read( parent_fd, (void*)buf, buf_size ) < 0 ){
            dbg.print(ERRORS, FL, "error: problem reading scatter from parent" );
            return LIBI_EIO;
        }
        dbg.print(INCLUDE_COMMUNICATION_INFO, FL, "received scatter" );
    }

    //take ours
    memcpy( recvbuf, (void*)buf, numbyte_per_element );

    //send buf to children
    cur = buf;
    cur += numbyte_per_element;
    int child_step;
    dbg.print(INCLUDE_COMMUNICATION_INFO, FL, "scattering to %i children", num_children );
    for(i=0; i<num_children; i++){
        child_step = descendants[i] * numbyte_per_element;
        dbg.print(INCLUDE_COMMUNICATION_INFO, FL, "scattering to child[%i] size:%i", i, child_step );
        if( libi_write( child_fds[i], (void*)cur, child_step ) < 0 ){
            dbg.print(ERRORS, FL, "error: problem sending scattter to rank:%i", child_rank[i] );
            return LIBI_EIO;
        }
        cur += child_step;
    }
    if( isMaster )
        buf=NULL;
    else
        free(buf);
    
    return LIBI_OK;
}

libi_rc_e ProcessGroupMember::barrier(){
    if( !isInit )
        return LIBI_EINIT;

    dbg.print(INCLUDE_COMMUNICATION_INFO, FL, "starting barrier()" );

    char send = 'a';
    void* buf = NULL;
    libi_rc_e rc;
    if( isMaster )
        buf = malloc( subtree_size );

    rc = broadcast( (void*)&send, 1 );
    if( rc != LIBI_OK )
        return rc;

    rc = gather( (void*)&send, 1, buf );
    if( rc != LIBI_OK )
        return rc;

    dbg.print(INCLUDE_COMMUNICATION_INFO, FL, "ending barrier()" );

    return LIBI_OK;
}

/* Launch helper functions */


bool parse_topology( char* topo, vector<child_connection*> & children, int & rank )
{
    //topo format: localhost|dist1;rank1:rank2:rank3|dist2;rank4|{child1|...|{...},child2|...|)
    string top = string( topo );
    int local_pos = top.find( "{" );
    int child = local_pos + 1;
    int paren_left = 0;
    int end,pos,last,pipe,semi,dist,r;
    bool another_child=true;
    char* temp;
    child_connection * conn, * prev = NULL;
    vector<child_connection*> colocate;

    //current process
    last = pipe = top.find_first_of("|;:{},");
    if( pipe == string::npos || topo[pipe] != '|' ){
        dbg.print(ERRORS, FL, "error: invalid LIBI topology missing a '|' before character %c number %i: %s", topo[pipe], pipe, topo);
        return false;
    }
    semi = top.find_first_of("|;:{},", pipe+1);
    if( semi == string::npos || topo[semi] != ';' ){
        dbg.print(ERRORS, FL, "error: invalid LIBI topology missing a ':' before character %c number %i: %s", topo[semi], semi, topo);
        return false;
    }
    dist = atoi( top.substr( pipe+1, semi-pipe+1 ).c_str() );
    pos = top.find_first_of("|;:{},", semi+1);
    if( pos == string::npos || ( topo[pos] != ':' && topo[pos] != '|') ){
        dbg.print(ERRORS, FL, "error: invalid LIBI topology missing a '|' or ':' before character %c number %i: %s", topo[pos], pos, topo);
        return false;
    }
    rank = atoi( top.substr( semi+1, pos-semi+1 ).c_str() );

    char id[15];
    sprintf(id, "%i", rank);
    dbg.set_identifier( id );
    
    semi = pos;
    if( topo[pos] == '|' ){
        pipe = pos;
        semi = top.find_first_of("|;:{},", pipe+1);
        if( semi != string::npos & ( topo[semi] != ';' && topo[semi] != '{' ) ){
            dbg.print(ERRORS, FL, "error: invalid LIBI topology missing a ';' or '{' before character %c number %i: %s", topo[semi], semi, topo);
            return false;
        }
        if( topo[semi] == ';' ){
            dist = atoi( top.substr( pipe+1, semi-pipe+1 ).c_str() );
            pos = top.find_first_of("|;:{},", semi+1);
        } else {
            pos = semi;
        }
    }else
        pos = top.find_first_of("|;:{},", semi+1);

    //colocated processes
    while( pos != string::npos && ( topo[pos] == ':' || topo[pos] == '|' ) ){
        r = atoi( top.substr( semi+1, pos-semi+1 ).c_str() );
        conn = new child_connection();
        conn->hostname = strdup( top.substr( 0, last ).c_str() );
        conn->dist = dist;
        temp = (char*)malloc(200);
        sprintf(temp, "%s|%i;%i|",conn->hostname,dist,r);
        conn->topo = strdup(temp);
        conn->topo_length = strlen(conn->topo)+1;
        conn->rank = r;
//        conn->topo_length = strlen(temp)+1;
//        conn->topo = (char*)malloc( conn->topo_length );
//        memcpy( conn->topo, temp, conn->topo_length );  //ignore the trailing \0.  added during send
        conn->descendants = 1;
        conn->sock = -1;
        colocate.push_back( conn );

        dbg.print(ALL_LAUNCH_INFO, FL, "colocated host:%s dist:%i length:%i starting with:%s", conn->hostname, conn->dist, conn->topo_length, conn->topo);

        if( topo[pos] == '|' ){
            pipe = pos;
            semi = top.find_first_of("|;:{},", pipe+1);
            if( semi != string::npos & ( topo[semi] != ';' && topo[semi] != '{' ) ){
                dbg.print(ERRORS, FL, "error: invalid LIBI topology missing a ';' or '{' before character %c number %i: %s", topo[semi], semi, topo);
                return false;
            }
            if( topo[semi] == ';' ){
                dist = atoi( top.substr( pipe+1, semi-pipe+1 ).c_str() );
                pos = top.find_first_of("|;:{},", semi+1);
            } else {
                pos = semi;
            }
        } else { //:
            semi = pos;
            pos = top.find_first_of("|;:{},", semi+1);
            if( pos == string::npos || ( topo[pos] != ':' && topo[pos] != '|' ) ){
                dbg.print(ERRORS, FL, "error: invalid LIBI topology missing a ':' or '|' before character %c number %i: %s", topo[pos], pos, topo);
                return false;
            }
        }
    }

    if( pos == string::npos ){
        another_child=false;
    } else if( topo[pos] == '{' ){
        another_child=true;
        child=pos+1;
    } else {
        dbg.print(ERRORS, FL, "error: invalid LIBI topology missing a '{' after character number %i: %s", pos, topo);
        return false;
    }

    //child nodes
    while( another_child ){
        paren_left = 0;
        pos = child;
        conn = new child_connection();

        pipe = top.find_first_of("|;:{},", child); //exclude colocation & rank info
        if( pipe == string::npos || topo[pipe] != '|' ){
            dbg.print(ERRORS, FL, "error: invalid LIBI topology missing a '|' before character %c number %i: %s", topo[pipe], pipe, topo);
            return false;
        }
        semi = top.find_first_of("|;:{},", pipe+1);
        if( semi == string::npos || topo[semi] != ';' ){
            dbg.print(ERRORS, FL, "error: invalid LIBI topology missing a ';' before character number %i: %s", topo[semi], semi, topo);
            return false;
        }
        conn->hostname = strdup( top.substr( child, pipe-child ).c_str() );
        conn->dist = atoi( top.substr( pipe+1, semi-pipe+1 ).c_str() );

        pipe = top.find_first_of("|;:{},", semi+1);
        if( pipe == string::npos || ( topo[pipe] != ':' && topo[pipe] != '|') ){
            dbg.print(ERRORS, FL, "error: invalid LIBI topology missing a '|' or ':' before character %c number %i: %s", topo[pipe], pipe, topo);
            return false;
        }
        conn->rank = atoi( top.substr( semi+1, pipe-semi+1 ).c_str() );

        //find end of current child
        while( paren_left > 0 || pos == child ){
            pos = top.find_first_of("{},", pos+1);
            if( pos == string::npos ){
                another_child=false;
                break;
            } else if( topo[pos] == '{' )
                paren_left++;
            else if( topo[pos] == ',' )
                end = pos-1;
            else if( topo[pos] == '}' ){
                if( paren_left == 0){
                    //no more children
                    end = pos-1;
                    another_child=false;
                    break;
                }
                paren_left--;
                if( paren_left == 0 )
                    end = pos;
            }
        }

        if( pos != string::npos && pos <= top.size() ){
            conn->topo_length=end-child+2;
            conn->topo = (char*)malloc( conn->topo_length );
            memcpy(conn->topo, &topo[child], conn->topo_length-1);
            conn->topo[conn->topo_length-1]='\0';
            conn->sock = -1;
            dbg.print(ALL_LAUNCH_INFO, FL, "child host:%s dist:%i length:%i starting with:%s", conn->hostname, conn->dist, conn->topo_length, conn->topo);
            children.push_back( conn );

            //find last rank of this child
            pipe = top.find_last_of("|",end);
            if( pipe == string::npos){
                dbg.print(ERRORS, FL, "error: invalid LIBI topology missing a '|' before character %c number %i: %s", topo[end], end, topo);
                return false;
            }
            semi = top.find_last_of("|;:{},", pipe-1);
            if( semi == string::npos || ( topo[semi] != ';' && topo[semi] != ':') ){
                dbg.print(ERRORS, FL, "error: invalid LIBI topology missing a ';' or ':' after character %c number %i: %s", topo[semi], semi, topo);
                return false;
            }
            r = atoi( top.substr( semi+1, pos-semi+1 ).c_str() );

            conn->descendants = r - conn->rank + 1;
            dbg.print(ALL_LAUNCH_INFO, FL, "rank:%i has %i descendants (%i - %i +1) %s", conn->rank, conn->descendants, r, conn->rank, conn->topo);

            if( topo[pos] == '}' ){
                child = pos+1;
                if( topo[child] == ',' ){
                    child++;
                    another_child = true;
                } else
                    another_child = false;
            } else if( topo[pos] == ','){
                child = pos+1;
                another_child = true;
            } else {
                another_child = false;
            }
        } else {
            another_child = false;
            dbg.print(ERRORS, FL, "error: invalid topology specification: %s", topo);
            return false;
        }
    }
    first_local = children.size();
    if( colocate.size() > 0 )
        children.insert(children.end(), colocate.begin(), colocate.end() );

    return true;
}

void* respond_with_topo( void* rargs ){
    respond_args* details = (respond_args*)rargs;

    fd_set waiting_fds, ready_fds;
    int range;
    int num_waiting;
    int sock;

    FD_ZERO( &waiting_fds );
    FD_SET( details->listener, &waiting_fds );
    range = details->listener;

    vector<int> orphans;
    vector<int>::iterator o_iter;
    int rc;
    unsigned int position;
    char* msg = "a";

    while( details->current < details->children->size() ){
        dbg.print(ALL_LAUNCH_INFO, FL, "waiting for %i of %i", details->current, details->children->size() );
        
//        struct sockaddr *addr;
//        socklen_t *addrlen;
//        int e,s;
//        SocketUtils::getSocketConnection( details->listener, e );

        int s = accept( details->listener, NULL, NULL );  //blocking
        dbg.print(ALL_LAUNCH_INFO, FL, "accepting a connection" );
        if( s < 0 ){
            int ecode = NetUtils::GetLastError();
            if( ecode == EINTR )
                dbg.print(ALL_LAUNCH_INFO, FL, "accept interrupted, trying again.\n" );
            else
                dbg.print(ERRORS, FL, "error:%i problems accepting member: %s\n", ecode, Error::GetErrorString( ecode ).c_str() );
        }else{
            dbg.print(ALL_LAUNCH_INFO, FL, "accepted connection" );

            rc = libi_read( s, &position, sizeof( unsigned int ) );
            if( rc < 0)
                dbg.print(ERRORS, FL, "error: problems reading position." );
            else {
                dbg.print(ALL_LAUNCH_INFO, FL, "child at position %u connected", position );
                details->children->at(position)->sock = s;

                //send child specific args
                unsigned int length = details->args_length + details->children->at(position)->topo_length + 1;
                dbg.print(ALL_LAUNCH_INFO, FL, "sending buf of length:%u", length );

                if( libi_write( s, &length, sizeof( unsigned int ) ) < 0)
                    dbg.print(ERRORS, FL, "error: sending args length" );

                char* cur = (char*)details->args_buf;
                cur += details->args_length;
                cur[0] = 's';
                cur++;
                memcpy( cur, details->children->at(position)->topo, details->children->at(position)->topo_length );

                if ( libi_write( s, details->args_buf, length) < 0 )
                    dbg.print(ERRORS, FL, "error: sending args" );

                details->current++;
                dbg.print(ALL_LAUNCH_INFO, FL, "sent" );
                timeval t1;
                gettimeofday(&t1,NULL);
                individual_time[position] = ((double)t1.tv_sec + (double)t1.tv_usec / 1000000.0) - individual_time[position];
            }
        }
    }
    
    dbg.print(ALL_LAUNCH_INFO, FL, "done: %i of %i", details->current, details->children->size() );
    
    details->all_ready->Lock();
    details->all_ready->SignalCondition( ALL_CHILDREN_READY );
    details->all_ready->Unlock();

    return NULL;
}

void launch_children( string phost, int pport, std::vector<child_connection*> & children, dist_desc distributions[] ){
    std::vector<child_connection*>::const_iterator iter = children.begin();
    char temp[50];
    sprintf(temp, "--libi-pport=%i", pport);
    string port_arg = temp;
    string host_arg = "--libi-phost=" + phost;
    int posi=0;
    int rc;
    
    char* rshcmd     = getenv( "XPLAT_RSH" );
    char* rshcmdargs = getenv( "XPLAT_RSH_ARGS" );
    char* rshremcmd  = getenv( "XPLAT_REMCMD" );
    if( rshcmd != NULL )
        XPlat::Process::set_RemoteShell( string(rshcmd) );
    if( rshcmdargs != NULL )
        XPlat::Process::set_RemoteShellArgs( string(rshcmdargs) );
    if( rshremcmd != NULL )
        XPlat::Process::set_RemoteCommand( string(rshremcmd) );
    
    timeval t1,t2;
    while( iter != children.end() ){

        if( posi>0 ){
            gettimeofday(&t2,NULL);
            seq_time[posi] = ((double)t2.tv_sec + (double)t2.tv_usec / 1000000.0) - ((double)t1.tv_sec + (double)t1.tv_usec / 1000000.0);
        }
        gettimeofday(&t1,NULL);
        individual_time[posi]=((double)t1.tv_sec + (double)t1.tv_usec / 1000000.0);

        sprintf(temp, "--libi-pposi=%i", posi);
        vector<string> argv;
        int dist = (*iter)->dist;
        if( distributions[dist].args.size() == 0 || distributions[dist].cmd != distributions[dist].args[0] )
            argv.push_back( distributions[dist].cmd );
        argv.insert( argv.end(), distributions[dist].args.begin(), distributions[dist].args.end());
        argv.push_back( host_arg );
        argv.push_back( port_arg );
        argv.push_back( string(temp) );

        rc = Process::Create( string( (*iter)->hostname ), distributions[(*iter)->dist].cmd, argv );
        if( rc < 0 )
            dbg.print(ERRORS, FL, "error: could not create process on host:%s", (*iter)->hostname );
        posi++;
        iter++;
    }
}

void unpack( void * recv_buf, int recv_buf_size, vector< pair<char*,char*> > & env,
           dist_desc * & distributions, unsigned int & nreq, unsigned int & args_size,
           bool & isMaster, unsigned int & num_procs, char * & launch_topo ){
    char* cur = (char *)recv_buf;
    char* name;
    unsigned int i,j,num;

    args_size=0;
    num = *(unsigned int *)cur;
    args_size+=sizeof(unsigned int);
    cur+=sizeof(unsigned int);

    dbg.print(ALL_LAUNCH_INFO, FL, "args_size:%u env-num:%u", args_size, num);
    for( i=0; i < num; i++ ){
        name = cur;
        args_size+=strlen(cur)+1;
        cur+=strlen(cur)+1;

        dbg.print(ALL_LAUNCH_INFO, FL, "env name:%s val:%s", name, cur);
        env.push_back( pair<char*,char*>(name,cur) );
        args_size+=strlen(cur)+1;
        cur+=strlen(cur)+1;
    }
    dbg.print(ALL_LAUNCH_INFO, FL, "env args_size:%u", args_size);

    nreq = *(unsigned int *)cur;
    args_size+=sizeof(unsigned int);
    cur+=sizeof(unsigned int);
    dbg.print(ALL_LAUNCH_INFO, FL, "args_size%u nreq:%u", args_size, nreq );
    if( nreq > 0 ){
        distributions = new dist_desc[nreq];
    }else{
        distributions = NULL;
    }
    dbg.print(ALL_LAUNCH_INFO, FL, "args_size:%u", args_size );

    for( i = 0; i < nreq; i++ ){
        distributions[i].cmd = string(cur);
        args_size+=strlen(cur)+1;
        cur+=strlen(cur)+1;

        num = *(unsigned int *)cur;
        args_size+=sizeof(unsigned int);
        cur+=sizeof(unsigned int);
        dbg.print(ALL_LAUNCH_INFO, FL, "args_size:%u num_args:%u", args_size, num );
        if( num > 0 ){
            //distributions[i].args = new vector<string>;
            for( j=0; j<num; j++){
                distributions[i].args.push_back( string(cur) );
                args_size+=strlen(cur)+1;
                cur+=strlen(cur)+1;
            }
        }
    }
    num_procs = *(unsigned int*)cur;
    args_size += sizeof(unsigned int);
    cur += sizeof(unsigned int);

    dbg.print(ALL_LAUNCH_INFO, FL, "args_size:%u num_procs:%u isMaster:%c", args_size, num_procs, cur[0]);
    if( cur[0] == 'm' )
        isMaster=true;
    else
        isMaster=false;
    cur++;
//    fprintf(stderr, "%s(%i) args_size:%u isMaster:%i", args_size, isMaster);
//    fprintf(stderr, "%s(%i) launch_topo:%s", cur);


    launch_topo = cur;
    int unpacked = args_size + 1 + strlen(launch_topo)+1;
    if( unpacked != recv_buf_size )
        dbg.print(ERRORS, FL, "error: unpacked %i bytes.  Expected %i.", unpacked, recv_buf_size );
}
