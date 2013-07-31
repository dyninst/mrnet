/****************************************************************************
 *  Copyright © 2011 The Regents of the University of New Mexico            *
 *  All Rights Reserved                                                     *
 *  Created by Josh Goehner and Dorian Arnold                               *
 *  Department of Computer Science                                          *
 *  Detailed usage rights in "LICENSE" file.                                *
 ****************************************************************************/

#include <stdio.h>
#include <cstdlib>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <string>
//#include "mrnet/MRNet.h"
//#include "lmon_api/lmon_fe.h"

using namespace std;
//using namespace MRN;

void quit_on_command(int a);
/* This is the decoy application for use with the LMON BE API launch method
 * of LIBI.  Launch this application to the designated distribution of nodes
 * then attach the proper libi application to these processes.
 */
int main(int argc, char** argv) {
    void * a;
    //request 10 gigs of memory
    for( int i = 0; i < 16; i++){
        fprintf(stderr, "%i\t", i);
        fflush(stderr);
        for(int j = 0; j < 1024; j++)
            a = malloc( 1024 * 1024 );
    }
    if(a==NULL)
        fprintf(stderr, "success\n");
//    timeval t1;
//    char h[50];
//    gethostname( h, 50 );
//    char* msg="bare bones";
//    char* exe="/g/g20/goehner1/decoy";
//    char* local="/tmp/decoy";
//
//    if( argc > 2 ){
//        int argsc = 5;
//        char** args = (char**)malloc(argsc*sizeof(char*));
//        //args[0]="/g/g20/goehner1/install/slurm/bin/srun";
//        //args[0]="srun";
//        args[0]=strdup(argv[1]);
//        args[1]=(char*)malloc(12 + strlen(argv[2]) );
//        sprintf(args[1], "--nodelist=%s",argv[2]);
//        args[2]="--distribution=arbitrary";
//        //args[3]="/g/g20/goehner1/mrnet/libi/decoy";
//        args[3]=strdup(local);
//        args[4]=NULL;
//
//
//        for(int i=0; i<argsc; i++)
//            fprintf(stderr, "%s\n", args[i] );
//
//        //mkdir("/tmp/jgoehner/", S_IRWXU );
//
//        char sbcast_cmd[500];
//        sprintf( sbcast_cmd, "/g/g20/goehner1/install/slurm/bin/sbcast %s %s", exe, local );
//
//        gettimeofday(&t1, NULL);
//        fprintf(stderr, "%ld.%.6ld: %s\n", t1.tv_sec, t1.tv_usec, sbcast_cmd );
//        system( sbcast_cmd );
//
//        gettimeofday(&t1, NULL);
//        fprintf(stderr, "%ld.%.6ld: %s launching %s\n", t1.tv_sec, t1.tv_usec, h, msg, argv[1] );
//        execvp ( args[0], args );
//    }
//    else{
//        //lmon_rc_e rc = LMON_fe_init(LMON_VERSION);
//
//        gettimeofday(&t1, NULL);
//        fprintf(stderr, "%ld.%.6ld: %s in main funciton\n", t1.tv_sec, t1.tv_usec, h );
//
//
//        //signal(SIGQUIT, quit_on_command);
//        //just wait
//        //while( true );
//    }
    return 0;
}

void quit_on_command(int a){
    // trapping the SIGQUIT command prevents srun
    // from throwing an abnormal exit error
    exit(0);
}

//void not_used(int argc, char** argv){
//
//    Stream * stream;
//    PacketPtr pkt;
//    int tag;
//    char recv_char='a';
//    int recv_len;
//    char * recv_string;
//    bool success=true;
//
//    //setenv("COBO_CLIENT_DEBUG", "9", 1);
//
//    Network * net = Network::CreateNetworkBE( argc, argv );
//
//}
