/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "mrnet_lightweight/MRNet.h"
#include "PerfData_lightweight.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    Stream_t * stream;
    Packet_t* p = (Packet_t*)malloc(sizeof(Packet_t));
    Network_t* net;
    int i, tag=0, recv_val=0, num_iters=0;
    
    assert(p);

    net = Network_CreateNetworkBE( argc, argv );
    assert(net);

    do {
        if( Network_recv(net,&tag, p, &stream) != 1 ) {
            fprintf(stderr, "BE: stream::recv() failure\n");
            break;
        }

        switch(tag) {

        case PROT_SUM:
            Packet_unpack(p, "%d %d", &recv_val, &num_iters );

            // Send num_iters waves of integers
            for( i=0; i < num_iters; i++ ) {
                if( Stream_send(stream, tag, "%d", recv_val*i ) == -1 ) {
                    fprintf(stderr, "BE: stream::send(%%d) failure\n");
                    tag = PROT_EXIT;
                    break;
                }
                if( Stream_flush(stream ) == -1 ) {
                    fprintf(stderr, "BE: stream::flush() failure\n");
                    tag = PROT_EXIT;
                    break;
                }
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
    
    if (p != NULL)
        free(p);

    // wait for final teardown packet from FE
    Network_waitfor_ShutDown(net);
    if ( net != NULL )
        delete_Network_t(net);


    return 0;
}
