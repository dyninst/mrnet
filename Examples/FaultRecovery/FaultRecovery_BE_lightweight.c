/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
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
    Stream_t * stream;
    Packet_t* p = (Packet_t*)malloc(sizeof(Packet_t));
    Network_t * net;
    assert(p);
    int rc, tag=0;
    unsigned int min_val=fr_range_max, max_val=0, num_iters=0;

    char pct[fr_bins];
    memset( (void*)pct, 0, (size_t)fr_bins );

    net = Network_CreateNetworkBE(argc, argv);
    assert(net);

    Rank me = Network_get_LocalRank(net);
    unsigned int seed = me;
    time_t now = time(NULL);
    seed += (seed * 1000) + (unsigned int)(now % 100);
    srandom( seed );

    do {
        
        rc = Network_recv(net,&tag, p, &stream);
        if( rc != 1 ) {
            if( rc == 0 ) continue; // no worries, a stream was just closed
            
            fprintf(stderr, "BE: stream::recv() failure\n");
            break;
        }

        switch(tag) {

        case PROT_START: {
            Packet_unpack(p, "%ud", &num_iters );

            // Send num_iters waves of integers
            unsigned int i;
            for( i=0; i < num_iters; i++ ) {

                long int randval = random();
                unsigned int val = (unsigned int)(randval % fr_range_max);
                if( val < min_val ) min_val = val;
                if( val > max_val ) max_val = val;
                double tile = floor( (double)val / (fr_range_max / fr_bins) );
                unsigned int ndx = (unsigned int) tile;
                assert( ndx < fr_bins );
                pct[ndx] = 1;

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
        }
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
            unsigned int u;
            uint64_t percent = 0;
            char bits[fr_bins+1];
            bits[fr_bins] = '\0';
            for( u = 0; u < fr_bins; u++ ) {
                percent += (uint64_t)(pct[u] << u);
                // need to match bitset printing, where max bin is on left
                if( pct[u] )
                    bits[(fr_bins-1)-u] = '1';
                else
                    bits[(fr_bins-1)-u] = '0';
            }
            fprintf( stdout, "BE %d: Sending pct (%s)\n", me, bits );
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

    if (p != NULL)
        free(p);

    // wait for final teardown packet from FE
    Network_waitfor_ShutDown(net);
    if ( net != NULL )
        delete_Network_t(net);

    return 0;
}
