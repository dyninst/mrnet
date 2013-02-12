/****************************************************************************
 *  Copyright � 2011 The Regents of the University of New Mexico            *
 *  All Rights Reserved                                                     *
 *  Created by Josh Goehner and Dorian Arnold                               *
 *  Department of Computer Science                                          *
 *  Detailed usage rights in "LICENSE" file.                                *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

#include "xplat/SocketUtils.h"
#include "xplat/NetUtils.h"
#include "xplat/Monitor.h"
#include "xplat/Thread.h"
#include "xplat/Process.h"
#include <string>
#include <sys/time.h>
#include "utils.h"

using namespace std;

typedef struct _accept_args{
    int listener;
    int proc_count;
    XPlat::Monitor * all_connected;
    vector<int> * children;
} accept_args;

enum{ ALL_CONNECTED };

vector<string> createHostDistFake(int num);
vector<string> createHostDistSlurmBatch( int & limit );
void* accept_conn( void* args );
void sequential_launch( vector<string> & hosts, int listen_port, accept_args* aargs );
void sequential_broadcast( accept_args* aargs, void * msg, int msg_length );
void sequential_scatter( accept_args* aargs, void * msg, int per_length );
void sequential_barrier( fd_set waitingfds, int range, accept_args* aargs );
void sequential_gather( fd_set waitingfds, int range, accept_args* aargs, void * msg, int per_length );

/* socket_message_type is no longer valid as of 4.0.0
void socketmsgs( XPlat::socket_message_type type, char *msg, const char *file,
                    int line, const char * func){
    //fprintf( stderr, "%s(%i) %s: %s\n", file, line, func, msg );
}
*/

int main(int argc, char** argv) {
    vector<string> hosts;
    vector<int> fds;
    int proc_count=-1;
    timeval t1,t2,t3,t4,t5,t6;

    if(argc > 1)
        if(strcmp(argv[1],"-slurm") == 0){
            if(argc > 2)
                proc_count = atoi( argv[2] );

            hosts = createHostDistSlurmBatch( proc_count );
        }
        else{
            proc_count = atoi(argv[1]);
            fprintf( stderr, "%s(%i) localhost %i\n", __FUNCTION__, __LINE__, proc_count );
            hosts = createHostDistFake( proc_count );
        }
    else{
        proc_count=2;
        fprintf( stderr, "%s(%i) localhost 2\n", __FUNCTION__, __LINE__);
        hosts = createHostDistFake(2);
    }


    //bind to a listening port
    int listener;
    unsigned int listen_port = 0;
    bindPort( &listener, &listen_port, false, socketmsgs );

    XPlat::Monitor all_connected;
    all_connected.RegisterCondition( ALL_CONNECTED );

    accept_args aargs;
    aargs.listener = listener;
    aargs.proc_count = proc_count;
    aargs.all_connected = &all_connected;
    aargs.children = &fds;

    long int accept_thread_id;
    XPlat::Thread::Create( accept_conn, (void*)&aargs, &accept_thread_id );

    gettimeofday( &t1, NULL );
    sequential_launch( hosts, listen_port, &aargs );
    gettimeofday( &t2, NULL );
    
    //broadcast at the member level
    int msg_length = 128;
    void * msg = malloc( msg_length );


    fd_set readfds;
    int range=-1;
    FD_ZERO( &readfds );
    vector<int>::iterator iter = fds.begin();
    while( iter != fds.end() ){
        FD_SET( (*iter), &readfds );
        if( range == -1 || (*iter) > range)
            range=(*iter);
        iter++;
    }

    gettimeofday( &t3, NULL );
    sequential_broadcast( &aargs, msg, 128 );
    sequential_barrier( readfds, range, &aargs );
    gettimeofday( &t4, NULL );

    free( msg );
    msg_length = 128 * proc_count;
    msg = malloc( msg_length );

    //scatter at the member level
    gettimeofday( &t5, NULL );
    sequential_scatter( &aargs, msg, 128 );
    sequential_gather( readfds, range, &aargs, msg, 128 );
    gettimeofday( &t6, NULL );
    
    double launch = ((double)t2.tv_sec + ((double)t2.tv_usec * 0.000001)) - ((double)t1.tv_sec + ((double)t1.tv_usec * 0.000001));
    double broadcast = ((double)t4.tv_sec + ((double)t4.tv_usec * 0.000001)) - ((double)t3.tv_sec + ((double)t3.tv_usec * 0.000001));
    double scatter = ((double)t6.tv_sec + ((double)t6.tv_usec * 0.000001)) - ((double)t5.tv_sec + ((double)t5.tv_usec * 0.000001));

    printf("%i\t%f\t%f\t%f\n", proc_count, launch, broadcast, scatter);

    return 0;
}

