/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "utils_lightweight.h"
#include "BackEndNode.h"
#include "ChildNode.h"
#include "PeerNode.h"
#include "Protocol.h"
#include "SerialGraph.h"

#include "mrnet_lightweight/Network.h"
#include "mrnet_lightweight/NetworkTopology.h"
#include "mrnet_lightweight/Packet.h"
#include "mrnet_lightweight/Stream.h"
#include "xplat_lightweight/vector.h"

int ChildNode_init_newChildDataConnection(BackEndNode_t* be, 
                                          PeerNode_t* iparent, 
                                          Rank ifailed_rank) 
{
    char *topo_ptr;
    Packet_t* packet;
    NetworkTopology_t* nettop;
    const char* fmt_str = "%s %uhd %ud %uhd %ud %c %s";
    int num_retry = 15;

    mrn_dbg_func_begin();

    mrn_dbg(5, mrn_printf(FLF, stderr,
                          "new parent rank=%d, hostname=%s, port=%d, ifailed_rank=%d\n",
                          iparent->rank, iparent->hostname, 
                          iparent->port, ifailed_rank));

    // establish data detection connect with new parent
    if( PeerNode_connect_DataSocket(iparent, num_retry) == -1 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "PeerNode_connect_data_socket() failed\n"));
        return -1;
    }

    be->incarnation++;

    topo_ptr = Network_get_LocalSubTreeStringPtr(be->network);
    mrn_dbg(5, mrn_printf(FLF, stderr, "prior topology: \"%s\"\n", 
                          topo_ptr));

    packet = new_Packet_t_2(CTL_STRM_ID, 
                            PROT_NEW_CHILD_DATA_CONNECTION, 
                            fmt_str, 
                            strdup(be->myhostname),
                            be->myport,
                            be->myrank, 
                            be->incarnation, 
                            ifailed_rank, 
                            'f', 
                            topo_ptr);
  
    mrn_dbg(5, mrn_printf(FLF, stderr, "Send initialization info...\n" ));
    if( PeerNode_sendDirectly(iparent, packet) == -1 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "send/flush() failed\n"));
        return -1;
    }
    Packet_set_DestroyData( packet, true );
    delete_Packet_t( packet );
     
    if(be->incarnation == 1) {
        vector_t* packets = new_empty_vector_t();
        int ret = PeerNode_recv( iparent, packets, true );
        if( (ret == -1) || ((ret ==0 ) && (packets->size == 0)) ) {
            if( ret == -1 ) {
                mrn_dbg( 3, mrn_printf(FLF, stderr,
                            "recv() topo and env failed! \n"));
                return -1;
            }
        }

        if( ChildNode_proc_PacketsFromParent( be, packets ) == -1 )
            mrn_dbg(1, mrn_printf(FLF, stderr, "proc_PacketsFromParent() failed\n"));
        delete_vector_t( packets );

        nettop = Network_get_NetworkTopology(be->network);
        topo_ptr = NetworkTopology_get_TopologyStringPtr(nettop);
        if( topo_ptr != NULL ) {
            mrn_dbg(5, mrn_printf(FLF, stderr, "new topology is \"%s\"\n",
                        topo_ptr));
            free( topo_ptr );
        }
    }

    mrn_dbg_func_end();

    return 0;
}

int ChildNode_init_newChildEventConnection(BackEndNode_t* be, 
                                           PeerNode_t* iparent) 
{
    int retval = 0;
    Packet_t* packet;
    const char* fmt_str = "%s %uhd %ud";
    int num_retry = 5;

    mrn_dbg_func_begin();

    // establish data detection connect with new parent
    if( PeerNode_connect_EventSocket(iparent, num_retry) == -1 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "PeerNode_connect_data_socket() failed\n"));
        return -1;
    }

    packet = new_Packet_t_2(CTL_STRM_ID, 
                            PROT_NEW_CHILD_FD_CONNECTION, 
                            fmt_str,
                            strdup(be->myhostname),
                            be->myport,
                            be->myrank);
  
    mrn_dbg(5, mrn_printf(FLF, stderr, "Initializing new Child FD Connection\n\n" ));
  
    iparent->msg_out.packet = packet;  
    if( Message_send(&(iparent->msg_out), iparent->event_sock_fd) == -1 ) { 
        mrn_dbg(1, mrn_printf(FLF, stderr, "Message_send() failed\n"));
        retval = -1;
    }

    Packet_set_DestroyData( packet, true );
    delete_Packet_t( packet );
     
    mrn_dbg_func_end();

    return retval;
}

