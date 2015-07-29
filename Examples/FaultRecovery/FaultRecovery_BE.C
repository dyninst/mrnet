/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "mrnet/MRNet.h"
#include "FaultRecovery.h"

#include <string>
#include <cstdlib>
#include <ctime>
#include <cmath>

using namespace std;
using namespace MRN;

int main(int argc, char **argv)
{
    Stream * stream = NULL;
    PacketPtr p;
    int tag=0;
    unsigned int min_val=fr_range_max, max_val=0, num_iters=0;

    fr_bin_set pct;

    Network * net = Network::CreateNetworkBE( argc, argv );

    Rank me = net->get_LocalRank();
    unsigned int seed = me;
    time_t now = time(NULL);
    seed += (seed * 1000) + (unsigned int)(now % 100);
    srandom( seed );

    do {
        int rc = net->recv(&tag, p, &stream);
        if( rc != 1 ) {
            if( rc == 0 ) continue; // no worries, a stream was just closed

            fprintf(stderr, "BE: Network::recv() failure\n");
            break;
        }

        switch(tag) {

        case PROT_START:
            p->unpack( "%ud", &num_iters );

            // Send num_iters waves of integers
            for( unsigned int i=0; i < num_iters; i++ ) {

                long int randval = random();
                unsigned int val = (unsigned int)(randval % fr_range_max);
                if( val < min_val ) min_val = val;
                if( val > max_val ) max_val = val;
                double tile = floor( (double)val / (fr_range_max / fr_bins) );
                unsigned int ndx = (unsigned int) tile;
                assert( ndx < fr_bins );
                pct.set(ndx);

                fprintf( stdout, "BE %d: Sending wave %u\n", me, i);
                fflush( stdout );

                if( stream->send(PROT_WAVE, "%ud", val) == -1 ) {
                    fprintf(stderr, "BE: stream::send(%%d) failure\n");
                    tag = PROT_EXIT;
                    break;
                }
                stream->flush();
                sleep(2); // stagger sends
            }
            break;

        case PROT_CHECK_MIN:
            fprintf( stdout, "BE %d: Sending min %u\n", me, min_val);
            if( stream->send(tag, "%ud", min_val) == -1 ) {
                fprintf(stderr, "BE: stream::send(%%d) failure\n");
                tag = PROT_EXIT;
                break;
            }
            stream->flush();
            break;

        case PROT_CHECK_MAX:
            fprintf( stdout, "BE %d: Sending max %u\n", me, max_val);
            if( stream->send(tag, "%ud", max_val) == -1 ) {
                fprintf(stderr, "BE: stream::send(%%d) failure\n");
                tag = PROT_EXIT;
                break;
            }
            stream->flush();
            break;

        case PROT_CHECK_PCT: {
#ifdef compiler_sun
            string bits = pct.to_string();
#else
            string bits = pct.to_string<char,char_traits<char>,allocator<char> >();
#endif
            unsigned long percent = pct.to_ulong();
            fprintf( stdout, "BE %d: Sending pct (%s)\n", me, bits.c_str() );
            if( stream->send(tag, "%uld", percent) == -1 ){
                fprintf(stderr, "BE: stream::send(%%d) failure\n");
                tag = PROT_EXIT;
                break;
            }
            stream->flush();
            break;
        }

        case PROT_EXIT:
	    if( stream->send(tag, "%d", 0) == -1 ){
                fprintf(stderr, "BE: stream::send(%%s) failure\n");
                break;
            }
            stream->flush();
            break;

        default:
            fprintf(stderr, "BE: Unknown Protocol: %d\n", tag);
            tag = PROT_EXIT;
            break;
        }

        fflush(stdout);
        fflush(stderr);

    } while ( tag != PROT_EXIT );

    // FE delete of the net will cause us to exit, wait for it
    net->waitfor_ShutDown();
    delete net;

    return 0;
}
