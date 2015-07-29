/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "mrnet/MRNet.h"
#include "PerfData.h"

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

using namespace MRN;

int main(int argc, char **argv)
{
    int tag, retval;
    PacketPtr p;

    if( argc != 4 ){
        fprintf(stderr, "Usage: %s <topology file> <backend_exe> <so_file>\n", argv[0]);
        exit(-1);
    }
    const char * topology_file = argv[1];
    const char * backend_exe = argv[2];
    const char * so_file = argv[3];
    const char * dummy_argv=NULL;

    // This Network() cnstr instantiates the MRNet internal nodes, according to the
    // organization in "topology_file," and the application back-end with any
    // specified cmd line args
    Network * net = Network::CreateNetworkFE( topology_file, backend_exe, &dummy_argv  );

    // A Broadcast communicator contains all the back-ends
    Communicator * comm_BC = net->get_BroadcastCommunicator();

    // Make sure path to "so_file" is in LD_LIBRARY_PATH
    int filter_id = net->load_FilterFunc( so_file, "PerfDataFilt" );
    if( filter_id == -1 ) {
        fprintf( stderr, "Network::load_FilterFunc() failure\n");
        delete net;
        return -1;
    }

    // Create a stream that will use the filter for aggregation
    Stream * stream = net->new_Stream( comm_BC, filter_id,
                                       SFILTER_WAITFORALL );

    stream->enable_PerformanceData( PERFDATA_MET_NUM_BYTES, PERFDATA_CTX_SEND );
    stream->enable_PerformanceData( PERFDATA_MET_NUM_BYTES, PERFDATA_CTX_RECV );
    stream->enable_PerformanceData( PERFDATA_MET_NUM_PKTS, PERFDATA_CTX_SEND );
    stream->enable_PerformanceData( PERFDATA_MET_NUM_PKTS, PERFDATA_CTX_RECV );
    stream->enable_PerformanceData( PERFDATA_MET_NUM_PKTS, PERFDATA_CTX_FILT_IN );
    stream->enable_PerformanceData( PERFDATA_MET_NUM_PKTS, PERFDATA_CTX_FILT_OUT );
    stream->enable_PerformanceData( PERFDATA_MET_ELAPSED_SEC, PERFDATA_CTX_FILT_OUT );

    int num_backends = int(comm_BC->get_EndPoints().size());

    tag = PROT_SUM;
    int send_val=10;
    int recv_val;
    int num_iters=5;
    // Broadcast a control message to back-ends to send us "num_iters"
    // waves of integers
    if( stream->send( tag, "%d %d", send_val, num_iters ) == -1 ){
        fprintf( stderr, "stream::send() failure\n");
        return -1;
    }
    if( stream->flush( ) == -1 ){
        fprintf( stderr, "stream::flush() failure\n");
        return -1;
    }

    // We expect "num_iters" aggregated responses from all back-ends
    for( int i=0; i < num_iters; i++ ) {
        retval = stream->recv(&tag, p);
        assert( retval != 0 ); //shouldn't be 0, either error or block till data
        if( retval == -1 ){
            //recv error
            return -1;
        }

        if( p->unpack( "%d", &recv_val ) == -1 ){
            fprintf( stderr, "stream::unpack() failure\n");
            return -1;
        }

        if( recv_val != num_backends * i * send_val ){
            printf("Iteration %d: Failure! recv_val(%d) != %d*%d*%d=%d (send_val*i*num_backends)\n",
                   i, recv_val, send_val, i, num_backends, send_val*i*num_backends );
        }
        else{
            printf("Iteration %d: Success! recv_val(%d) == %d*%d*%d=%d (send_val*i*num_backends)\n",
                   i, recv_val, send_val, i, num_backends, send_val*i*num_backends );
        }
    }

    rank_perfdata_map data;
    stream->print_PerformanceData( PERFDATA_MET_NUM_BYTES, PERFDATA_CTX_SEND );
    stream->print_PerformanceData( PERFDATA_MET_NUM_BYTES, PERFDATA_CTX_RECV );
    stream->print_PerformanceData( PERFDATA_MET_NUM_PKTS, PERFDATA_CTX_SEND );
    stream->print_PerformanceData( PERFDATA_MET_NUM_PKTS, PERFDATA_CTX_RECV );
    stream->print_PerformanceData( PERFDATA_MET_NUM_PKTS, PERFDATA_CTX_FILT_IN );
    stream->print_PerformanceData( PERFDATA_MET_NUM_PKTS, PERFDATA_CTX_FILT_OUT );
    stream->collect_PerformanceData( data,
                                     PERFDATA_MET_ELAPSED_SEC, PERFDATA_CTX_FILT_OUT,
                                     TFILTER_AVG );
    rank_perfdata_map::iterator mi = data.begin();
    for( ; mi != data.end() ; mi++ ) {
        size_t sz = mi->second.size();
        printf("Average of %d ranks :\n", 0-(int)mi->first);
        for(size_t u=0; u < sz; u++ ) {
            // NOTE: for unsigned integer data use - printf(" %" PRIu64, mi->second[u].u);
            // NOTE: for signed integer data use - printf(" %" PRIi64, mi->second[u].i);
            printf("  filter execution %zu time: %.9lf\n", u, mi->second[u].d);
        }
    }

    if( stream->send(PROT_EXIT, "") == -1 ) {
        fprintf( stderr, "stream::send(exit) failure\n");
        return -1;
    }
    if( stream->flush() == -1 ) {
        fprintf( stderr, "stream::flush() failure\n");
        return -1;
    }

    // The Network destructor will cause all internal and leaf tree nodes to exit
    delete net;

    return 0;
}
