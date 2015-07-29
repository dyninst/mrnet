/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "mrnet/MRNet.h"
#include "PerThreadStreams.h"
#include "xplat/Thread.h"
#include <string>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <cassert>

#ifndef os_windows
#include <unistd.h>
#endif

using namespace std;
using namespace MRN;

unsigned int min_val=fr_range_max, max_val=0, num_iters=0;
Rank me;
char pct[fr_bins];

// These three thread functions are to test the most common case of
// multi-threaded usage in MRNet: assigning a thread each to a different stream.
// The threads ping-pong with the front-end using blocking and non-blocking
// Stream_recv.
void* MaxBEMain(void *arg) {
    Stream *max_stream = (Stream *) arg;
    int tag;
    int i;
    int retval = 0;
    PacketPtr p;
    /* Test blocking recv exchange. For loop is if this test is desired to be
     * repeated.
     */
    for(i = 0; i < 1; i++) {
        if( max_stream->recv(&tag, p) == -1) {
            fprintf(stderr, "BE: stream::recv failure in MaxBEMain\n");
            return NULL;
        }
        if(tag != PROT_CHECK_MAX) {
            fprintf(stderr, "BE: MaxBEMain received the wrong packet! %d\n", tag);
            return NULL;
        }
        fprintf( stdout, "BE %d: Sending max %u\n", me, max_val);
        if( max_stream->send(tag, "%ud", max_val) == -1 ) {
            fprintf(stderr, "BE: stream::send(%%d) failure\n");
            return NULL;
        }
        if(max_stream->flush() == -1) {
            fprintf(stderr, "BE: Stream::flush() failure\n");
            return NULL;
        }
    }

    /* Test non-blocking exchange */
    while(retval <= 0) {
        retval = max_stream->recv(&tag, p, false);
        if(retval == -1) {
            fprintf(stderr, "BE: Stream::recv failure in MaxBEMain\n");
            return NULL;
        }
    }
    if(tag != PROT_CHECK_MAX) {
        fprintf(stderr, "BE: MaxBEMain received the wrong packet! %d\n", tag);
        return NULL;
    }
    fprintf( stdout, "BE %d: Sending max %u (%d)\n", me, max_val, i);
    if( max_stream->send(tag, "%ud", max_val) == -1 ) {
        fprintf(stderr, "BE: Stream::send(%%d) failure\n");
        return NULL;
    }
    if(max_stream->flush() == -1) {
        fprintf(stderr, "BE: Stream::flush() failure\n");
    }
    return NULL;
}

void* MinBEMain(void *arg) {
    Stream *min_stream = (Stream *) arg;
    int tag;
    int i;
    int retval = 0;
    PacketPtr p;
    for(i = 0; i < 1; i++) {
        if( min_stream->recv(&tag, p) == -1) {
            fprintf(stderr, "BE: stream::recv failure in MinBEMain\n");
            return NULL;
        }
        if(tag != PROT_CHECK_MIN) {
            fprintf(stderr, "BE: MinBEMain received the wrong packet! %d\n", tag);
            return NULL;
        }
        fprintf( stdout, "BE %d: Sending min %u\n", me, min_val);
        if( min_stream->send(tag, "%ud", min_val) == -1 ) {
            fprintf(stderr, "BE: stream::send(%%d) failure\n");
            tag = PROT_EXIT;
            return NULL;
        }
        if(min_stream->flush() == -1) {
            fprintf(stderr, "BE: Stream::flush() failure\n");
            return NULL;
        }
    }

    while(retval <= 0) {
        retval = min_stream->recv(&tag, p, false);
        if(retval == -1) {
            fprintf(stderr, "BE: Stream::recv failure in MinBEMain\n");
            return NULL;
        }
    }
    if(tag != PROT_CHECK_MIN) {
        fprintf(stderr, "BE: MinBEMain received the wrong packet! %d\n", tag);
        return NULL;
    }
    fprintf( stdout, "BE %d: Sending min %u (%d)\n", me, min_val, i);
    if( min_stream->send(tag, "%ud", min_val) == -1 ) {
        fprintf(stderr, "BE: Stream::send(%%d) failure\n");
        tag = PROT_EXIT;
        return NULL;
    }
    if(min_stream->flush() == -1) {
        fprintf(stderr, "BE: Stream::flush() failure\n");
    }

    return NULL;
}