int ChildNode_send_SubTreeInitDoneReport(BackEndNode_t* be)
{
    Packet_t * packet;
	
    mrn_dbg_func_begin();

    packet = new_Packet_t_2(CTL_STRM_ID, PROT_SUBTREE_INITDONE_RPT, NULL_STRING);

    if (packet) {
        if (PeerNode_sendDirectly(Network_get_ParentNode(be->network), packet) == -1) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "send/flush failed\n"));
            return false;
        }
    } else {
        mrn_dbg(1, mrn_printf(FLF, stderr, "new packet() failed\n"));
        return false;
    }

    delete_Packet_t( packet );
    mrn_dbg_func_end();
    return true;
}

int ChildNode_proc_PacketsFromParent(BackEndNode_t* be, vector_t* packets)
{
    int retval = 0;
    unsigned int i;
    Packet_t * cur_packet;

    mrn_dbg_func_begin();

    for (i = 0; i < packets->size; i++) {
        cur_packet = (Packet_t*)packets->vec[i];
        mrn_dbg(5, mrn_printf(FLF, stderr, "tag is %d\n", cur_packet->tag)); 
        if (ChildNode_proc_PacketFromParent(be, cur_packet) == -1)
            retval = -1;
    }
    
    mrn_dbg(3, mrn_printf(FLF, stderr, "ChildNode_proc_Packets() %s", 
                (retval == -1 ? "failed\n" : "succeeded\n")));

    return retval;
}

int ChildNode_proc_PacketFromParent(BackEndNode_t* be, Packet_t* packet)
{
    int tag, retval = 0;

    mrn_dbg_func_begin();
 
    tag = packet->tag;

    mrn_dbg(5, mrn_printf(FLF, stderr, 
                          "ChildNode_proc_PacketFromParent, packet->tag = %d\n", 
                          tag));

    if( (tag > PROT_FIRST) && (tag < PROT_LAST) ) {

        switch( tag ) {

        case PROT_LAUNCH_SUBTREE:
            // BEs don't launch anything, ignore
            break;

        //case PROT_EDT_SHUTDOWN:
        case PROT_EDT_REMOTE_SHUTDOWN:
        //case PROT_SHUTDOWN:
            
            mrn_dbg(5, mrn_printf(FLF, stderr,"Shutting Down\n"));
            if (BackEndNode_proc_DeleteSubTree(be, packet) == -1) {
                mrn_dbg(1, mrn_printf(FLF, stderr,"BackEndNode_proc_deleteSubTree() failed\n"));
                retval = -1;
            }
            break;

        case PROT_NEW_STREAM:
        case PROT_NEW_HETERO_STREAM:
        case PROT_NEW_INTERNAL_STREAM:
            if(BackEndNode_proc_newStream(be, packet) == -1) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_newStream() failed\n" ));
                retval = -1;
            }
            break;
          
        case PROT_SET_FILTERPARAMS_UPSTREAM_SYNC:
        case PROT_SET_FILTERPARAMS_UPSTREAM_TRANS:
            if( BackEndNode_proc_UpstreamFilterParams(be, packet) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_UpstreamFilterParams() failed\n" ));
                retval = -1;
            }
            break;

        case PROT_SET_FILTERPARAMS_DOWNSTREAM:
            if( BackEndNode_proc_DownstreamFilterParams(be, packet) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_DownstreamFilterParams() failed\n" ));
                retval = -1;
            }
            break;

        case PROT_DEL_STREAM:
            /* We're always a leaf node in the lightweight library, 
               so don't have to do the InternalNode check */
            if (BackEndNode_proc_deleteStream(be, packet) == -1) {
                mrn_dbg(1, mrn_printf(FLF, stderr, "proc_deleteStream() failed\n"));
                retval = -1;
            }
            break;

        case PROT_NEW_FILTER:
            if (BackEndNode_proc_newFilter(be, packet) == -1) {
                mrn_dbg(1, mrn_printf(FLF, stderr, "proc_deleteStream() failed\n"));
                retval = -1;
            }
            //mrn_dbg(5, mrn_printf(FLF, stderr, "lightweight BE ignoring new filter\n"));
            break;

        case PROT_RECOVERY_RPT:
            if( ChildNode_proc_RecoveryReport(be, packet) == -1 ){
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "proc_RecoveryReport() failed\n" ));
                retval = -1;
            }
            break;
        case PROT_ENABLE_PERFDATA:
            if( ChildNode_proc_EnablePerfData(be, packet) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "proc_CollectPerfData() failed\n" ));
                retval = -1;
            }
            break;
        case PROT_DISABLE_PERFDATA:
            if( ChildNode_proc_DisablePerfData(be, packet) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "proc_CollectPerfData() failed\n" ));
                retval = -1;
            }
            break;
        case PROT_COLLECT_PERFDATA:
            if( ChildNode_proc_CollectPerfData(be, packet) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "proc_CollectPerfData() failed\n" ));
                retval = -1;
            }
            break;
        case PROT_PRINT_PERFDATA:
            if( ChildNode_proc_PrintPerfData(be, packet) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "proc_PrintPerfData() failed\n" ));
                retval = -1;
            }
            break;
        case PROT_PORT_UPDATE:
            if (ChildNode_proc_PortUpdate(be, packet) == -1 ) {
                mrn_dbg(1, mrn_printf(FLF, stderr,
                                      "proc_PortUpdate() failed\n"));
                retval = -1;
            }
            break;
        case PROT_TOPO_UPDATE:
            if (ChildNode_proc_TopologyUpdates(be, packet) == -1 ) {
                mrn_dbg(1, mrn_printf(FLF, stderr,
                                      "proc_TopologyUpdates() failed\n"));
                retval = -1;
            }
            break;
        case PROT_ENABLE_RECOVERY:
            if( ChildNode_proc_EnableFailReco(be) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "proc_EnableFailReco() failed\n" ));
                retval = -1;
            }
            break;
        case PROT_DISABLE_RECOVERY:
            if( ChildNode_proc_DisableFailReco(be) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "proc_DisableFailReco() failed\n" ));
                retval = -1;
            }
            break;
        case PROT_NET_SETTINGS:
            if( ChildNode_proc_NetworkSettings(be, packet) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "proc_SetTopoEnv() failed\n" ));
                retval = -1;
            }
            break;
        default:
            mrn_dbg( 3, mrn_printf(FLF, stderr,
                                   "internal protocol tag %d is unhandled\n", tag) );
            break;
        }
        Packet_set_DestroyData( packet, true );
        delete_Packet_t( packet );
    }
    else if( tag >= FirstApplicationTag ) {
        if( BackEndNode_proc_DataFromParent(be, packet) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "BackEndNode_proc_DataFromParent() failed\n") );
            retval = -1;
        }
    }
    else {
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                               "tag %d is invalid\n", tag) );
        retval = -1;
        Packet_set_DestroyData( packet, true );
        delete_Packet_t( packet );
    }

    return retval;
}

