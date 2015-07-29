/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include "mrnet_lightweight/MRNet.h"
#include "header_lightweight.h"

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
				
    fprintf( stderr, "Backend %s[%d] connecting to %s:%d\n",
             myHostname, myRank, parHostname, parPort );
    
    Packet_t* p = (Packet_t*) calloc( (size_t)1, sizeof(Packet_t) );

    Network_t* net = Network_CreateNetworkBE(argc, argv);
    assert(net);

    int32_t recv_int=0;
    int tag;   
    Stream_t* stream;
    const char* fmt_str = "%d";
    
    do {
        if( Network_recv(net, &tag, p, &stream) != 1 ) {
            fprintf(stderr, "BE: receive failure\n");
            break;
        }
   

        switch( tag ) {

        case PROT_INT: // tag 101
            if( Packet_unpack( p, fmt_str, &recv_int) == -1 ) {
                fprintf(stderr, "BE: stream::unpack(%%d) failure\n");
                tag = PROT_EXIT;
                break;
            }

            fprintf(stdout, "BE: received int = %d\n", recv_int);

            if ( (Stream_send(stream, PROT_INT, fmt_str, recv_int) == -1) ||
                 Stream_flush(stream) == -1 ) {
                fprintf(stderr, "BE: stream::send() failure\n");
                tag = PROT_EXIT;
            }
            break;

        case PROT_EXIT: // tag 100
            break;

        default:
            fprintf(stderr, "BE: Unknown Protocol: %d\n", tag);
            tag = PROT_EXIT;
            break;
        }

        fflush(stdout);
        fflush(stderr);

    } while ( tag != PROT_EXIT );

#if 0 // TESTING delete_Network_t as detach before shutdown
    delete_Network_t(net);
#else    
    // wait for final teardown packet from FE
    Network_waitfor_ShutDown(net);
    if (net != NULL )
        delete_Network_t(net);
#endif

    return 0;
}
