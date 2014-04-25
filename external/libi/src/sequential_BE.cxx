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

#include <string>
#include <sys/time.h>
#include "xplat/SocketUtils.h"
#include "xplat/NetUtils.h"

using namespace std;
FILE * fout;

/* socket_message_type is no longer valid as of 4.0.0
void socketmsgs( XPlat::socket_message_type type, char *msg, const char *file,
                    int line, const char * func){
    //fprintf( fout, "%s(%i) %s: %s\n", file, line, func, msg );
}
*/


int main(int argc, char** argv) {
    int pfd;
    char* phost;
    int pport;

    if(argc != 3){
        fprintf(stderr, "%s(%i) error: proper usage is sequential_BE hostname port\n", __FUNCTION__, __LINE__ );
        exit(1);
    }
    else{
        phost = argv[1];
        pport = atoi( argv[2] );
        //fprintf(fout, "%s(%i) connecting to: %s(%i) \n", __FUNCTION__, __LINE__, phost, pport );
    }

    pfd = -1;
    int cret = XPlat::SocketUtils::connectHost( &pfd, phost, pport, 10, socketmsgs );


    //simulated broadcast
    int msg_length = 128;
    void * msg = malloc( msg_length );
    char barrier_value = 'b';

    //fprintf(fout, "%s(%i) begining broadcast\n", __FUNCTION__, __LINE__ );
    int rc;

    rc = read( pfd, msg, msg_length );
    if( rc <= 0 ){
        fprintf(stderr, "%s(%i) error: could not read broadcast.\n\t%i %s\n", __FUNCTION__, __LINE__, rc, XPlat::Error::GetErrorString( XPlat::NetUtils::GetLastError() ).c_str() );
        exit(1);
    }

    //simulated barrier
    //fprintf(fout, "%s(%i) begining barrier\n", __FUNCTION__, __LINE__ );
    rc = write( pfd, &barrier_value, 1 );
    if( rc <= 0 ){
        fprintf(stderr, "%s(%i) error: could not write barrier.\n\t%i %s\n", __FUNCTION__, __LINE__, rc, XPlat::Error::GetErrorString( XPlat::NetUtils::GetLastError() ).c_str() );
        exit(1);
    }

    free( msg );
    msg_length = 128;
    msg = malloc( msg_length );

    //simulated scatter
    //fprintf(fout, "%s(%i) begining scatter\n", __FUNCTION__, __LINE__ );
    rc = read( pfd, msg, msg_length );
    if( rc <= 0 ){
        fprintf(stderr, "%s(%i) error: could not read scatter.\n\t%i %s\n", __FUNCTION__, __LINE__, rc, XPlat::Error::GetErrorString( XPlat::NetUtils::GetLastError() ).c_str() );
        exit(1);
    }

    //simulated gather
    //fprintf(fout, "%s(%i) begining gather\n", __FUNCTION__, __LINE__ );
    rc = write( pfd, msg, msg_length );
    if( rc <= 0 ){
        fprintf(stderr, "%s(%i) error: could not write barrier.\n\t%i %s\n", __FUNCTION__, __LINE__, rc, XPlat::Error::GetErrorString( XPlat::NetUtils::GetLastError() ).c_str() );
        exit(1);
    }

    //fprintf(fout, "%s(%i) done \n", __FUNCTION__, __LINE__ );

    return 0;
}
