/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/
#include "mpi.h"


#include <iostream>
#include <fstream>

#include "mrnet/MRNet.h"
#include "xplat/Thread.h"
#include "header.h"

using namespace MRN;
using namespace std;

char* connfile = NULL;

int getParentInfo(const char* file, int rank, char* phost, char* pport, char* prank)
{
    ifstream ifs(file);
    if( ifs.is_open() ) {
        while( ifs.good() ) {
            char line[256];
	    ifs.getline( line, 256 );
            
            char pname[64];
            int tpport, tprank, trank;
            int matches = sscanf( line, "%s %d %d %d",
                                  pname, &tpport, &tprank, &trank );
            if( matches != 4 ) {
                fprintf(stderr, "Error while scanning %s\n", file);
                ifs.close();
	        return 1;
            }
            if( trank == rank ) {
                sprintf(phost, "%s", pname);
                sprintf(pport, "%d", tpport);
                sprintf(prank, "%d", tprank);
                ifs.close();
                return 0;
            }

        }
        ifs.close();
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

    char parHostname[64], parPort[10], parRank[10], myRank[10];
    Rank mRank = rinfo->mrnet_rank;
    sprintf(myRank, "%d", mRank);

    if( getParentInfo(connfile, rinfo->mpi_rank, parHostname, parPort, parRank) != 0 ) {
        fprintf(stderr, "Failed to parse connections file\n");
        return NULL;
    }

    int32_t recv_int=0;
    int tag;   
    PacketPtr p;
    Stream* stream;
    Network * net = NULL;

    assert( rinfo->argc == 6 );

    fprintf( stdout, "BE[%d] on %s connecting to %s:%s[%s]\n",
             mRank, rinfo->argv[4], parHostname, parPort, parRank );

    rinfo->argv[1] = parHostname;
    rinfo->argv[2] = parPort;
    rinfo->argv[3] = parRank;
    rinfo->argv[5] = myRank;
				
    net = Network::CreateNetworkBE( rinfo->argc, rinfo->argv );
    if( NULL == net ) {
         fprintf( stderr, "BE[%d] failed to attach to network\n", mRank );
    }
    else {
        do {
            if( net->recv(&tag, p, &stream) != 1 ) {
                fprintf( stderr, "BE[%d]: net->recv() failure\n", mRank );
                tag = PROT_EXIT;
            }

            switch( tag ) {
            case PROT_INT:
                if( p->unpack( "%d", &recv_int) == -1 ) {
                    fprintf( stderr, "BE[%d]: stream::unpack(%%d) failure\n", mRank );
                    return NULL;
                }

                fprintf( stdout, "BE[%d]: received int = %d\n", mRank, recv_int);

                if( (stream->send(PROT_INT, "%d", recv_int) == -1) ||
                    (stream->flush() == -1 ) ) {
                    fprintf( stderr, "BE[%d]: stream::send(%%d) failure\n", mRank );
                    return NULL;
                }
                break;
            
            case PROT_EXIT:            
                break;

            default:
                fprintf( stderr, "BE[%d]: Unknown Protocol %d\n", mRank, tag);
                tag = PROT_EXIT;
                break;
            }

            fflush(stdout);
            fflush(stderr);

        } while ( tag != PROT_EXIT );    

        // FE delete of the network will cause us to exit, wait for it
        net->waitfor_ShutDown();
        delete net;
    }

    free( arg );
    return NULL;
}

int main( int argc, char *argv[] ) {

    // Command line args are: connections_file [num_threads] 
    int nthreads = 1;
    int wSize, wRank;
    if( (argc < 2) || (argc > 3) ) {
        fprintf( stderr, "ERROR: usage: %s connections_file [#threads]\n", argv[0]);
        return -1;
    }
    
    connfile = argv[1];

    if( argc == 3 )
        nthreads = atoi( argv[2] ); 

    MPI_Init(&argc,&argv);

    /* determine my place in the world */
    wSize = wRank = -1;
    MPI_Comm_size( MPI_COMM_WORLD, &wSize );
    MPI_Comm_rank( MPI_COMM_WORLD, &wRank );


    /* Start MRNet threads */
    char myHostname[64];
    while( gethostname(myHostname, 64) == -1 ) {}
    myHostname[63] = '\0';

    int BE_argc = 6;
    char* BE_argv[BE_argc];
    BE_argv[0] = argv[0];
    BE_argv[4] = myHostname;

    std::vector< XPlat::Thread::Id > tids;
    for( int i=0; i < nthreads; i++ ) {
        info_t* rinfo = (info_t*) calloc(1, sizeof(info_t));
        rinfo->mpi_rank = wRank;
        rinfo->mrnet_rank = (10000*(i+1)) + wRank;
        rinfo->argc = BE_argc;
        rinfo->argv = BE_argv;

        XPlat::Thread::Id tid;
        XPlat::Thread::Create( BackendThreadMain, (void*)rinfo, &tid );
        tids.push_back( tid );
    }

    /* this is where MPI+work would normally be done */
    sleep(10);

    MPI_Finalize();


    /* Cleanup MRNet threads */
    for( int i=0; i < nthreads; i++ ) {
        XPlat::Thread::Id tid = tids[i];
        void* retval = NULL;
        XPlat::Thread::Join( tid, &retval );
    }

    return 0;
}

