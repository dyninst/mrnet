/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <iostream>
#include <string>
#include <stdlib.h>
#include <assert.h>

#include "mrnet/MRNet.h"
#include "timer.h"
#include "microbench.h"

#ifndef os_windows
#include <unistd.h>
#else
#include <winsock2.h>
#endif

using namespace MRN;
const unsigned int kMaxRecvTries = 1000000;


Network * BuildNetwork( std::string cfgfile, std::string backend_exe );

int DoRoundtripLatencyExp( Stream* stream,
                           unsigned long nIters,
                           unsigned int nBackends );
int DoReductionThroughputExp( Stream* stream,
                              unsigned long nIters,
                              unsigned int nBackends );


int
main( int argc, char* argv[] )
{
    int ret;

    // parse the command line
    if( argc != 5 ) {
        std::cerr << "Usage: " << argv[0]
                << "<roundtrip_iters> <thru_iters>"
                << " <topology file> <backend exe>"
                << std::endl;
        return -1;
    }
    
    unsigned long nRoundtripIters  = strtoul( argv[1], NULL, 10 );
    unsigned long nThroughputIters  = strtoul( argv[2], NULL, 10 );
    std::string topology_file = argv[3];
    std::string backend_exe = argv[4];
    
    // instantiate the network
    Network * net = BuildNetwork( topology_file, backend_exe );
    if( net == NULL )
        return -1;

    // get a broadcast communicator
    Communicator * bcComm = net->get_BroadcastCommunicator();
    Stream* stream = net->new_Stream( bcComm, TFILTER_SUM );
    assert( bcComm != NULL );
    assert( stream != NULL );
    unsigned int nBackends = (unsigned int)bcComm->get_EndPoints().size();
    std::cout << "FE: broadcast communicator has " 
              << nBackends << " backends" 
              << std::endl;

    // perform roundtrip latency experiment
    ret = DoRoundtripLatencyExp( stream, nRoundtripIters, nBackends );
    if( ret == 0 ) {

        // perform reduction throughput experiment
        ret = DoReductionThroughputExp( stream, nThroughputIters, nBackends );
        if( ret == 0 ) {

            // tell back-ends to go away
            if( (stream->send( MB_EXIT, "%d", 0 ) == -1) ||
                (stream->flush() == -1) ) {
                std::cerr << "FE: failed to broadcast termination message" << std::endl;
            }
            delete stream;
        }
    }

    // delete the net
    std::cout << "FE: deleting net" << std::endl;
    delete net;

    return ret;
}


Network *
BuildNetwork( std::string cfgfile, std::string backend_exe )
{
    mb_time startTime;
    mb_time endTime;
    const char *argv = NULL;

    std::map< std::string, std::string > attrs;
    std::string debug_level = "MRNET_OUTPUT_LEVEL";
    char* lvl = getenv( debug_level.c_str() );
    if( lvl != NULL )
        attrs[debug_level] = std::string( lvl );

    std::cout << "FE: net instantiation: " << std::endl;
    startTime.set_time();
    Network * net = Network::CreateNetworkFE( cfgfile.c_str(), 
                                              backend_exe.c_str(), &argv,
                                              &attrs );
    endTime.set_time();

    if( net->has_Error() ) {
        delete net;
        return NULL;
    }

    net->set_FailureRecovery( false );

    // dump net instantiation latency
    double latency = (endTime - startTime).get_double_time();
    std::cout << " latency(sec): " << latency << std::endl;

    return net;
}


int
DoRoundtripLatencyExp( Stream* stream,
                       unsigned long nIters,
                       unsigned int nBackends )
{
    mb_time startTime;
    mb_time endTime;

    int sret = -1;
    int fret = -1;

    std::cout << "FE: starting roundtrip latency experiment" << std::endl;

    startTime.set_time();
    for( unsigned long i = 0; i < nIters; i++ ) {
        // broadcast
        sret = stream->send( MB_ROUNDTRIP_LATENCY, "%d", 1 );
        if( sret != -1 ) {
            fret = stream->flush();
        }
        if( (sret == -1) || (fret == -1) ) {
            std::cerr << "broadcast failed" << std::endl;
            return -1;
        }

        // receive reduced value
        int tag = 0;
        PacketPtr buf;
        int rret = stream->recv( &tag, buf );
        if( rret == -1 ) {
            std::cerr << "FE: roundtrip latency recv() failed - stream error\n";
            return -1;
        }
        else if( rret == 0 ) {
            std::cerr << "FE: roundtrip latency recv() failed - stream closed\n";
            return -1;
        }

        // check tag and value
        if( tag != MB_ROUNDTRIP_LATENCY ) {
            std::cerr << "FE: roundtrip latency recv() found unexpected tag=" 
                      << tag << std::endl;
        }
        else {
            int ival = 0;
            buf->unpack( "%d", &ival );
            if( ival != (int)nBackends ) {
                std::cerr << "FE: unexpected reduction value " << ival 
                          << " seen, expected " << nBackends << std::endl;
            }
        }
    }
    endTime.set_time();

    // dump broadcast/reduction roundtrip latency
    double totalLatency = (endTime - startTime).get_double_time();
    double avgLatency = totalLatency / (double)nIters;
    std::cout << "FE: roundtrip latency: "
              << "total(sec): " << totalLatency
              << ", nIters: " << nIters
              << ", avg(sec): " << avgLatency
              << std::endl;

    return 0;
}

int
DoReductionThroughputExp( Stream* stream,
                          unsigned long nIters,
                          unsigned int nBackends )
{
    mb_time startTime;
    mb_time endTime;

    std::cout << "FE: starting reduction throughput experiment" << std::endl;

    // broadcast request to start throughput experiment
    // we send number of iterations to do and the value to send
    if( (stream->send( MB_RED_THROUGHPUT, "%d %d", nIters, 1 ) == -1) ||
        (stream->flush() == -1) ) {
        std::cerr << "FE: failed to start throughput experiment" << std::endl;
        return -1;
    }

    // do the experiment
    startTime.set_time();
    for( unsigned long i = 0; i < nIters; i++ ) {

        // receive reduced value
        int tag = 0;
        PacketPtr buf;
        int rret = stream->recv( &tag, buf );
        if( rret == -1 ) {
            std::cerr << "FE: reduction throughput recv() failed - stream error\n"; 
            return -1;
        }
        else if( rret == 0 ) {
            std::cerr << "FE: reduction throughput recv() failed - stream closed\n";
            return -1;
        }

        // check tag and value
        if( tag != MB_RED_THROUGHPUT ) {
            std::cerr << "FE: reduction throughput recv() found unexpected tag=" 
                      << tag << std::endl;
        }
        else {
            int ival = 0;
            buf->unpack( "%d", &ival );
            if( ival != (int)nBackends ) {
                std::cerr << "FE: unexpected reduction value " << ival 
                          << " received, expected " << nBackends << std::endl;
            }
        }
    }
    endTime.set_time();

    // dump reduction throughput
    double expLatency = (endTime - startTime).get_double_time();
    double throughput = ((double)nIters) / expLatency;
    std::cout << "FE: reduction throughput: "
              << "nIters: " << nIters
              << ", latency(sec): " << expLatency
              << ", throughput(ops/sec): " << throughput
              << std::endl;

    return 0;
}