int ChildNode_proc_NetworkSettings( BackEndNode_t* be, Packet_t* ipacket ) 
{
    char* sg_byte_array = NULL;
    int* keys = NULL;
    char** vals = NULL ;
    uint32_t i, count;
    SerialGraph_t* parent_sg = NULL;
    SerialGraph_t* my_subtree = NULL;
    SerialGraph_t* subtree_in_parent = NULL;
    NetworkTopology_t* nt;
    Node_t* my_root_node;
    Node_t* parent_node;
    PeerNode_t* parent;
    Stream_t* topol_strm;
    char* myhost;
    char* my_sg_str;
    int gen_topol_attach = 0;
    Rank myrank, parrank, parent_topol_root, my_topol_root;
    Port myport;

    mrn_dbg_func_begin();
   
    nt = Network_get_NetworkTopology( be->network );

    if( Packet_unpack(ipacket, "%s %ad %as", 
                      &sg_byte_array, 
                      &keys, &count, 
                      &vals, &count) == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "unpack env packet failed\n" ));
        return -1;
    }

    // update topology based on parent's topology (note: parent topology from BEFORE adding my subtree)
    myhost = Network_get_LocalHostName( be->network );
    myport = Network_get_LocalPort( be->network );
    myrank = Network_get_LocalRank( be->network );

    parent = Network_get_ParentNode( be->network );
    parrank = PeerNode_get_Rank( parent );

    parent_sg = new_SerialGraph_t( sg_byte_array );
    if( sg_byte_array != NULL )
        free( sg_byte_array );
    subtree_in_parent = SerialGraph_get_MySubTree( parent_sg, myhost, myport, myrank );  
    if( NULL == subtree_in_parent ) { 
        // I wasn't in parent's topology, must be BE/CP attach
        gen_topol_attach = 1;
    }   
    
    my_sg_str = NetworkTopology_get_LocalSubTreeStringPtr( nt );
    my_subtree = new_SerialGraph_t( my_sg_str );

    my_root_node = NetworkTopology_get_Root( nt );
    my_topol_root = NetworkTopology_Node_get_Rank( my_root_node );
    parent_topol_root = SerialGraph_get_RootRank( parent_sg );
    if( my_topol_root != parent_topol_root ) { 
        // reset topology to parent's previous topology, then add my subtree
        NetworkTopology_reset( nt, parent_sg );
        parent_node = NetworkTopology_find_Node( nt, parrank );
        NetworkTopology_add_SubGraph( nt, parent_node, my_subtree, true );  
    }   
    else {
        // roots are the same, so we assume the rest of the topology is as well
        mrn_dbg( 5, mrn_printf(FLF, stderr, "current and parent's previous topology root match\n") );
    }   

    // BE attach: generate topology update for local rank
    if( gen_topol_attach ) { 
        mrn_dbg( 3, mrn_printf(FLF, stderr, "not in parent's previous topology, generating topology update\n") );
        int type = TOPO_NEW_BE;
        uint32_t send_iprank = parrank;
        uint32_t send_myrank = myrank;
        uint16_t send_port = myport;
        topol_strm = Network_get_Stream( be->network, TOPOL_STRM_ID );
        Stream_send( topol_strm, PROT_TOPO_UPDATE, "%ad %aud %aud %as %auhd",
                     &type, 1, &send_iprank, 1, &send_myrank, 1,
                     &myhost, 1, &send_port, 1 );
    }

    // init other network settings
    for( i=0; i < count; i++ ) {

        switch ( keys[i] ) {
        case MRNET_DEBUG_LEVEL :
            Network_set_OutputLevel( atoi(vals[i]) );        
            break;
	   
        case MRNET_DEBUG_LOG_DIRECTORY :
            Network_set_DebugLogDir( vals[i] );
            break;
               
        case MRNET_FAILURE_RECOVERY :
            if( strcmp(vals[i], "0") == 0 )
                Network_disable_FailureRecovery( be->network );
            break;	   

        default : 
            break;
        }

        free( vals[i] );
    }

    free( vals );
    free( keys );

    mrn_dbg_func_end();
    return 0;
}

