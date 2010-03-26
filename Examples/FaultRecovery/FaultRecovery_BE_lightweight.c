/****************************************************************************
 * Copyright © 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "mrnet_lightweight/MRNet.h"
#include "FaultRecovery_lightweight.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#ifndef os_windows
#include <unistd.h>
#endif

int main(int argc, char **argv)
{
    Stream_t * stream = (Stream_t*)malloc(sizeof(Stream_t));
    assert(stream);
    Packet_t* p = (Packet_t*)malloc(sizeof(Packet_t));
    assert(p);
    int tag=0;
    unsigned int min_val=100, max_val=0, num_iters=0;

    Network_t* net = Network_CreateNetworkBE(argc, argv);
    assert(net);

    Rank me = Network_get_LocalRank(net);
    unsigned int seed = me, now = time(NULL);
    seed += (seed * 1000) + (now % 100);
    srandom( seed );

    do{
        if ( Network_recv(net,&tag, p, &stream) != 1 ) {
            fprintf(stderr, "BE: stream::recv() failure\n");
            break;
        }

        switch(tag) {

        case PROT_START:
            Packet_unpack(p, "%ud", &num_iters );

            // Send num_iters waves of integers
            unsigned int i;
            for( i=0; i < num_iters; i++ ) {

                long int rand = random();
                unsigned int val = rand % 100;
                if( val < min_val ) min_val = val;
                if( val > max_val ) max_val = val;
                double tile = floor( (double)val / 10 );

                fprintf( stdout, "BE %d: Sending wave %u\n", me, i);
                fflush( stdout );

                if( Stream_send(stream,PROT_WAVE, "%ud", val) == -1 ) {
                    fprintf(stderr, "BE: stream::send(%%d) failure\n");
                    tag = PROT_EXIT;
                    break;
                }
                Stream_flush(stream);
                sleep(2); // stagger sends
            }
            break;

        case PROT_CHECK_MIN:
            fprintf( stdout, "BE %d: Sending min %u\n", me, min_val);
            if( Stream_send(stream,tag, "%ud", min_val) == -1 ) {
                fprintf(stderr, "BE: stream::send(%%d) failure\n");
                tag = PROT_EXIT;
                break;
            }
            Stream_flush(stream);
            break;

        case PROT_CHECK_MAX:
            fprintf( stdout, "BE %d: Sending max %u\n", me, max_val);
            if( Stream_send(stream,tag, "%ud", max_val) == -1 ) {
                fprintf(stderr, "BE: stream::send(%%d) failure\n");
                tag = PROT_EXIT;
                break;
            }
            Stream_flush(stream);
            break;

        case PROT_CHECK_PCT: {
            unsigned long percent = 1;
            if( Stream_send(stream,tag, "%uld", percent) == -1 ){
                fprintf(stderr, "BE: stream::send(%%d) failure\n");
                tag = PROT_EXIT;
                break;
            }
            Stream_flush(stream);
            break;
        }

        case PROT_EXIT:
	    if( Stream_send(stream,tag, "%d", 0) == -1 ){
                fprintf(stderr, "BE: stream::send(%%s) failure\n");
                break;
            }
            Stream_flush(stream);
            break;

        default:
            fprintf(stderr, "BE: Unknown Protocol: %d\n", tag);
            tag = PROT_EXIT;
            break;
        }

        fflush(stdout);
        fflush(stderr);

    } while ( tag != PROT_EXIT );

    // wait for final teardown packet from FE; this will cause us
    // to exit
    Network_recv(net, &tag, p, &stream);
        
    return 0;
}
