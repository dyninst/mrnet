/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "mrnet_lightweight/MRNet.h"
#include "PerThreadStreams_lightweight.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#ifndef os_windows
#include <unistd.h>
#endif

// These variables made global to allow access for multiple threads
unsigned int min_val=fr_range_max, max_val=0, num_iters=0;
Rank me;
char pct[fr_bins];
uint64_t pct_ul;

// These three thread functions are to test the most common case of
// multi-threaded usage in MRNet: assigning a thread each to a different stream.
// The threads ping-pong with the front-end using blocking and non-blocking
// Stream_recv.
void* MaxBEMain(void *arg) {
    Stream_t *max_stream = (Stream_t *) arg;
    int i;
    int tag;
    int retval = 0;
    Packet_t *p = (Packet_t*)malloc(sizeof(Packet_t));
	assert(p);

    printf("BE %d starting MaxBEMain: %d\n", (int)me, (unsigned int)Thread_GetId());

    /* Test blocking recv exchange. For loop is if this test is desired to be
     * repeated.
     */
    for(i = 0; i < 1; i++) {
        if(Stream_recv(max_stream, &tag, p, 1) == -1) {
            fprintf(stderr, "BE: Stream_recv failure in MaxBEMain\n");
            return NULL;
        }
        if(tag != PROT_CHECK_MAX) {
            fprintf(stderr, "BE: MaxBEMain received the wrong packet! %d\n", tag);
            return NULL;
        }
        fprintf( stdout, "BE %d: Sending max %u (%d)\n", me, max_val, i);
        if( Stream_send(max_stream, tag, "%ud", max_val) == -1 ) {
            fprintf(stderr, "BE: Stream_send(%%d) failure\n");
            return NULL;
        }
        Stream_flush(max_stream);
        tag = 0;
    }
    
    /* Test non-blocking exchange */
    while(retval <= 0) {
        retval = Stream_recv(max_stream, &tag, p, 0);
        if(retval == -1) {
            fprintf(stderr, "BE: Stream_recv failure in MaxBEMain\n");
            return NULL;
        }
    }
    if(tag != PROT_CHECK_MAX) {
        fprintf(stderr, "BE: MaxBEMain received the wrong packet! %d\n", tag);
        return NULL;
    }
    fprintf( stdout, "BE %d: Sending max %u (%d)\n", me, max_val, i);
    if( Stream_send(max_stream, tag, "%ud", max_val) == -1 ) {
        fprintf(stderr, "BE: Stream_send(%%d) failure\n");
        return NULL;
    }
    if(Stream_flush(max_stream) == -1) {
        fprintf(stderr, "BE: Stream_flush() failure\n");
    }

    return NULL;
}

void* MinBEMain(void *arg) {
    Stream_t *min_stream = (Stream_t *) arg;
    int tag;
    int i;
    int retval = 0;
    Packet_t *p = (Packet_t*)malloc(sizeof(Packet_t));
	assert(p);
    printf("BE %d starting MinBEMain: %d\n", (int)me, (unsigned int)Thread_GetId());

    for(i = 0; i < 1; i++) {
        if(Stream_recv(min_stream, &tag, p, 1) == -1) {
            fprintf(stderr, "BE: Stream_recv failure in MinBEMain\n");
            return NULL;
        }
        if(tag != PROT_CHECK_MIN) {
            fprintf(stderr, "BE: MinBEMain received the wrong packet! %d\n", tag);
            return NULL;
        }
        fprintf( stdout, "BE %d: Sending min %u (%d)\n", me, min_val, i);
        if( Stream_send(min_stream, tag, "%ud", min_val) == -1 ) {
            fprintf(stderr, "BE: Stream_send(%%d) failure\n");
            tag = PROT_EXIT;
            return NULL;
        }
        if(Stream_flush(min_stream) == -1) {
            fprintf(stderr, "BE: Stream_flush() failure\n");
            return NULL;
        }
    }

    while(retval <= 0) {
        retval = Stream_recv(min_stream, &tag, p, 0);
        if(retval == -1) {
            fprintf(stderr, "BE: Stream_recv failure in MinBEMain\n");
            return NULL;
        }
    }
    if(tag != PROT_CHECK_MIN) {
        fprintf(stderr, "BE: MinBEMain received the wrong packet! %d\n", tag);
        return NULL;
    }
    fprintf( stdout, "BE %d: Sending min %u (%d)\n", me, min_val, i);
    if( Stream_send(min_stream, tag, "%ud", min_val) == -1 ) {
        fprintf(stderr, "BE: Stream_send(%%d) failure\n");
        tag = PROT_EXIT;
        return NULL;
    }
    if(Stream_flush(min_stream) == -1) {
        fprintf(stderr, "BE: Stream_flush() failure\n");
    }

    return NULL;
}

