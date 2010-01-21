/****************************************************************************
 * Copyright � 2003-2008 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <assert.h>

#ifndef os_windows
#include <unistd.h>
#else
#include <winsock2.h>
#endif

#include "microbench_lightweight.h"

int main( int argc, char* argv[] )
{
    Stream_t* stream = (Stream_t*)malloc(sizeof(Stream_t));
    int tag;
    Packet_t* pkt = (Packet_t*)malloc(sizeof(Packet_t));
	int bCont;
	Network_t* net;
	int done;
	int rret;
	int nReductions = 0;
    int ival;
	int i;

    if( getenv( "MRN_DEBUG_BE" ) != NULL ) {
#ifndef os_windows
		fprintf( stderr, "BE: spinning for debugger, pid=%d\n", getpid() );
#else
		fprintf(stderr, "BE: spinning for debugger, pid=%d\n", (int)GetCurrentProcessId());
#endif
        bCont=false;
        while( !bCont )
        {
            // spin
        }
    }

    // join the MRNet net
    net = Network_CreateNetworkBE( argc, argv );

    // participate in the broadcast/reduction roundtrip latency experiment
    done = false;
    while( !done ) {
        // receive the broadcast message
        tag = 0;
        rret = Network_recv(net,  &tag, pkt, &stream);
        if( rret == -1 ) {
            fprintf(stderr, "BE: Stream_recv() failed\n");
            return -1;
        }

        if( tag == MB_ROUNDTRIP_LATENCY ) {
            // extract the value and send it back
            int ival = 0;
            Packet_unpack(pkt,  "%d", &ival );

            // send our value for the reduction
            if( (Stream_send(stream,  tag, "%d", ival ) == -1) ||
                (Stream_flush(stream) == -1) ) {
                fprintf(stderr, "BE: roundtrip exp reduction failed\n");
                return -1;
            }
        }
        else {
            // we're done with the roundtrip latency experiment
            done = true;
            if( tag != MB_RED_THROUGHPUT ) {
                fprintf(stderr, "BE: unexpected tag %d seen to end roundtrip experiments\n", tag);
            }
        }
    }


    //
    // participate in the reduction throughput experiment
    //

    // determine the number of reductions required
    assert( tag == MB_RED_THROUGHPUT );
    assert( stream != NULL );

    Packet_unpack(pkt,  "%d %d", &nReductions, &ival );

    // do the reductions
    for( i = 0; i < nReductions; i++ ) {

        // send a value for the reduction
        if( (Stream_send(stream,  MB_RED_THROUGHPUT, "%d", ival ) == -1 ) ||
            (Stream_flush(stream) == -1) ) {
            fprintf(stderr, "BE: reduction throughput exp reduction failed\n");
            return -1;
        }
    }
    // cleanup
    // receive a go-away message
    tag = 0;
    rret = Network_recv(net,  &tag, pkt, &stream);
    if( (rret != -1) && (tag != MB_EXIT) ) {
        fprintf(stderr, "BE: received unexpected go-away tag %d\n", tag);
    }
    if( tag != MB_EXIT ) {
        fprintf(stderr, "BE: received unexpected go-away tag %d\n", tag);
    }

    Network_recv(net,  &tag, pkt, &stream);

    return 0;
}