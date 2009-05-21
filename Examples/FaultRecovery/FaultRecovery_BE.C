/****************************************************************************
 * Copyright © 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "mrnet/MRNet.h"
#include "FaultRecovery.h"
#include <bitset>
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
    unsigned int min_val=100, max_val=0, num_iters=0;

    std::bitset<10> pct;

    Network * net = new Network( argc, argv );

    Rank me = net->get_LocalRank();
    unsigned int seed = me, now = time(NULL);
    seed += (seed * 1000) + (now % 100);
    srandom( seed );

    do{
        if ( net->recv(&tag, p, &stream) != 1){
            fprintf(stderr, "stream::recv() failure\n");
            return -1;
        }

        switch(tag){
        case PROT_START:
            p->unpack( "%ud", &num_iters );

            // Send num_iters waves of integers
            for( unsigned int i=0; i < num_iters; i++ ){

                long int rand = random();
                unsigned int val = rand % 100;
                if( val < min_val ) min_val = val;
                if( val > max_val ) max_val = val;
                double tile = floor( (double)val / 10 );
                unsigned int ndx = (unsigned int) tile;
                assert( ndx < pct.size() );
                pct.set(ndx);

                fprintf( stdout, "BE %d: Sending wave %u\n", me, i);
                fflush( stdout );

                if( stream->send(PROT_WAVE, "%ud", val) == -1 ){
                    fprintf(stderr, "stream::send(%%d) failure\n");
                    return -1;
                }
                stream->flush();
                sleep(1); // stagger sends
            }
            break;

        case PROT_CHECK_MIN:

            fprintf( stdout, "BE %d: Sending min %u\n", me, min_val);
            fflush( stdout );

            if( stream->send(tag, "%ud", min_val) == -1 ){
                fprintf(stderr, "stream::send(%%d) failure\n");
                return -1;
            }
            stream->flush();
            break;

        case PROT_CHECK_MAX:

            fprintf( stdout, "BE %d: Sending max %u\n", me, max_val);
            fflush( stdout );

            if( stream->send(tag, "%ud", max_val) == -1 ){
                fprintf(stderr, "stream::send(%%d) failure\n");
                return -1;
            }
            stream->flush();
            break;

        case PROT_CHECK_PCT: {

            string bits = pct.to_string<char,char_traits<char>,allocator<char> >();
            unsigned long percent = pct.to_ulong();
            fprintf( stdout, "BE %d: Sending pct (%s)\n", me, bits.c_str() );
            fflush( stdout );

            if( stream->send(tag, "%uld", percent) == -1 ){
                fprintf(stderr, "stream::send(%%d) failure\n");
                return -1;
            }
            stream->flush();
            break;
        }

        case PROT_EXIT:
            fprintf( stdout, "BE %d: Processing PROT_EXIT\n", me);
            fflush( stdout );
	    if( stream->send(tag, "%d", 0) == -1 ){
                fprintf(stderr, "stream::send(%%s) failure\n");
                return -1;
            }
            stream->flush();
            break;

        default:
            fprintf(stderr, "Unknown Protocol: %d\n", tag);
            break;
        }
    } while ( tag != PROT_EXIT );

    sleep(10); // wait for network termination

    return 0;
}
