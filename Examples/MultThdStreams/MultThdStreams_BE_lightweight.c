/****************************************************************************
 * Copyright © 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "mrnet_lightweight/MRNet.h"
#include "MultThdStreams_lightweight.h"
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
unsigned long pct_ul;
pthread_barrier_t net_barr;
pthread_barrierattr_t net_barr_attr;

// This test serves two purposes: 
//  1. To test the lightweight library's ability to have multiple threads
//     receiving on the same network.
//
//  2. Provide a structured use of the fault-tolerant filter as in the
//     FaultRecovery example so that the user can kill a CP (if running
//     with a CP) during execution and test the lightweight library's
//     ability to recover from failure with multiple threads.
void *WaveChkBEMain(void *arg) {
    Network_t *recv_net = (Network_t *)arg;
    int tag, num, i;
    Packet_t *p = (Packet_t*)malloc(sizeof(Packet_t));
    Stream_t *stream;

    if ( Network_recv(recv_net, &tag, p, &stream) != 1 ) {
        fprintf(stderr, "BE: stream::recv() failure\n");
        return NULL;
    }
    
    Packet_unpack(p, "%d", &num);
    if(tag != PROT_WAVE_CHECK) {
        fprintf(stderr, "BE %u: WaveChkBEMain received incorrect packet: %d\n",
                me, tag);
        return NULL;
    }

    // If receiving streams this way, must stop all threads so we don't mix
    // operations on the stream and the entire network
    pthread_barrier_wait(&net_barr);

    // Let the multiple threads on the FE check our values over and over.
    // If the user chooses, she may kill a CP now.
    for(i = 0; i < num; i++) {
        printf("BE %u: Sending wave-check %d\n", me, i);
        if(Stream_send(stream, PROT_WAVE_CHECK, "%uld %ud %ud", pct_ul, max_val, min_val) == -1) {
            fprintf(stderr, "BE: stream::send() failure\n");
            return NULL;
        }
        sleep(2);
    }

    return NULL;
}

// These three thread functions are to test the most common case of
// multi-threaded usage in MRNet: assigning a thread each to a different stream.
// The threads ping-pong with the front-end using blocking and non-blocking
// Stream_recv.
void* MaxBEMain(void *arg) {
    printf("BE %d starting MaxBEMain: %d\n", (int)me, (unsigned int)pthread_self());
    Stream_t *max_stream = (Stream_t *) arg;
    int tag;
    int retval = 0;
    Packet_t *p = (Packet_t*)malloc(sizeof(Packet_t));
	assert(p);
    int i;
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
            tag = PROT_EXIT;
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
        tag = PROT_EXIT;
        return NULL;
    }
    i++;
    Stream_flush(max_stream);

    return NULL;
}

void* MinBEMain(void *arg) {
    printf("BE %d starting MinBEMain: %d\n", (int)me, (unsigned int)pthread_self());
    Stream_t *min_stream = (Stream_t *) arg;
    int tag;
    int retval = 0;
    Packet_t *p = (Packet_t*)malloc(sizeof(Packet_t));
	assert(p);
    int i;
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
        Stream_flush(min_stream);
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
    Stream_flush(min_stream);
    i++;

    return NULL;
}

void* PctBEMain(void *arg) {
    printf("BE %d starting PctBEMain: %d\n", (int)me, (unsigned int)pthread_self());
    Stream_t *pct_stream = (Stream_t *) arg;
    int tag;
    int retval = 0;
	Packet_t *p = (Packet_t*)malloc(sizeof(Packet_t));
	assert(p);
    unsigned int u;
    char bits[fr_bins+1];
    int i;
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
    Stream_flush(pct_stream);
    i++;

    return NULL;
}

int main(int argc, char **argv)
{
    Stream_t *stream;
    Stream_t *max_stream = NULL, *min_stream = NULL, *pct_stream = NULL;
    Packet_t *p = (Packet_t*)malloc(sizeof(Packet_t));
    Network_t * net;
    assert(p);
    int tag=0;
	int ret_val;
    int regd_streams = 0;

    memset( (void*)pct, 0, (size_t)fr_bins );
    
    net = Network_CreateNetworkBE(argc, argv);
    assert(net);

    me = Network_get_LocalRank(net);
    unsigned int seed = me, now = time(NULL);
    seed += (seed * 1000) + (now % 100);
    srandom( seed );

    /* Receive PROT_START and establish values for test */
    if ( Network_recv(net,&tag, p, &stream) != 1 ) {
        fprintf(stderr, "BE: stream::recv() failure\n");
        return 1;
    }

    if(tag == PROT_START) {
        Packet_unpack(p, "%ud", &num_iters );

        /* Send num_iters waves of integers */
        unsigned int i;
        for( i=0; i < num_iters; i++ ) {

            long int randval = random();
            unsigned int val = randval % fr_range_max;
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
    } else {
        //fprintf(stderr, "received %d\n", tag);
        fprintf(stderr, "Did not receive PROT_START to start. Exiting.\n");
        return 1;
    }

    // Convert bit value to unsigned long (global var) for WaveChkBEMain
    int i;
    for( i = 0; i < fr_bins; i++ ) {
        pct_ul += (pct[i] << i);
    }

    pthread_barrier_init(&net_barr, &net_barr_attr, 3);
    pthread_t wave_chk_BE0;
    pthread_t wave_chk_BE1;
    pthread_t wave_chk_BE2;

    pthread_create(&wave_chk_BE0, NULL, WaveChkBEMain, (void *)net);
    pthread_create(&wave_chk_BE1, NULL, WaveChkBEMain, (void *)net);
    pthread_create(&wave_chk_BE2, NULL, WaveChkBEMain, (void *)net);
    
	/* Join worker threads */
    void *chk0_ret;
    void *chk1_ret;
    void *chk2_ret;
    pthread_join(wave_chk_BE0, &chk0_ret);
    pthread_join(wave_chk_BE1, &chk1_ret);
    pthread_join(wave_chk_BE2, &chk2_ret);

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
    
    pthread_t min_BE;
    pthread_t max_BE;
    pthread_t pct_BE;

    pthread_create(&min_BE, NULL, MinBEMain, (void *)min_stream);
    pthread_create(&max_BE, NULL, MaxBEMain, (void *)max_stream);
    pthread_create(&pct_BE, NULL, PctBEMain, (void *)pct_stream);
    
	/* Join worker threads */
    void *min_ret;
    void *max_ret;
    void *pct_ret;
    pthread_join(min_BE, &min_ret);
    pthread_join(max_BE, &max_ret);
    pthread_join(pct_BE, &pct_ret);

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
