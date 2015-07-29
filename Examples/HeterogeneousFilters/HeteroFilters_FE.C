/**************************************************************************
 * Copyright 2003-2015   Michael J. Brim, Barton P. Miller                *
 *                Detailed MRNet usage rights in "LICENSE" file.          *
 **************************************************************************/

#include <iostream>
#include <sstream>
#include <string>

#include "mrnet/MRNet.h"
#include "HeteroFilters.h"

using namespace MRN;

bool assign_filters( NetworkTopology* nettop, 
                     int be_filter, int cp_filter, int fe_filter, 
                     std::string& up )
{
    ostringstream assignment;

    // assign FE
    NetworkTopology::Node* root = nettop->get_Root();
    assignment << fe_filter << " => " << root->get_Rank() << " ; ";

    // assign BEs
    std::set< NetworkTopology::Node * > bes;
    nettop->get_BackEndNodes(bes);
    std::set< NetworkTopology::Node* >::iterator niter = bes.begin();
    for( ; niter != bes.end(); niter++ ) {
        assignment << be_filter << " => " << (*niter)->get_Rank() << " ; ";
    }

    // assign CPs (if any) using '*', which means everyone not already assigned
    assignment << cp_filter << " => * ; ";

    up = assignment.str();
    
    return true;
}

int main( int argc, char ** argv )
{
    int tag, retval;
    PacketPtr p;

    if( argc != 6 ){
        fprintf( stderr, "Usage: %s <search_string> <search_file> <topology_file> <backend_exe> <so_file>\n", argv[0] );
        exit(-1);
    }
    const char * search_str = argv[1];
    const char * search_file = argv[2];
    const char * topology_file = argv[3];
    const char * backend_exe = argv[4];
    const char * so_file = argv[5];
    const char * empty_argv = NULL;

    Network * net = Network::CreateNetworkFE( topology_file, backend_exe, &empty_argv );
    if( net->has_Error() ) {
        net->perror( "network creation failed" );
        return -1;
    }
    NetworkTopology * nettop = net->get_NetworkTopology();
    Communicator * comm_BC = net->get_BroadcastCommunicator( );

    int be_filter_id = net->load_FilterFunc( so_file, BE_filter_scan );
    if( be_filter_id == -1 ){
        fprintf( stderr, "ERROR: failed to load %s from library %s\n", BE_filter_scan, so_file );
        delete net;
        return -1;
    }
    int cp_filter_id = net->load_FilterFunc( so_file, CP_filter_sort );
    if( cp_filter_id == -1 ){
        fprintf( stderr, "ERROR: failed to load %s from library %s\n", CP_filter_sort, so_file );
        delete net;
        return -1;
    }
    int fe_filter_id = net->load_FilterFunc( so_file, FE_filter_uniq );
    if( fe_filter_id == -1 ){
        fprintf( stderr, "ERROR: failed to load %s from library %s\n", FE_filter_uniq, so_file );
        delete net;
        return -1;
    }
    printf("FE: Loaded custom filters\n");

    // use default (TFILTER_NULL) filter for downstream
    std::string down = "";

    // use default (SFILTER_DONTWAIT) filter for upstream synchronization
    char assign[16];
    sprintf(assign, "%d => *;", SFILTER_WAITFORALL);
    std::string sync = assign;

    // use custom BE/CP/FE upstream data filters
    std::string up;
    if( ! assign_filters(nettop, be_filter_id, cp_filter_id, fe_filter_id, up) ) {
        fprintf( stderr, "ERROR: generate_filter_assignments() failed\n");
        delete net;
        return -1;
    }
    printf("FE: Generated filter assignments\n");

    // tell BEs to open local files and determine max # blocks
    Stream * init_stream = net->new_Stream(comm_BC, TFILTER_MAX);
    if( init_stream->send(PROT_INIT, "%s %d", search_file, 4096) == -1 ) {
        fprintf( stderr, "ERROR: init_stream::send() failure\n");
        delete net;
        return -1;
    }
    init_stream->flush();
    int rc = init_stream->recv(&tag, p);
    if( rc == -1 ) {
        fprintf( stderr, "ERROR: init_stream::recv() failure\n");
        delete net;
        return -1;
    }
    assert(tag == PROT_INIT);
    int max_num_blocks = 0;
    p->unpack("%d", &max_num_blocks);

    // tell BEs to start sending data (max_num_blocks waves expected);
    Stream * search_stream = net->new_Stream(comm_BC, up, sync, down);
    search_stream->set_FilterParameters(FILTER_UPSTREAM_TRANS, "%s", search_str);
    if( search_stream->send(PROT_SEARCH, "%d", max_num_blocks) == -1 ) {
        fprintf( stderr, "ERROR: multi_stream::send() failure\n");
        delete net;
        return -1;
    }
    search_stream->flush();
    
    // receive unique, sorted matches
    for( int i=0; i < max_num_blocks; i++ ) {

        rc = search_stream->recv(&tag, p);
        if( rc == -1 ) {
            fprintf( stderr, "ERROR: multi_stream::recv() failure\n");
            delete net;
            return -1;
        }
        assert(tag == PROT_SEARCH);

        char** strArr = NULL;
        unsigned arrLen = 0;
        p->unpack("%as", &strArr, &arrLen);

        if( arrLen == 0 ) continue;

        for( unsigned u=0; u < arrLen; u++ ) {
            printf("%s", strArr[u]);
            free( strArr[u] );
        }
        fflush(stdout);
        free( strArr );   
    }
    
    Stream * exit_stream = net->new_Stream(comm_BC, TFILTER_MAX);
    if( exit_stream->send(PROT_EXIT, "") == -1 )
        fprintf( stderr, "ERROR: exit_stream::send() failure\n");
    exit_stream->flush();

    tag = 0;
    while( tag != PROT_EXIT ) {
        retval = exit_stream->recv(&tag, p);
        if( retval == -1 )
            fprintf( stderr, "ERROR: exit_stream::recv() failure\n");        
    }
    delete init_stream;
    delete search_stream;
    delete exit_stream;
    delete net;

    return 0;
}
