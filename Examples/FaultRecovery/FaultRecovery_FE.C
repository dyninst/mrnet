/****************************************************************************
 * Copyright � 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "mrnet/MRNet.h"
#include "FaultRecovery.h"
#include <bitset>
#include <iostream>
#include <string>
using namespace std;

using namespace MRN;

int main(int argc, char **argv)
{
    unsigned int min_val=0;
    unsigned int max_val=0;
    unsigned int bits_val;
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
    Network * net = new Network( topology_file, backend_exe, &dummy_argv );
    if( net->has_Error() ) {
        net->perror("Network creation failed");
        return -1;
    }

    NetworkTopology* nettop = net->get_NetworkTopology();
    if( nettop->get_Root()->find_SubTreeHeight() < 2 ) {
        fprintf(stderr, "Please use a topology with depth >= 2.\n"
                        "There needs to be at least one mrnet_commnode process to kill.\n");
        delete net;
        return -1;
    }

    // Make sure path to "so_file" is in LD_LIBRARY_PATH
    int filter_id = net->load_FilterFunc( so_file, "IntegerPercentiles" );
    if( filter_id == -1 ){
        fprintf( stderr, "Network::load_FilterFunc() failure\n");
        delete net;
        return -1;
    }
    int chk_filter_id = net->load_FilterFunc( so_file, "BitsetOr" );
    if( chk_filter_id == -1 ){
        fprintf( stderr, "Network::load_FilterFunc() failure\n");
        delete net;
        return -1;
    }

    // A Broadcast communicator contains all the back-ends
    Communicator * comm_BC = net->get_BroadcastCommunicator( );

    // Create a stream that will use the Integer_Add filter for aggregation
    Stream * add_stream = net->new_Stream( comm_BC, filter_id,
                                           SFILTER_WAITFORALL );

    int num_backends = comm_BC->get_EndPoints().size();

    // Broadcast a control message to back-ends to send us "num_iters"
    // waves of integers
    tag = PROT_START;
    unsigned int num_iters=50;
    if( add_stream->send( tag, "%ud", num_iters ) == -1 ){
        fprintf( stderr, "stream::send() failure\n");
        return -1;
    }
    if( add_stream->flush( ) == -1 ){
        fprintf( stderr, "stream::flush() failure\n");
        return -1;
    }

    // We expect "num_iters" aggregated responses from all back-ends
    for( unsigned int i=0; i < num_iters; i++ ){

        retval = add_stream->recv(&tag, p);
        assert( retval != 0 ); //shouldn't be 0, either error or block till data
        if( retval == -1){
            //recv error
            fprintf( stderr, "stream::recv() failure\n");
            return -1;
        }
        assert( tag == PROT_WAVE );
        if( p->unpack( "%ud %ud %ud", &bits_val, &max_val, &min_val ) == -1 ){
            fprintf( stderr, "stream::unpack() failure\n");
            return -1;
        }
        fprintf(stdout, "FE: min %u max %u bits %u\n", min_val, max_val, bits_val);
        fflush(stdout);
    }
    delete add_stream;

    // Check min
    Stream * min_stream = net->new_Stream( comm_BC, TFILTER_MIN,
                                           SFILTER_WAITFORALL );
    if(min_stream->send(PROT_CHECK_MIN, "") == -1){
        fprintf( stderr, "stream::send(exit) failure\n");
        return -1;
    }
    if(min_stream->flush() == -1){
        fprintf( stderr, "stream::flush() failure\n");
        return -1;
    }
    retval = min_stream->recv(&tag, p);
    if( retval == -1){
         //recv error
         fprintf( stderr, "stream::recv() failure\n");
         return -1;
    }
    unsigned int good_min;
    p->unpack("%ud", &good_min);
    fprintf(stdout, "FE: check of MIN ");
    if( good_min != min_val ) fprintf(stdout, "failed, filter %u != check %u\n", min_val, good_min);
    else fprintf(stdout, "successful %u\n", min_val);
    fflush(stdout);
    delete min_stream;

    // Check max
    Stream * max_stream = net->new_Stream( comm_BC, TFILTER_MAX,
                                           SFILTER_WAITFORALL );
    if(max_stream->send(PROT_CHECK_MAX, "") == -1){
        fprintf( stderr, "stream::send(exit) failure\n");
        return -1;
    }
    if(max_stream->flush() == -1){
        fprintf( stderr, "stream::flush() failure\n");
        return -1;
    }
    retval = max_stream->recv(&tag, p);
    if( retval == -1){
         //recv error
         fprintf( stderr, "stream::recv() failure\n");
         return -1;
    }
    unsigned int good_max;
    p->unpack("%ud", &good_max);
    fprintf(stdout, "FE: check of MAX ");
    if( good_max != max_val ) fprintf(stdout, "failed, filter %u != check %u\n", max_val, good_max);
    else fprintf(stdout, "successful %u\n", max_val);
    fflush(stdout);
    delete max_stream;

    // Check percentiles
    Stream * pct_stream = net->new_Stream( comm_BC, chk_filter_id,
                                           SFILTER_WAITFORALL );
    if(pct_stream->send(PROT_CHECK_PCT, "") == -1){
        fprintf( stderr, "stream::send(exit) failure\n");
        return -1;
    }
    if(pct_stream->flush() == -1){
        fprintf( stderr, "stream::flush() failure\n");
        return -1;
    }
    retval = pct_stream->recv(&tag, p);
    if( retval == -1){
         //recv error
         fprintf( stderr, "stream::recv() failure\n");
         return -1;
    }
    unsigned int good_pct;
    p->unpack("%ud", &good_pct);
    string bits = bitset<10>(bits_val).to_string<char,char_traits<char>,allocator<char> >();
    string good = bitset<10>(good_pct).to_string<char,char_traits<char>,allocator<char> >();
    fprintf(stdout, "FE: check of PERCENTILES ");
    if( good_pct != bits_val ) fprintf(stdout, "failed, filter %s != check %s\n", 
                                       bits.c_str(), good.c_str());
    else fprintf(stdout, "successful %s\n", bits.c_str());
    fflush(stdout);
    delete pct_stream;

    // Tell back-ends to exit
    Stream * ctl_stream = net->new_Stream( comm_BC, TFILTER_MAX,
                                           SFILTER_WAITFORALL );
    if(ctl_stream->send(PROT_EXIT, "") == -1){
        fprintf( stderr, "stream::send(exit) failure\n");
        return -1;
    }
    if(ctl_stream->flush() == -1){
        fprintf( stderr, "stream::flush() failure\n");
        return -1;
    }
    retval = ctl_stream->recv(&tag, p);
    if( retval == -1){
         //recv error
         fprintf( stderr, "stream::recv() failure\n");
         return -1;
    }
    delete ctl_stream;
    delete net; // destructor will cause all internal and leaf tree nodes to exit


    return 0;
}