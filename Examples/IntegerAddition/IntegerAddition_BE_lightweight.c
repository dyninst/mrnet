/****************************************************************************
 * Copyright © 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/
#include <assert.h>
#include <unistd.h>
#include "mrnet/MRNet.h"
#include "IntegerAddition_lightweight.h"

int main(int argc, char **argv)
{
    Stream_t * stream=(Stream_t*)malloc(sizeof(Stream_t));
    assert(stream);
    Packet_t* p = (Packet_t*)malloc(sizeof(Packet_t));
    assert(p);
    int tag=0, recv_val=0, num_iters=0;
    
    Network_t * net = Network_CreateNetworkBE( argc, argv );
    assert(net);

    do {
        if( Network_recv(net, &tag, p, &stream) != 1 ) {
            fprintf(stderr, "BE: stream::recv() failure\n");
            break;
        }

        switch(tag) {

        case PROT_SUM:
            Packet_unpack(p, "%d %d", &recv_val, &num_iters );

            // Send num_iters waves of integers
            int i;
            for( i=0; i<num_iters; i++ ) {
                fprintf( stdout, "BE: Sending wave %u ...\n", i);
                if( Stream_send(stream,tag, "%d", recv_val*i) == -1 ){
                    fprintf(stderr, "BE: stream::send(%%d) failure\n");
                    tag = PROT_EXIT;
                    break;
                }
                if( Stream_flush(stream ) == -1 ){
                    fprintf(stderr, "BE: stream::flush() failure\n");
                    tag = PROT_EXIT;
                    break;
                }
                fflush(stdout);
                sleep(2); // stagger sends
            }
            break;

        case PROT_EXIT:
	    if( Stream_send(stream,tag, "%d", 0) == -1 ) {
                fprintf(stderr, "BE: stream::send(%%s) failure\n");
                break;
            }
            if( Stream_flush(stream ) == -1 ) {
                fprintf(stderr, "BE: stream::flush() failure\n");
            }
            break;

        default:
            fprintf(stderr, "BE: Unknown Protocol: %d\n", tag);
            tag = PROT_EXIT;
            break;
        }

        fflush(stderr);

    } while ( tag != PROT_EXIT );

    // wait for final teardown packet from FE; this will cause
    // us to exit
    Network_recv(net, &tag, p, &stream);
    
    return 0;
}
