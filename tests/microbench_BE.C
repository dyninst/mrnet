/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <iostream>
#include <assert.h>
#ifndef os_windows
#include <unistd.h>
#else
#include <winsock2.h>
#endif

#include "mrnet/MRNet.h"
#include "microbench.h"

using namespace MRN;



int main( int argc, char* argv[] )
{
    Stream* stream;
    int tag;
    PacketPtr pkt;

    if( getenv( "MRN_DEBUG_BE" ) != NULL ) {
#ifndef os_windows
		fprintf( stderr, "BE: spinning for debugger, pid=%d\n", getpid() );
#else 
		fprintf(stderr, "BE: spinning for debugger, pid=%d\n", (int)GetCurrentProcessId());
#endif
        bool bCont=false;
        while( !bCont )
        {
            // spin
        }
    }

    // join the MRNet net
    Network * net = Network::CreateNetworkBE( argc, argv );

    // participate in the broadcast/reduction roundtrip latency experiment
    bool done = false;
    while( !done ) {
        // receive the broadcast message
        tag = 0;
        int rret = net->recv( &tag, pkt, &stream );
        if( rret == -1 ) {
            std::cerr << "BE: Network::recv() failed" << std::endl;
            break;
        }

        if( tag == MB_ROUNDTRIP_LATENCY ) {
            // extract the value and send it back
            int ival = 0;
            pkt->unpack( "%d", &ival );

            // send our value for the reduction
            if( (stream->send( tag, "%d", ival ) == -1) ||
                (stream->flush() == -1) ) {
                std::cerr << "BE: roundtrip exp reduction failed" << std::endl;
                break;
            }
        }
        else {
            // we're done with the roundtrip latency experiment
            done = true;
            if( tag != MB_RED_THROUGHPUT ) {
                std::cerr << "BE: unexpected tag " << tag 
                        << " seen to end roundtrip experiments"
                        << std::endl;
            }
        }
    }


    //
    // participate in the reduction throughput experiment
    //

    // determine the number of reductions required
    if( tag == MB_RED_THROUGHPUT ) {
        int nReductions = 0;
        int ival;
        pkt->unpack( "%d %d", &nReductions, &ival );

        // do the reductions
        for( int i = 0; i < nReductions; i++ ) {
            
            // send a value for the reduction
            if( (stream->send( MB_RED_THROUGHPUT, "%d", ival ) == -1 ) ||
                (stream->flush() == -1) ) {
                std::cerr << "BE: reduction throughput exp reduction failed" 
                          << std::endl;
                break;
            }
        }
    }

    // cleanup
    // receive a go-away message
    tag = 0;
    int rret = net->recv( &tag, pkt, &stream );
    if( rret == -1 ) {
        std::cerr << "BE: failed to receive go-away tag" << std::endl;
    }
    else if( tag != MB_EXIT ) {
        std::cerr << "BE: received tag " << tag << " instead of go-away tag" << std::endl;
    }

    // wait for FE to delete network, which will shut us down
    net->waitfor_ShutDown();
    delete net;

    return 0;
}