vector<string> createHostDistSlurmBatch( int & limit ){

    string ns = getenv("SLURM_NODELIST");
    //string ns = "zeus[14-16]";
    vector<string> nodes;
    int f = ns.find('[');
    if(f<0){
        char* temp = (char*)malloc((ns.length()+1)*sizeof(char));
        temp = strdup(ns.c_str());
        nodes.push_back(ns);
    }
    else{
        //zeus[24-33,36-66,69-248,251-283]
        string base = ns.substr(0,f);
        string numbers = ns.substr(f+1, ns.length()-f-2); //extract numbers from base[numbers]
        string range;
        while(numbers.length()>0){
            f = numbers.find(',');
            if(f<0){
                range = numbers;
                numbers = "";
            }
            else{
                range = numbers.substr(0,f);
                numbers = numbers.substr(f+1);
            }

            int h = range.find('-');
            if(h<0){
                ostringstream node;
                node << base << range;
                char* temp = (char*)malloc((node.str().length()+1)*sizeof(char));
                nodes.push_back(node.str());
            }else{
                int begin = atoi(range.substr(0,h).c_str());
                int end = atoi(range.substr(h+1).c_str());
                for(int no = begin; no<=end; no++){
                    ostringstream node;
                    node << base << no;
                    char* temp = (char*)malloc((node.str().length()+1)*sizeof(char));
                    nodes.push_back(node.str());
                }
            }
        }
    }

    vector<string> ret;
    int node_count = nodes.size();
    int per = 1;
    int high_count=0;
    int count=0;
    int i,lim;
    if( limit >= node_count ){
        per = (int)floor( limit / node_count );
        high_count = limit % node_count;
    }
    vector<string>::iterator iter = nodes.begin();
    while(iter != nodes.end() && (count < limit || limit == -1) ){
        lim=per;
        if( high_count > 0 ){
            lim++;
            high_count--;
        }

        for( i=0; i<lim; i++ )
            ret.push_back( (*iter) );

        iter++;
        count++;
    }

    if( limit == -1 )
        limit = node_count;
    return ret;
}
vector<string> createHostDistFake(int num){
    vector<string> ret;
    string lh = "localhost";
    for( int i=0; i<num; i++ )
        ret.push_back( lh );

    return ret;
}


void* accept_conn( void* args ){
    accept_args* aargs = (accept_args*)args;
    int rc;

    aargs->all_connected->Lock();

    while( aargs->children->size() < aargs->proc_count ){
        rc = listen( aargs->listener, aargs->proc_count );
        if( rc != 0 )
            fprintf( stderr, "FE has problems listening: %s\n", XPlat::NetUtils::GetLastError() );
        else{
            struct sockaddr *addr;
            socklen_t *addrlen;
            int s = accept( aargs->listener, addr, addrlen );
            
            if( s < 0 )
                fprintf( stderr, "FE has problems accepting others: %s\n", XPlat::NetUtils::GetLastError() );
            else{
                aargs->children->push_back(s);
                //fprintf( stderr, "FE accepted: %i\n", aargs->children->size() );
            }
        }
    }
    
    aargs->all_connected->SignalCondition( ALL_CONNECTED );
    
    aargs->all_connected->Unlock();
    
    return NULL;
}

