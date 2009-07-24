/****************************************************************************
 * Copyright © 2003-2008 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/
#include "mpi.h"


#include <iostream>
#include <fstream>

#include "mrnet/MRNet.h"
#include "header.h"

using namespace MRN;
using namespace std;

int getParentInfo(const char* file, int rank, 
                  char* phost, char* pport, char* prank);

int main( int argc, char *argv[] ) {

    // Command line args are: connections_file  

    int wSize, wRank;
    if( argc != 2 ) {
        fprintf(stderr, "Incorrect usage, must pass connections file\n");
        return -1;
    }
    const char* connfile = argv[1];

    MPI_Init(&argc,&argv);

    /* determine my place in the world */
    wSize = wRank = -1;
    MPI_Comm_size( MPI_COMM_WORLD, &wSize );
    MPI_Comm_rank( MPI_COMM_WORLD, &wRank );

    char parHostname[64], myHostname[64], parPort[10], parRank[10], myRank[10];
    Rank mRank = 10000 + wRank;
    sprintf(myRank, "%d", mRank);

    if( getParentInfo(connfile, wRank, parHostname, parPort, parRank) != 0 ) {
        fprintf(stderr, "Failed to parse connections file\n");
        return -1;
    }

    while( gethostname(myHostname, 64) == -1 ) {}
    myHostname[63] = '\0';

    int BE_argc = 6;
    char* BE_argv[6];
    BE_argv[0] = argv[0];
    BE_argv[1] = parHostname;
    BE_argv[2] = parPort;
    BE_argv[3] = parRank;
    BE_argv[4] = myHostname;
    BE_argv[5] = myRank;
				
    int32_t recv_int=0;
    int tag;   
    PacketPtr p;
    Stream* stream;
    Network * net = NULL;

    fprintf( stdout, "Backend %s[%s] connecting to %s:%s[%s]\n",
             myHostname, myRank, parHostname, parPort, parRank );

    net = Network::CreateNetworkBE( BE_argc, BE_argv );

    do {
        if( net->recv(&tag, p, &stream) != 1 ) {
            printf("receive failure\n");
            return -1;
        }

        switch( tag ) {
        case PROT_INT:
            if( p->unpack( "%d", &recv_int) == -1 ) {
                printf("stream::unpack(%%d) failure\n");
                return -1;
            }

            fprintf(stdout, "BE: received int = %d\n", recv_int);

            if( (stream->send(PROT_INT, "%d", recv_int) == -1) ||
                (stream->flush() == -1 ) ) {
                printf("stream::send(%%d) failure\n");
                return -1;
            }
            break;
            
        case PROT_EXIT:            
            break;

        default:
            fprintf(stdout, "BE: Unknown Protocol %d\n", tag);
            tag = PROT_EXIT;
            break;
        }

        fflush(stdout);
        fflush(stderr);

    } while ( tag != PROT_EXIT );

    MPI_Finalize();

    // FE delete of the network will causes us to exit, wait for it
    while(true) sleep(5);

    return 0;
}

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