void* PctBEMain(void *arg) {

    Stream_t *pct_stream = (Stream_t *) arg;
    int tag;
    int retval = 0;
	Packet_t *p = (Packet_t*)malloc(sizeof(Packet_t));
    unsigned int u;
    char bits[fr_bins+1];
    int i;

    assert(p);
    printf("BE %d starting PctBEMain: %d\n", (int)me, (unsigned int)Thread_GetId());

    for(i = 0; i < 1; i++) {
        if( Stream_recv(pct_stream, &tag, p, 1) == -1) {
            fprintf(stderr, "BE: Stream_recv failure in PctBEMain\n");
            return NULL;
        }
        if(tag != PROT_CHECK_PCT) {
            fprintf(stderr, "BE: PctBEMain received the wrong packet! %d\n", tag);
            return NULL;
        }
        bits[fr_bins] = '\0';
        for( u = 0; u < fr_bins; u++ ) {
            // need to match bitset printing, where max bin is on left
            if( pct[u] )
                bits[(fr_bins-1)-u] = '1';
            else
                bits[(fr_bins-1)-u] = '0';
        }
        fprintf( stdout, "BE %d: Sending pct %s (%d)\n", me, bits, i);
        if( Stream_send(pct_stream, tag, "%uld", pct_ul) == -1 ){
            fprintf(stderr, "BE: Stream_send(%%d) failure\n");
            tag = PROT_EXIT;
            return NULL;
        }
        Stream_flush(pct_stream);
    }
    
    // Test nonblocking recvs
    while(retval <= 0) {
        retval = Stream_recv(pct_stream, &tag, p, 0);
        if( retval == -1) {
            fprintf(stderr, "BE: Stream_recv failure in PctBEMain\n");
            return NULL;
        }
    }
    if(tag != PROT_CHECK_PCT) {
        fprintf(stderr, "BE: PctBEMain received the wrong packet! %d\n", tag);
        return NULL;
    }
    fprintf( stdout, "BE %d: Sending pct %s (%d)\n", me, bits, i);
    if( Stream_send(pct_stream, tag, "%uld", pct_ul) == -1 ){
        fprintf(stderr, "BE: Stream_send(%%d) failure\n");
        tag = PROT_EXIT;
        return NULL;
    }
    if(Stream_flush(pct_stream) == -1) {
        fprintf(stderr, "BE: Stream_flush() failure\n");
    }

    return NULL;
}

int main(int argc, char **argv)
{
    Stream_t *stream;
    Stream_t *max_stream = NULL, *min_stream = NULL, *pct_stream = NULL;
    Packet_t *p = (Packet_t*)malloc(sizeof(Packet_t));
    Network_t * net;
    unsigned int j;
    int tag=0;
	int ret_val;
    int regd_streams = 0;
    unsigned int seed;
    time_t now;
    Thread_Id min_BE;
    Thread_Id max_BE;
    Thread_Id pct_BE;
    void *min_ret;
    void *max_ret;
    void *pct_ret;

    assert(p);

    memset( (void*)pct, 0, (size_t)fr_bins );

    net = Network_CreateNetworkBE(argc, argv);
    assert(net);

    me = Network_get_LocalRank(net);
    seed = me;
    now = time(NULL);
    seed += (seed * 1000) + (unsigned int)(now % 100);
    srandom( seed );

    /* Receive PROT_START and establish values for test */
    if ( Network_recv(net,&tag, p, &stream) != 1 ) {
        fprintf(stderr, "BE: stream::recv() failure\n");
        return 1;
    }

    if(tag == PROT_START) {
        unsigned int i, val, ndx;
        long int randval;
        double tile;
        Packet_unpack(p, "%ud", &num_iters );

        /* Send num_iters waves of integers */
        for( i=0; i < num_iters; i++ ) {

            randval = random();
            val = (unsigned int)(randval % fr_range_max);
            if( val < min_val ) min_val = val;
            if( val > max_val ) max_val = val;
            tile = floor( (double)val / (fr_range_max / fr_bins) );
            ndx = (unsigned int) tile;
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
    } else {
        fprintf(stderr, "Did not receive PROT_START to start. Exiting.\n");
        return 1;
    }

    // Convert bit value to unsigned long (global var) for WaveChkBEMain
    for( j = 0; j < fr_bins; j++ ) {
        pct_ul += (uint64_t)(pct[j] << j);
    }

    /* Register streams for threads */
    do {
        if(Network_recv(net, &tag, p, &stream) != 1) {
            fprintf(stderr, "BE: stream::recv() failure\n");
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

        /* Weak check that we got all the packets we wanted and no others */
        if(regd_streams == 3) {
            if(max_stream == NULL || min_stream == NULL || pct_stream == NULL) {
                regd_streams--;
                fprintf(stderr, "BE: WARNING Received one stream twice\n");
            }
        }
    } while(regd_streams < 3);

    /* Send acknowledgment that our streams are registered. */
    if(Stream_send(max_stream, PROT_REG_STRM, "%d", 0) == -1) {
        fprintf(stderr, "BE: Could not send stream reg. acknowledgment!\n");
        return 1;
    }

    //printf("Sent reg. stream ack to FE\n");

    Thread_Create(MinBEMain, (void *)min_stream, &min_BE);
    Thread_Create(MaxBEMain, (void *)max_stream, &max_BE);
    Thread_Create(PctBEMain, (void *)pct_stream, &pct_BE);
    
	/* Join worker threads */
    Thread_Join(min_BE, &min_ret);
    Thread_Join(max_BE, &max_ret);
    Thread_Join(pct_BE, &pct_ret);

	/* Acknowledge FE's intent to exit */
	ret_val = Network_recv(net, &tag, p, &stream);
    while(ret_val != 1) {
        if(ret_val == -1) {
            fprintf(stderr, "BE: Network_recv() failure of PROT_EXIT.\n");
            return 1;
        }
        ret_val = Network_recv(net, &tag, p, &stream);
    }

    if(tag != PROT_EXIT) {
        fprintf(stderr, "BE: Received %d. Expected PROT_EXIT.\n", tag);
        return 1;
    }

	/* Send ack on the stream we received. This will trigger FE shutdown */
	if(Stream_send(stream, PROT_EXIT, "%d", 0) == -1) {
        fprintf(stderr, "BE: Stream_send PROT_EXIT ack failure\n");
        return 1;
	}

    if (p != NULL)
        free(p);

    // wait for final teardown packet from FE
    Network_waitfor_ShutDown(net);
    if ( net != NULL )
        delete_Network_t(net);

    return 0;
}
