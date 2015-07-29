/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include "mpi.h"

#include "mrnet_lightweight/MRNet.h"
#include "header_lightweight.h"

char* connfile = NULL;

int getParentInfo(const char* file, int rank, char* phost, char* pport, char* prank)
{
    char* line;
    size_t len = 256;
    ssize_t linelen;
    FILE* ifs;

    char pname[64];
    int tpport, tprank, trank, matches;

    line = (char*) malloc(len);
    ifs = fopen(file, "r");
    if( NULL != ifs ) {
        do {
            memset( (void*)line, 0, len );
	    linelen = getline( &line, &len, ifs );
            if( linelen > 0 ) {

                matches = sscanf( line, "%s %d %d %d",
                                  pname, &tpport, &tprank, &trank );
                if( matches != 4 ) {
                    fprintf(stderr, "Error while scanning %s\n", file);
                    fclose(ifs);
                    return 1;
                }
                if( trank == rank ) {
                    sprintf(phost, "%s", pname);
                    sprintf(pport, "%d", tpport);
                    sprintf(prank, "%d", tprank);
                    fclose(ifs);
                    return 0;
                }
            }
        } while( linelen != -1 );
        fclose(ifs);
    }
    // my rank not found :(
    return 1;
}

typedef struct {
    int mpi_rank;
    int mrnet_rank;
    int argc;
    char** argv;
} info_t;

void* BackendThreadMain( void* arg )
{
    info_t* rinfo = (info_t*)arg;
    int32_t recv_int=0;
    int tag;   
    Packet_t* p;
    Stream_t* stream;
    Network_t * net = NULL;
    const char* fmt_str = "%d";

    char parHostname[64], parPort[10], parRank[10], myRank[10];
    Rank mRank = rinfo->mrnet_rank;
    sprintf(myRank, "%d", mRank);

    if( getParentInfo(connfile, rinfo->mpi_rank, parHostname, parPort, parRank) != 0 ) {
        fprintf(stderr, "Failed to parse connections file\n");
        return NULL;
    }

    assert( rinfo->argc == 6 );

    fprintf( stdout, "BE[%d] on %s connecting to %s:%s[%s]\n",
             mRank, rinfo->argv[4], parHostname, parPort, parRank );

    rinfo->argv[1] = parHostname;
    rinfo->argv[2] = parPort;
    rinfo->argv[3] = parRank;
    rinfo->argv[5] = myRank;
				
    net = Network_CreateNetworkBE( rinfo->argc, rinfo->argv );

    p = (Packet_t*) calloc( (size_t)1, sizeof(Packet_t) );

    do {
        if( Network_recv(net, &tag, p, &stream) != 1 ) {
            printf("net->recv() failure\n");
            tag = PROT_EXIT;
        }

        switch( tag ) {
        case PROT_INT:
            if( Packet_unpack( p, fmt_str, &recv_int) == -1 ) {
                printf("Packet_unpack() failure\n");
                return NULL;
            }

            fprintf(stdout, "BE[%d]: received int = %d\n", mRank, recv_int);

            if( (Stream_send(stream, PROT_INT, fmt_str, recv_int) == -1) ||
                (Stream_flush(stream) == -1 ) ) {
                printf("Stream_send() failure\n");
                return NULL;
            }
            break;
            
        case PROT_EXIT:
            fprintf(stdout, "BE[%d]: received PROT_EXIT\n", mRank);            
            break;

        default:
            fprintf(stdout, "BE[%d]: Unknown Protocol %d\n", mRank, tag);
            tag = PROT_EXIT;
            break;
        }

        fflush(stdout);
        fflush(stderr);

    } while ( tag != PROT_EXIT );    

    // FE delete of the network will cause us to exit, wait for it
    Network_waitfor_ShutDown(net);
    delete_Network_t(net);

    return NULL;
}

int main( int argc, char *argv[] )
{
    info_t rinfo;
    int BE_argc = 6;
    char* BE_argv[6];
    char myHostname[64];

    // Command line args are: connections_file
    int wSize, wRank;
    if( argc != 2 ) {
        fprintf(stderr, "ERROR: usage: %s connections_file\n", argv[0]);
        return -1;
    }
    
    connfile = argv[1];

    MPI_Init(&argc,&argv);

    /* determine my place in the world */
    wSize = wRank = -1;
    MPI_Comm_size( MPI_COMM_WORLD, &wSize );
    MPI_Comm_rank( MPI_COMM_WORLD, &wRank );


    /* Start MRNet threads */
    while( gethostname(myHostname, 64) == -1 ) {}
    myHostname[63] = '\0';

    BE_argv[0] = argv[0];
    BE_argv[4] = myHostname;

    rinfo.mpi_rank = wRank;
    rinfo.mrnet_rank = 100000 + wRank;
    rinfo.argc = BE_argc;
    rinfo.argv = BE_argv;
    BackendThreadMain( (void*)&rinfo );

    MPI_Finalize();

    return 0;
}