int ChildNode_ack_ControlProtocol(BackEndNode_t* be, int ack_tag, char success)
{
    Packet_t* packet;
    char succ_byte;
    
    mrn_dbg_func_begin();
    succ_byte = success;
    packet = new_Packet_t_2(CTL_STRM_ID, ack_tag, "%c", succ_byte);
   
    if( packet != NULL ) {
        if((PeerNode_sendDirectly(be->network->parent, packet) == -1 ) ||
           (PeerNode_flush(be->network->parent) == -1)) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "send failed\n"));
            return false;
        }
    }
    else {
        mrn_dbg(1, mrn_printf(FLF, stderr, "new packet() failed\n"));
        return false;
    } 
  
    delete_Packet_t( packet );

    mrn_dbg_func_end();
    return true;
}

int ChildNode_ack_DeleteSubTree(BackEndNode_t* be)
{
    mrn_dbg_func_begin();
    return ChildNode_ack_ControlProtocol(be, PROT_SHUTDOWN_ACK, (char)1);
}

int ChildNode_proc_RecoveryReport(BackEndNode_t* be, Packet_t* ipacket)
{
    Rank child_rank, failed_parent_rank, new_parent_rank;
  
    mrn_dbg_func_begin();

    Packet_unpack(ipacket, "%ud %ud %ud",
                  &child_rank,
                  &failed_parent_rank,
                  &new_parent_rank);
    // remove node, but don't update datastructs since following procedure will
    Network_remove_Node(be->network, failed_parent_rank, false);
    Network_change_Parent(be->network, child_rank, new_parent_rank);

    mrn_dbg_func_end();
    return 0;
}

 int ChildNode_proc_EnablePerfData(BackEndNode_t* be, Packet_t* ipacket)
 {
     unsigned int stream_id;
     Stream_t* strm;
     int metric, context;

     mrn_dbg_func_begin();

     stream_id = ipacket->stream_id;
     strm = Network_get_Stream(be->network, stream_id);
     if (strm == NULL) {
         mrn_dbg(1, mrn_printf(FLF, stderr, "stream %u lookup failed\n", stream_id));
         return -1;
     } 

     //local update
     Packet_unpack(ipacket, "%d %d", &metric, &context);
     Stream_enable_PerfData(strm, (perfdata_metric_t)metric, (perfdata_context_t)context);

     mrn_dbg_func_end();
     return 0;
}

