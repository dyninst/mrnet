/****************************************************************************
 *  Copyright 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdlib.h>
#include <assert.h>

#include "BackEndNode.h"
#include "ChildNode.h"
#include "FilterDefinitions.h"
#include "PeerNode.h"
#include "Protocol.h"
#include "utils_lightweight.h"

#include "mrnet_lightweight/Network.h"
#include "mrnet_lightweight/NetworkTopology.h"
#include "mrnet_lightweight/Stream.h"
#include "mrnet_lightweight/Types.h"
#include "xplat_lightweight/NetUtils.h"
#include "xplat_lightweight/SocketUtils.h"

Port defaultTopoPort = 26500;

BackEndNode_t* new_BackEndNode_t(Network_t* inetwork, 
                                 char* imyhostname,
                                 Rank imyrank,
                                 char* iphostname,
                                 Port ipport,
                                 Rank iprank)
{
    BackEndNode_t* be;
    PeerNode_t* parent;
    NetworkTopology_t* nt;

    Stream_t* s;
    char** host_arr;
    int32_t* type;
    uint32_t* send_iprank;
    uint32_t* send_myrank;
    uint16_t* send_port;

    be = (BackEndNode_t*) calloc( (size_t)1, sizeof(BackEndNode_t) );
    assert(be != NULL);
    be->network = inetwork;
    be->myhostname = strdup(imyhostname);
    be->myrank = imyrank;
    be->myport = UnknownPort;
    be->phostname = strdup(iphostname);
    be->pport = ipport;
    be->prank = iprank;
    be->incarnation = 0;

    parent = Network_new_PeerNode(inetwork, 
                                  iphostname,
                                  ipport, 
                                  iprank, 
                                  true, 
                                  true);

    Network_set_ParentNode(inetwork, parent);

    inetwork->local_hostname = imyhostname;
    inetwork->local_rank = imyrank;
    inetwork->local_back_end_node = be;

    nt = new_NetworkTopology_t(inetwork, imyhostname, 
                               (Port)UnknownPort, imyrank, true);
    inetwork->network_topology = nt;

    // establish data connection with parent
    if( ChildNode_init_newChildDataConnection(be, inetwork->parent, 
                                              UnknownRank) == -1 ) {
        mrn_dbg (1, mrn_printf(FLF, stderr,
                    "init_newChildDataConnection() failed\n"));
        return NULL;
    }

    // get it again, since it may have been updated
    nt = Network_get_NetworkTopology(inetwork);
    if( nt != NULL ) {

        if( ! NetworkTopology_isInTopology(nt, imyhostname, UnknownPort, imyrank) ) {
            // back-end attach case
            mrn_dbg( 5, mrn_printf(FLF, stderr, "Backend not already in the topology\n") );

            // send a topology update
            s = Network_new_Stream(inetwork, TOPOL_STRM_ID, NULL, 0, 0, 0, 0);

            /* these must be heap allocated, since Stream_send() frees
               packets sent on non-user streams */
            host_arr = (char**) malloc( sizeof(char*) );
            type = (int32_t*) malloc( sizeof(int32_t) );
            send_iprank = (uint32_t*) malloc( sizeof(uint32_t) );
            send_myrank = (uint32_t*) malloc( sizeof(uint32_t) );
            send_port = (uint16_t*) malloc( sizeof(uint16_t) );

            type[0] = TOPO_NEW_BE;
            send_iprank[0] = iprank;
            send_myrank[0] = imyrank;
            host_arr[0] = strdup(imyhostname);
            send_port[0] = UnknownPort;

            Stream_send(s, PROT_TOPO_UPDATE, "%ad %aud %aud %as %auhd",
                        type, 1, 
                        send_iprank, 1, 
                        send_myrank, 1, 
                        host_arr, 1, 
                        send_port, 1);
        } else {
            mrn_dbg( 5, mrn_printf(FLF, stderr, "Backend already in the topology\n") );
        }

        if( ChildNode_send_SubTreeInitDoneReport(be) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "ChildNode_send_SubTreeInitDoneReport() failed\n") );
        }
    } 

    // establish event connection with parent
    if( ChildNode_init_newChildEventConnection(be, inetwork->parent) == -1 ) {
        mrn_dbg (1, mrn_printf(FLF, stderr,
                    "init_newChildEventConnection() failed\n"));
        return NULL;
    }

    return be; 
}

void delete_BackEndNode_t( BackEndNode_t* be )
{
    if( be != NULL ) {
        if( be->myhostname != NULL )
	    free( be->myhostname );
        if( be->phostname != NULL )
	    free( be->phostname );
        free( be );
    }
}

BackEndNode_t* CreateBackEndNode( Network_t* inetwork,
                                  char* imy_hostname,
                                  Rank imy_rank,
                                  char* iphostname,
                                  Port ipport,
                                  Rank iprank ) 
{
    // create the new backend node
    BackEndNode_t* be = new_BackEndNode_t(inetwork, imy_hostname, imy_rank, 
                                          iphostname, ipport, iprank); 

    return be;
}

int BackEndNode_proc_DeleteSubTree(BackEndNode_t* be, Packet_t* UNUSED(packet) )
{
    (void) packet;
    mrn_dbg_func_begin();

    // processes will be exiting -- disable failure recovery
    Network_disable_FailureRecovery(be->network);

    // Send ack to parent
    if( ! ChildNode_ack_DeleteSubTree(be) ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "ChildNode_ack_DeleteSubTree() failed\n"));
    }
    
    // close event connection
    if( be->network->parent->event_sock_fd != 0 )
        XPlat_SocketUtils_Close(be->network->parent->event_sock_fd);
  
    // kill topology  
    Network_shutdown_Network(be->network);

    mrn_dbg_func_end();
    return 0;
}

