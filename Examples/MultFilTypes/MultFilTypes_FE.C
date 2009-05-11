/**************************************************************************
 * Copyright 2003-2008 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                Detailed MRNet usage rights in "LICENSE" file.          *
 **************************************************************************/

// Example use of multiple upstream filter types in a single MRNet tree...
// Give it a whirl:
// MultFilTypes_FE ./MultFilTypes_BE ./MultFilTypes.so
// Uses topology with one parent node and two children.
// The parent tells the children how many terms of the Fibonacci sequence
// it desires, and they in turn compute these terms and hand them upwards.
// The filter that runs at the root of the tree is like that of the children,
// except it limits its output at 42.

#include <iostream>
#include <string>

#include "mrnet/MRNet.h"
#include "MultFilTypes.h"

using namespace MRN;

int main( int argc, char ** argv )
{
    int send_val = 1, recv_val = 0;
    int tag, retval;
    PacketPtr p;

    if( argc != 3 ){
        fprintf( stderr, "Usage: %s <backend_exe> <so_file>\n", argv[0] );
        exit(-1);
    }
    const char * topology_file = "./MultFilTypes/local-1x2.top";
    const char * backend_exe = argv[1];
    const char * so_file = argv[2];
    const char * dummy_argv = NULL;

    Network * net = new Network( topology_file, backend_exe, &dummy_argv );
    if( net->has_Error() ) {
        net->perror( "network creation failed" );
        exit(-1);
    }

    uint node_rank = network->get_LocalRank( );
    std::cout << "frontend started on node " << node_rank << "...\n";

    int filter_id_1 = net->load_FilterFunc( so_file, "SumThenLimit42" );
    std::cout << "custom filter SumThenLimit42 id:" << filter_id_1 << "\n";
    if( filter_id_1 == -1 ){
        fprintf( stderr, "could not load SumThenLimit42\n" );
        delete net;
        return -1;
    }

    int filter_id_2 = net->load_FilterFunc( so_file, "SumOnly" );
    std::cout << "custom filter SumOnly id:" << filter_id_2 << "\n";
    if( filter_id_2 == -1 ){
        fprintf( stderr, "could not load SumOnly\n" );
        delete net;
        return -1;
    }
        
    int filter_id_3 = net->load_FilterFunc( so_file, "Pass" );
    std::cout << "custom filter Pass id:" << filter_id_3 << "\n";
    if( filter_id_2 == -1 ){
        fprintf( stderr, "could not load Pass\n" );
        delete net;
        return -1;
    }

    std::cout << "built-in filter WAITFORALL id:" << SFILTER_WAITFORALL << "\n";

    Communicator * comm_BC = net->get_BroadcastCommunicator( );
    std::string s1("16 => 0 ; 16 => 1 ; 15 => 2 ;"); // upstream filters
    std::string s2("13 => 0 ; 13 => 1 ; 13 => 2 ;"); // synchronization filters
    std::string s3("17 => 0 ; 17 => 1 ; 17 => 2 ;"); // downstream filters
    Stream * add_stream = net->new_Stream(comm_BC, s1, s2, s3);

    // Create a stream that will use the Integer_Add filter for aggregation
    int num_backends = comm_BC->get_EndPoints( ).size( );

    // Broadcast a control message to back-ends to send us "num_iters"
    // waves of integers
    tag = PROT_SUM;
    unsigned int num_iters = 10;
    if( add_stream->send( tag, "%d %d", send_val, num_iters ) == -1 ){
        fprintf( stderr, "stream::send() failure\n");
        return -1;
    }
    if( add_stream->flush( ) == -1 ){
        fprintf( stderr, "stream::flush() failure\n");
        return -1;
    }

    // num_iters (aggregated) responses should arrive from back-ends
    for( unsigned int i = 0; i < num_iters; i++ ){
        retval = add_stream->recv(&tag, p);
        if( retval == -1 ){
            fprintf( stderr, "stream::recv() failure\n");
            return -1;
        }
        if( p->unpack( "%d", &recv_val ) == -1 ){
            fprintf( stderr, "stream::unpack() failure\n");
            return -1;
        }
        fprintf(stderr, "FE got %d\n", recv_val);
    }

    if( add_stream->send(PROT_EXIT, "") == -1 ){
        fprintf( stderr, "stream::send(exit) failure\n");
        return -1;
    }
    if( add_stream->flush() == -1 ){
        fprintf( stderr, "stream::flush() failure\n");
        return -1;
    }
    retval = add_stream->recv(&tag, p);
    if( retval == -1 ){
         fprintf( stderr, "stream::recv() failure\n");
         return -1;
    }
    delete add_stream;
    if( tag == PROT_EXIT ){
        // force all internal and leaf tree nodes to exit
        delete net;
    }

    return 0;
}