void* PctBEMain(void *arg) {
    Stream *pct_stream = (Stream *) arg;
    int tag;
    int i;
    int retval = 0;
    uint64_t percent = 0;
    char bits[fr_bins+1];
    PacketPtr p;
    for(i = 0; i < 1; i++) {
        if( pct_stream->recv(&tag, p) == -1) {
            fprintf(stderr, "BE: stream::recv failure in PctBEMain\n");
            return NULL;
        }
        if(tag != PROT_CHECK_PCT) {
            fprintf(stderr, "BE: PctBEMain received the wrong packet! %d\n", tag);
            return NULL;
        }
        unsigned int u;
        bits[fr_bins] = '\0';
        for( u = 0; u < fr_bins; u++ ) {
            percent += (pct[u] << u);
            // need to match bitset printing, where max bin is on left
            if( pct[u] )
                bits[(fr_bins-1)-u] = '1';
            else
                bits[(fr_bins-1)-u] = '0';
        }
        fprintf( stdout, "BE %d: Sending pct (%s)\n", me, bits );
        if( pct_stream->send(tag, "%uld", percent) == -1 ){
            fprintf(stderr, "BE: stream::send(%%d) failure\n");
            tag = PROT_EXIT;
            return NULL;
        }
        if(pct_stream->flush() == -1) {
            fprintf(stderr, "BE: Stream::flush() failure\n");
            return NULL;
        }
    }
    
    while(retval <= 0) {
        retval = pct_stream->recv(&tag, p, false);
        if( retval == -1) {
            fprintf(stderr, "BE: Stream::recv failure in PctBEMain\n");
            return NULL;
        }
    }
    if(tag != PROT_CHECK_PCT) {
        fprintf(stderr, "BE: PctBEMain received the wrong packet! %d\n", tag);
        return NULL;
    }
    fprintf( stdout, "BE %d: Sending pct %s (%d)\n", me, bits, i);
    if( pct_stream->send(tag, "%uld", percent) == -1 ){
        fprintf(stderr, "BE: Stream::send(%%d) failure\n");
        tag = PROT_EXIT;
        return NULL;
    }
    if(pct_stream->flush() == -1) {
        fprintf(stderr, "BE: Stream::flush() failure\n");
    }

    return NULL;
}

int main(int argc, char **argv)
{
    Stream * stream = NULL, *max_stream = NULL;
    Stream *min_stream = NULL, *pct_stream = NULL;
    PacketPtr p;
    void *min_ret;
    void *max_ret;
    void *pct_ret;
    int tag=0;
    int ret_val;
    int regd_streams = 0;

    memset( (void*)pct, 0, (size_t)fr_bins );

    Network * net = Network::CreateNetworkBE( argc, argv );
    assert(net);

    me = net->get_LocalRank();
    unsigned int seed = me;
    time_t now = time(NULL);
    seed += (seed * 1000) + (unsigned int)(now % 100);
    srandom( seed );

    /* Receive PROT_START and establish values for test */
    if ( net->recv(&tag, p, &stream) != 1 ) {
        fprintf(stderr, "BE: stream::recv() failure\n");
        return 1;
    }

    if(tag == PROT_START) {
        p->unpack("%ud", &num_iters );

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

            if( stream->send(PROT_WAVE, "%ud", val) == -1 ) {
                fprintf(stderr, "BE: stream::send(%%d) failure\n");
                tag = PROT_EXIT;
                break;
            }
            stream->flush();
            sleep(2); // stagger sends
        }
    } else {
        fprintf(stderr, "tag: %d\n", tag);
        fprintf(stderr, "Did not receive PROT_START to start. Exiting.\n");
        return 1;
    }

    // Register streams for threads

    // Register max_stream
    do {
        if(net->recv(&tag, p, &stream) == -1) {
            fprintf(stderr, "BE: net::recv() failure of stream\n");
            return 1;
        }

        switch(tag) {

        case PROT_REG_MAX: {
            max_stream = stream;
            regd_streams++;
            break;
        }

        case PROT_REG_MIN: {
            min_stream = stream;
            regd_streams++;
            break;
        }

        case PROT_REG_PCT: {
            pct_stream = stream;
            regd_streams++;
            break;
        }

        default: {
            fprintf(stderr, "BE: Reg. stream loop received incorrect tag.\n");
            break;
        }

        }

        if(regd_streams == 3) {
            if(max_stream == NULL || min_stream == NULL || pct_stream == NULL) {
                regd_streams--;
                fprintf(stderr, "BE: Received same stream twice: %d\n", tag);
            }
        }
    } while(regd_streams < 3);

    // Send acknowledgment that our streams are registered.
    if(max_stream->send(PROT_REG_STRM, "%d", 0) == -1) {
        fprintf(stderr, "BE: Could not send stream reg. acknowledgment!\n");
        return 1;
    }

    // Test simultaneous send/recv on multiple threads. FE performs sanity chk
    XPlat::Thread::Id min_BE;
    XPlat::Thread::Id max_BE;
    XPlat::Thread::Id pct_BE;

    XPlat::Thread::Create(MinBEMain, (void *)min_stream, &min_BE);
    XPlat::Thread::Create(MaxBEMain, (void *)max_stream, &max_BE);
    XPlat::Thread::Create(PctBEMain, (void *)pct_stream, &pct_BE);
    
    // Join worker threads
    XPlat::Thread::Join(min_BE, &min_ret);
    XPlat::Thread::Join(max_BE, &max_ret);
    XPlat::Thread::Join(pct_BE, &pct_ret);

    // Receive PROT_EXIT
    ret_val = net->recv(&tag, p, &stream);
    while(ret_val != 1) {
        if(ret_val == -1) {
            fprintf(stderr, "BE: net::recv() failure of PROT_EXIT.\n");
            return 1;
        } else if(ret_val == 0) {
            //fprintf(stderr, "Detected stream closure.\n");
        }

        ret_val = net->recv(&tag, p, &stream);
    }

    if(tag != PROT_EXIT) {
        fprintf(stderr, "BE: Received %d. Expected PROT_EXIT.\n", tag);
        return 1;
    }

    // Send ack on the received stream
    // ***If I don't send anything, FE doesn't get message...
    if(stream->send(PROT_EXIT, "%d", 0) == -1) {
        fprintf(stderr, "BE: Stream::send PROT_EXIT ack failure\n");
        return 1;
    }

    // wait for final teardown packet from FE
    net->waitfor_ShutDown();
    delete net;

    return 0;
}