void sequential_launch( vector<string> & hosts, int listen_port, accept_args* aargs ){
    //fprintf(stderr, "%s(%i) start \n", __FUNCTION__, __LINE__ );
    
    int rc;
    vector<string> args;
    string cmd = "./sequential_BE";
    args.push_back( cmd );

    string hn;
    XPlat::NetUtils::GetLocalHostName( hn );
    args.push_back( hn );

    char* arg = (char*)malloc( 50 );
    snprintf( arg, 50, "%i", listen_port );
    args.push_back( string(arg) );

    char* rshcmd = getenv( "XPLAT_RSH" );
    if( rshcmd != NULL )
        XPlat::Process::set_RemoteShell( string(rshcmd) );

    vector<string>::iterator iter = hosts.begin();
    while( iter != hosts.end() ){
        rc = XPlat::Process::Create( (*iter), cmd, args );
        if( rc != 0 ){
            int err = XPlat::Process::GetLastError();
            fprintf( stderr, "failed to launch on %s: '%s'\n", (*iter).c_str(), strerror(err) );
        }
        iter++;
    }


    aargs->all_connected->Lock();
    while( aargs->children->size() < aargs->proc_count )
        aargs->all_connected->WaitOnCondition( ALL_CONNECTED );
    aargs->all_connected->Unlock();
    //fprintf(stderr, "%s(%i) stop\n", __FUNCTION__, __LINE__ );
}


void sequential_broadcast( accept_args* aargs, void * msg, int msg_length ){
    //fprintf(stderr, "%s(%i) start\n", __FUNCTION__, __LINE__ );
    int rc;
    vector<int>::iterator iter = aargs->children->begin();
    while( iter != aargs->children->end() ){
        rc = write( (*iter), msg, msg_length );
        iter++;
    }
    //fprintf(stderr, "%s(%i) stop\n", __FUNCTION__, __LINE__ );
}

void sequential_barrier( fd_set waitingfds, int range, accept_args* aargs ){
    //fprintf(stderr, "%s(%i) start\n", __FUNCTION__, __LINE__ );
    int count=0;
    int rc;
    int msg_length = 1;
    void * msg = malloc( msg_length );
    vector<int>::iterator iter;
    int num_waiting;
    fd_set readyfds;

    while( count < aargs->proc_count ){
        readyfds = waitingfds;
        num_waiting = select( range+1, &readyfds, NULL, NULL, NULL );  //blocking because no timeout was specified

        iter = aargs->children->begin();
        while( iter != aargs->children->end() ){
            if( FD_ISSET( (*iter), &readyfds ) ){
                rc = read( (*iter), msg, msg_length );
                count++;
            }
            iter++;
        }
    }
    //fprintf(stderr, "%s(%i) stop\n", __FUNCTION__, __LINE__ );
}

void sequential_scatter( accept_args* aargs, void * msg, int per_length ){
    //fprintf(stderr, "%s(%i) start\n", __FUNCTION__, __LINE__ );
    int rc;
    int i=0;
    char* per_msg = (char*)msg;

    vector<int>::iterator iter = aargs->children->begin();
    while( iter != aargs->children->end() ){
        rc = write( (*iter), (per_msg + i*per_length), per_length );
        i++;
        iter++;
    }
    //fprintf(stderr, "%s(%i) stop\n", __FUNCTION__, __LINE__ );
}


void sequential_gather( fd_set waitingfds, int range, accept_args* aargs, void * msg, int per_length ){
    //fprintf(stderr, "%s(%i) start\n", __FUNCTION__, __LINE__ );
    int count=0;
    int rc;
    vector<int>::iterator iter;
    char* per_msg = (char*)msg;
    int num_waiting;
    fd_set readyfds;

    while( count < aargs->proc_count ){
        readyfds = waitingfds;
        num_waiting = select( range+1, &readyfds, NULL, NULL, NULL );

        iter = aargs->children->begin();
        while( iter != aargs->children->end() ){
            if( FD_ISSET( (*iter), &readyfds ) ){
                //ignoring the order of the received data
                //normally this would be a problem, but since this is just for
                //timing purposes, order doesn't matter
                rc = read( (*iter), ( per_msg + (count*per_length) ), per_length );
                count++;
            }
            iter++;
        }
    }
    //fprintf(stderr, "%s(%i) stop\n", __FUNCTION__, __LINE__ );
}
