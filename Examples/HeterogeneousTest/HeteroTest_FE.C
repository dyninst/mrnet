/**************************************************************************
 * Copyright 2003-2009   James Jolly, Michael J. Brim, Barton P. Miller   *
 *                Detailed MRNet usage rights in "LICENSE" file.          *
 **************************************************************************/

// This sanity-check shows how multiple packet filter types can be used in a single
// MRNet stream.
// USAGE: ./HeteroTest_FE (run from Examples directory)

// "What's going on here?":
// Uses topology with 5 nodes where the root node (id 4) has four children (ids 0 - 3).
// Four filters are loaded: I, J, X, and Y.
// Each filter simply marks packets that pass through it with its name and node rank.
// The following static mapping of filters to nodes is used:
// All packets from the root to the children pass through I.
// All packets from the children to the root pass through J.
// (Both I and J are running at the root node)
// The children then use the following:
// 0's downstream is X and its upstream is Y.
// 1's downstream is Y and its upstream is X.
// 2's downstream is X and its upstream is Y.
// 3's downstream is Y and its upstream is Y.
// When run, the root sends one message to each of its children, which immediately send
// them back to the root. These 4 packets should contain the following histories:
// (4,I)(0,X)(0,X)(4,J)
// (4,I)(1,Y)(1,X)(4,J)
// (4,I)(2,X)(2,Y)(4,J)
// (4,I)(3,Y)(3,Y)(4,J)

#include <iostream>
#include <string>
using std::cout;
using std::string;

#include "mrnet/MRNet.h"
#include "HeteroTest.h"

using namespace MRN;

int main( int argc, char ** argv )
{
    int tag, retval;
    PacketPtr p;

    if( argc != 1 ){
        fprintf( stderr, "run %s from the Examples directory\n", argv[0] );
        exit( -1 );
    }
    const char * topology_file = "./HeterogeneousTest/HeteroTest.top";
    const char * backend_exe = "./HeteroTest_BE";
    const char * so_file = "./HeteroTest.so";
    const char * dummy_argv = NULL;
    Network * net = new Network( topology_file, backend_exe, &dummy_argv );
    if( net->has_Error( ) ){
        net->perror( "network creation failed" );
        exit( -1 );
    }

    uint node_rank = network->get_LocalRank( );
    cout << "frontend started on node " << node_rank << "...\n";

    int insertX_id = net->load_FilterFunc( so_file, "insertX" );
    int insertY_id = net->load_FilterFunc( so_file, "insertY" );
    int insertI_id = net->load_FilterFunc( so_file, "insertI" );
    int insertJ_id = net->load_FilterFunc( so_file, "insertJ" );
    cout << "custom filter insertX id: " << insertX_id << "\n"
         << "custom filter insertY id: " << insertY_id << "\n"
         << "custom filter insertI id: " << insertI_id << "\n"
         << "custom filter insertJ id: " << insertJ_id << "\n";
    if( insertX_id == -1 || insertY_id == -1 ||  insertI_id == -1 || insertJ_id == -1 ){
        fprintf( stderr, "could not load filters\n" );
        delete net;
        return -1;
    }

    Communicator * comm_BC = net->get_BroadcastCommunicator( );
    string upstream("15 => 0 ; 15 => 1 ; 16 => 2 ; 16 => 3 ; 18 => 4 ;"); 
    string sync("12 => *"); // use SFILTER_DONTWAIT for everything
    string downstream("15 => 0 ; 16 => 1 ; 15 => 2 ; 16 => 3 ; 17 => 4 ;"); 
    Stream * stream = net->new_Stream(comm_BC, upstream, sync, downstream);

    int num_backends = comm_BC->get_EndPoints( ).size( );
    char * test_pong( NULL );
    char * test_ping = (char *)"";
    if( stream->send( PROT_TEST, "%s", test_ping ) == -1 ){
        fprintf( stderr, "stream::send( ) failure\n");
        return -1;
    }
    if( stream->flush( ) == -1 ){
        fprintf( stderr, "stream::flush( ) failure\n");
        return -1;
    }
    cout << "frontend sent (blank) start message\n";

    for( int b = 0; b < 4; b++ ){
        retval = stream->recv(&tag, p);
        if( retval == -1 ){
            fprintf( stderr, "stream::recv( ) failure\n");
            return -1;
        }
        if( p->unpack( "%s", &test_pong ) == -1 ){
            fprintf( stderr, "stream::unpack( ) failure\n");
            return -1;
        }
        fprintf(stderr, "frontend got %s\n", test_pong);
    }

    fprintf(stderr, "frontend cleaning up\n");
    delete stream;
    delete net;
    fprintf(stderr, "frontend done cleaning up\n");
    return 0;
}
