/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "mrnet/MRNet.h"
#include "header.h"

using namespace MRN;

int main(int argc, char **argv) {

    // Command line args are: parent_hostname, parent_port, parent_rank, my_hostname, my_rank
    // these are used to instantiate the backend network object

    if( argc != 6 ) {
        fprintf(stderr, "Incorrect usage, must pass parent/local info\n");
        return -1;
    }

    const char* parHostname = argv[argc-5];
    Port parPort = (Port)strtoul( argv[argc-4], NULL, 10 );
    const char* myHostname = argv[argc-2];
    Rank myRank = (Rank)strtoul( argv[argc-1], NULL, 10 );
				
    fprintf( stdout, "Backend %s[%d] connecting to %s:%d\n",
             myHostname, myRank, parHostname, parPort );

    Network * net = Network::CreateNetworkBE( argc, argv );

    int32_t recv_int=0;
    int tag;   
    PacketPtr p;
    Stream* stream;

    do {
        if( net->recv(&tag, p, &stream) != 1 ) {
            fprintf(stderr, "BE: receive failure\n");
            break;
        }

        switch( tag ) {

        case PROT_INT:
            if( p->unpack( "%d", &recv_int) == -1 ) {
                fprintf(stderr, "BE: stream::unpack(%%d) failure\n");
                tag = PROT_EXIT;
                break;
            }

            fprintf(stdout, "BE: received int = %d\n", recv_int);

            if ( (stream->send(PROT_INT, "%d", recv_int) == -1) ||
                 (stream->flush() == -1 ) ) {
                fprintf(stderr, "BE: stream::send() failure\n");
                tag = PROT_EXIT;
            }
            break;

        case PROT_EXIT:
            break;

        default:
            fprintf(stderr, "BE: Unknown Protocol: %d\n", tag);
            tag = PROT_EXIT;
            break;
        }

        fflush(stdout);
        fflush(stderr);

    } while ( tag != PROT_EXIT );

#if 0 // TESTING detach before shutdown
    delete net;
#else    
    // wait for FE to delete the net
    net->waitfor_ShutDown();
    delete net;
#endif

    return 0;
}