int ChildNode_proc_DisablePerfData(BackEndNode_t* be, Packet_t* ipacket)
{
    unsigned int stream_id;
    Stream_t* strm;
    int metric, context;

    mrn_dbg_func_begin();

    stream_id = ipacket->stream_id;
    strm = Network_get_Stream(be->network, stream_id);
    if (strm == NULL) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "stream %u lookup failed\n", stream_id));
        return -1;
    }

    // local update
    Packet_unpack(ipacket, "%d %d", &metric, &context);
    Stream_disable_PerfData(strm, (perfdata_metric_t)metric, (perfdata_context_t)context);

    mrn_dbg_func_end();
    return 0;
}

int ChildNode_proc_CollectPerfData(BackEndNode_t* be, Packet_t* ipacket)
{
    unsigned int stream_id, aggr_strm_id;
    Stream_t* strm;
    int metric, context;
    Packet_t* pkt;
    Stream_t* aggr_strm;

    mrn_dbg_func_begin();

    stream_id = ipacket->stream_id;
    strm = Network_get_Stream(be->network, stream_id);
    if (strm == NULL) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "stream %u lookup failed\n", stream_id));
        return -1;
    }

    Packet_unpack(ipacket, "%d %d %ud", &metric, &context, &aggr_strm_id);

    // collect
    pkt = Stream_collect_PerfData(strm, 
                                  (perfdata_metric_t)metric, 
                                  (perfdata_context_t)context, 
                                  aggr_strm_id);

    // send
    aggr_strm = Network_get_Stream(be->network, aggr_strm_id);
    if (aggr_strm == NULL) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "aggr stream %u lookup failed\n", aggr_strm_id));
        return -1;
    }

    Stream_send_aux(aggr_strm, pkt->tag, pkt->fmt_str, pkt);

    mrn_dbg_func_end();
    return 0;
}

int ChildNode_proc_PrintPerfData(BackEndNode_t* be, Packet_t* ipacket)
{
    unsigned int stream_id;
    Stream_t* strm;
    int metric, context;

    mrn_dbg_func_begin();

    stream_id = ipacket->stream_id;
    strm = Network_get_Stream(be->network, stream_id);
    if (strm == NULL) { 
        mrn_dbg (1, mrn_printf(FLF,stderr, "stream %u lookup failed\n", stream_id));
        return -1;
    }

    // local print
    Packet_unpack(ipacket, "%d %d", &metric, &context);
    Stream_print_PerfData(strm, (perfdata_metric_t)metric, (perfdata_context_t)context); 

    mrn_dbg_func_end();
    return 0;
}

int ChildNode_proc_PortUpdate(BackEndNode_t * be,
                              Packet_t* UNUSED(ipacket))
{
    Stream_t* s;

    /* these must be heap allocated, since Stream_send() frees
       packets sent on non-user streams */
    int32_t* type = (int32_t*) malloc( sizeof(int32_t) );
    char** host_arr = (char**) malloc( sizeof(char*) );
    uint32_t* send_iprank = (uint32_t*) malloc( sizeof(uint32_t) );
    uint32_t* send_myrank = (uint32_t*) malloc( sizeof(uint32_t) );
    uint16_t* send_port = (uint16_t*) malloc( sizeof(uint16_t) );

    mrn_dbg_func_begin();
    
    // send update for my port
    s = Network_get_Stream(be->network, PORT_STRM_ID); // get port update stream

    type[0] = TOPO_CHANGE_PORT;
    host_arr[0] = strdup(NULL_STRING);
    send_iprank[0] = UnknownRank;
    send_myrank[0] = Network_get_LocalRank(be->network);
    send_port[0] = Network_get_LocalPort(be->network);
    
    Stream_send(s, PROT_TOPO_UPDATE, "%ad %aud %aud %as %auhd",
                type, 1, 
                send_iprank, 1, 
                send_myrank, 1, 
                host_arr, 1, 
                send_port, 1);

    mrn_dbg_func_end();
    return 0;
}

int ChildNode_proc_TopologyUpdates(BackEndNode_t * be,
                                   Packet_t* ipacket)
{
    // topology stream is a real stream, so process as stream data
    if( BackEndNode_proc_DataFromParent(be, ipacket) == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                               "BackEndNode_proc_DataFromParent() failed\n") );
        return -1;
    }
    return 0;
}


/*Failure Recovery Code*/

int ChildNode_proc_EnableFailReco(BackEndNode_t * be)
{
    be->network->recover_from_failures = 1;
    return 0;
}

int ChildNode_proc_DisableFailReco(BackEndNode_t * be)
{
    be->network->recover_from_failures = 0;
    return 0;
}
