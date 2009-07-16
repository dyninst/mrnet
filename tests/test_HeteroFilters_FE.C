/**************************************************************************
 * Copyright 2003-2009   James Jolly, Michael J. Brim, Barton P. Miller   *
 *                Detailed MRNet usage rights in "LICENSE" file.          *
 **************************************************************************/

// This sanity-check shows how multiple packet filter types can be used in a single
// MRNet stream.
// USAGE: ./test_HeteroFilters_FE (run from bin/$PLATFORM directory)

// "What's going on here?":
// Uses topology with 5 nodes where the root node (rank 4) has four children (ranks 0 - 3).
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
#include "test_common.h"
#include "test_HeteroFilters.h"

using namespace MRN;
using namespace MRN_test;
Test * test;

int main( int argc, char ** argv )
{
    int tag, retval;
    PacketPtr p;

    if( argc != 3 ){
        fprintf( stderr, "Usage: %s <shared_object file> <backend_exe>\n", argv[0] );
        exit( -1 );
    }
    const char * topology_file = "../../tests/topology_files/local-1x4.top";
    const char * so_file = argv[1];
    const char * backend_exe = argv[2];
    const char * dummy_argv = NULL;

    fprintf(stderr,"MRNet C++ Interface *Heterogeneous Stream* Test Suite\n"
                   "-----------------------------------------------------\n"
                   "This test suite performs tests that exercise\n"
                   "MRNet's \"Heterogeneous Stream Filters\" functionality.\n");
   
    test = new Test("MRNet Heterogeneous Stream Filters Test");

    Network * net = new Network( topology_file, backend_exe, &dummy_argv );
    if( net->has_Error( ) ){
        net->perror( "network creation failed" );
        exit( -1 );
    }

    Communicator * comm_BC = net->get_BroadcastCommunicator( );

    std::string testname("test_HeterogeneousStream");
    test->start_SubTest(testname);

    Rank node_rank = net->get_LocalRank( );

    int insertX_id = net->load_FilterFunc( so_file, "insertX" );
    int insertY_id = net->load_FilterFunc( so_file, "insertY" );
    int insertI_id = net->load_FilterFunc( so_file, "insertI" );
    int insertJ_id = net->load_FilterFunc( so_file, "insertJ" );
    cout << "FE custom filter insertX id: " << insertX_id << "\n"
         << "FE custom filter insertY id: " << insertY_id << "\n"
         << "FE custom filter insertI id: " << insertI_id << "\n"
         << "FE custom filter insertJ id: " << insertJ_id << "\n";
    if( insertX_id == -1 || insertY_id == -1 ||  insertI_id == -1 || insertJ_id == -1 ){
        test->print( "could not load filters\n", testname );
        test->end_SubTest(testname, FAILURE);
        return -1;
    }

    ostringstream up;
    up << insertX_id << " => 0 ; " << insertX_id << " => 1 ; "
       << insertY_id << " => 2 ; " << insertY_id << " => 3 ; "
       << insertJ_id << " => 4 ;" ;
    string upstream( up.str() ); 

    ostringstream sync;
    sync << SFILTER_DONTWAIT << " => * ;" ; // use SFILTER_DONTWAIT for everything
    string synch( sync.str() ); 

    ostringstream down;
    down << insertX_id << " => 0 ; " << insertY_id << " => 1 ; "
         << insertX_id << " => 2 ; " << insertY_id << " => 3 ; "
         << insertI_id << " => 4 ;" ;
    string downstream( down.str() );

    cout << "FE upstream filter assignments:\n" << upstream.c_str() << "\n"
         << "FE downstream filter assignments:\n" << downstream.c_str() << "\n\n";
 
    Stream * stream = net->new_Stream(comm_BC, upstream, synch, downstream);

    char * test_pong = NULL;
    char tmp_buf[256];

    if( stream->send( PROT_TEST, "%s", "" ) == -1 ){
        test->print("stream::send() failure\n", testname);
        test->end_SubTest(testname, FAILURE);
        delete net;
        return -1;
    }
    if( stream->flush() == -1 ){
        test->print("stream::flush() failure\n", testname);
        test->end_SubTest(testname, FAILURE);
        delete net;
        return -1;
    }

    for( int b=0; b < 4; b++ ) {

        retval = stream->recv(&tag, p);
        assert( retval != 0 ); //shouldn't be 0, either error or block till data
        if( retval == -1 ){
            test->print("stream::recv() failure\n", testname);
            test->end_SubTest(testname, FAILURE);
            delete net;
            return -1;
        }
        if( p->unpack( "%s", &test_pong ) == -1 ){
            test->print("stream::unpack() failure\n", testname);
            test->end_SubTest(testname, FAILURE);
            delete net;
            return -1;
        }
        
        sprintf( tmp_buf, "FE got %s\n", test_pong );
        free( test_pong );
        test->print( tmp_buf, testname );
    }

    test->end_SubTest(testname, SUCCESS);
    
    delete stream;
    delete net;

    return 0;
}