int BackEndNode_proc_newStream(BackEndNode_t* be, Packet_t* packet)
{
    uint32_t num_backends;
    Rank *backends;
    unsigned int stream_id;
    int tag;
    /* Safe since filters are not used in lightweight */
    int ds_filter_id = 0, us_filter_id = 0, sync_id = 0;
    char* us_filters;
    char* sync_filters;
    char* ds_filters;
    Rank me;

    mrn_dbg_func_begin();

    tag = packet->tag;

    if (tag == PROT_NEW_HETERO_STREAM) {
        me = be->network->local_rank;

        if (Packet_unpack(packet, "%d %ad %s %s %s",
            &stream_id, &backends, &num_backends, 
            &us_filters, &sync_filters, &ds_filters) == -1) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "Packet_unpack() failed\n"));
            return -1;
        }
  
        if (!Stream_find_FilterAssignment(us_filters, me, us_filter_id)) {
            mrn_dbg(3, mrn_printf(FLF, stderr, 
                                  "Stream_find_FilterAssignment(upstream) failed, using default\n"));
            us_filter_id = TFILTER_NULL;
        }
    
        if (!Stream_find_FilterAssignment(ds_filters, me, ds_filter_id)) {
            mrn_dbg(3, mrn_printf(FLF, stderr, 
                                  "Stream_find_FilterAssignment(downstream) failed, using default\n"));
            ds_filter_id = TFILTER_NULL;
        }
        if (!Stream_find_FilterAssignment(sync_filters, me, sync_id)) {
            mrn_dbg(3, mrn_printf(FLF, stderr, 
                                  "Stream_find_FilterAssignment(sync) failed, using default\n"));
            sync_id = SFILTER_WAITFORALL;
        }
    }

    else { // PROT_NEW_STREAM or PROT_NEW_INTERNAL_STREAM
        if (Packet_unpack(packet, "%ud %ad %d %d %d", 
                          &stream_id, &backends,&num_backends,
                          &us_filter_id, &sync_id, &ds_filter_id) == -1) 
        {
            mrn_dbg(1, mrn_printf(FLF, stderr, "Packet_unpack() failed\n"));
            return -1;
        }
    }

    Network_new_Stream(be->network, stream_id, backends, num_backends,
                       us_filter_id, sync_id, ds_filter_id);

    if( backends != NULL )
        free( backends );

    if (tag == PROT_NEW_INTERNAL_STREAM) {
        // Send ack to parent
        if( ! ChildNode_ack_ControlProtocol(be, PROT_NEW_STREAM_ACK) ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, 
                                  "ChildNode_ack_ControlProtocol() failed\n"));
        }
    }

    mrn_dbg_func_end();

    return 0;
}

int BackEndNode_proc_UpstreamFilterParams(BackEndNode_t* UNUSED(be), 
                                          Packet_t* UNUSED(ipacket))
{
    // no filter support in lightweight BE, nothing to do
    return 0;  
}

int BackEndNode_proc_DownstreamFilterParams(BackEndNode_t* UNUSED(be), Packet_t* UNUSED(ipacket))
{
    // no filter support in lightweight BE, nothing to do
    return 0;
}

int BackEndNode_proc_deleteStream(BackEndNode_t* be, Packet_t* ipacket)
{
    unsigned int stream_id;
    Stream_t * strm;
    
    mrn_dbg_func_begin();

    if (Packet_unpack(ipacket, "%ud", &stream_id) == -1) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Packet_unpack() failed\n"));
        return -1;
    }
    
    strm = Network_get_Stream(be->network, stream_id);
    if (strm == NULL) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "stream %u lookup failed\n", stream_id));
        return -1;
    }

    if( ! Network_is_UserStreamId(stream_id) ) {
        // kill internal streams
        delete_Stream_t( strm );
    }
    else {
        // close user streams
        strm->_was_closed = 1;
    }

    mrn_dbg_func_end();
    return 0;
}

int BackEndNode_proc_newFilter(BackEndNode_t* UNUSED(be), Packet_t* UNUSED(ipacket))
{
    // no filter support in lightweight BE, nothing to do
    return 0;
}

int BackEndNode_proc_DataFromParent(BackEndNode_t* be, Packet_t* ipacket)
{
    Stream_t* stream;
    Packet_t* opacket;
    vector_t * opackets = new_empty_vector_t();
    vector_t * opackets_reverse = new_empty_vector_t();
    unsigned int i;

    mrn_dbg_func_begin();

    stream = Network_get_Stream(be->network, ipacket->stream_id);

    if (stream == NULL) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "stream %u lookup failed\n", 
                              ipacket->stream_id));
        return -1;
    }

    if (Stream_push_Packet(stream, ipacket, opackets, opackets_reverse, false) == -1) {
        return -1;
    }

    for (i = 0; i < opackets->size; i++) {
        opacket = opackets->vec[i];
        Packet_set_DestroyData( opacket, true );
        mrn_dbg(5, mrn_printf(FLF, stderr, "adding Packet on stream[%u]\n", 
                              stream->id));
        Stream_add_IncomingPacket(stream, opacket);
    }

    delete_vector_t( opackets );
    delete_vector_t( opackets_reverse );
  
    mrn_dbg_func_end();

    return 0;
}
