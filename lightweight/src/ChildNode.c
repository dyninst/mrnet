/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <assert.h>

#include "mrnet_lightweight/Network.h"
#include "BackEndNode.h"
#include "ChildNode.h"
#include "mrnet_lightweight/NetworkTopology.h"
#include "mrnet_lightweight/Packet.h"
#include "PeerNode.h"
#include "Protocol.h"
#include "mrnet_lightweight/Stream.h"
#include "mrnet_lightweight/Types.h"
#include "utils_lightweight.h"
#include "vector.h"

int ChildNode_init_newChildDataConnection (BackEndNode_t* be, 
                                            PeerNode_t* iparent, 
                                            Rank ifailed_rank) 
{
    char* topo_ptr;
    char* fmt_str;
    Packet_t* packet;

    mrn_dbg_func_begin();

    mrn_dbg(5, mrn_printf(FLF, stderr, "new parent rank=%d, hostname=%s, port=%d, ifailed_rank=%d\n",  iparent->rank, iparent->hostname, iparent->port, ifailed_rank));

    // establish data detection connect with new parent
    if (PeerNode_connect_DataSocket(iparent) == -1 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "PeerNode_connect_data_socket() failed\n"));
        return -1;
    }

    be->incarnation++;

    topo_ptr = Network_get_LocalSubTreeStringPtr(be->network);

    mrn_dbg(5, mrn_printf(FLF, stderr, "topology: (%p), \"%s\"\n", topo_ptr, topo_ptr));

    fmt_str = "%s %uhd %ud %uhd %ud %c %s"; 

    packet = new_Packet_t_2 (0, 
          PROT_NEW_CHILD_DATA_CONNECTION, 
          fmt_str, 
          be->myhostname,
          be->myport,
          be->myrank, 
          be->incarnation, 
          ifailed_rank, 
          'f', 
          topo_ptr); 
  
    mrn_dbg(5, mrn_printf(FLF, stderr, "Send initialization info...\n" ));

    if ( PeerNode_sendDirectly(be->network->parent, packet) == -1) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "send/flush() failed\n"));
        return -1;
    } 

    mrn_dbg_func_end();

    return 0;
}

int ChildNode_send_NewSubTreeReport(BackEndNode_t* be)
{
    char* topo_ptr;
    Packet_t* packet;
    
    mrn_dbg_func_begin();

  // previously, mutex was here to prevent subtree overwriting;
  // is this no longer necessary because we are only registering
  // leaves and not internal subtrees?

  topo_ptr = NetworkTopology_get_LocalSubTreeStringPtr(be->network->network_topology);
  packet = new_Packet_t_2(0, PROT_NEW_SUBTREE_RPT, "%s", topo_ptr);

  if (packet) {
    if (PeerNode_sendDirectly(be->network->parent, packet) == -1) {
      mrn_dbg(1, mrn_printf(FLF, stderr, "send/flush failed\n"));
      return -1;
    }
  }
  else {
    mrn_dbg(1, mrn_printf(FLF, stderr, "new packet() failed\n"));
    return -1;
  }

  mrn_dbg_func_end();
  return 0;
}

int ChildNode_proc_PacketsFromParent(BackEndNode_t* be, vector_t* packets)
{
    int retval = 0;
    int i;

    mrn_dbg_func_begin();

    for (i = 0; i < packets->size; i++) {
        if (ChildNode_proc_PacketFromParent(be, (Packet_t*)packets->vec[i]) == -1)
            retval = -1;
    }
    
    clear(packets);

    mrn_dbg(3, mrn_printf(FLF, stderr, "ChildNode_proc_Packets() %s", 
                (retval == -1 ? "failed\n" : "succeeded\n")));

    return retval;
}


int ChildNode_proc_PacketFromParent(BackEndNode_t* be, Packet_t* packet)
{
  int retval = 0;

  mrn_dbg_func_begin();
 
  mrn_dbg(5, mrn_printf(FLF, stderr, "ChildNode_proc_PacketFromParent, packet->tag = %d\n", packet->tag));

  switch (packet->tag) {
    case PROT_SHUTDOWN:
      if (BackEndNode_proc_DeleteSubTree(be, packet) == -1) {
        mrn_dbg(1, mrn_printf(FLF, stderr,"BackEndNode_proc_deleteSubTree() failed\n"));
        retval = -1;
      }
      break;
    case PROT_NEW_HETERO_STREAM:
    case PROT_NEW_STREAM:
          if(BackEndNode_proc_newStream(be, packet) == -1) {
              mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_newStream() failed\n" ));
              retval = -1;
          }
          break;
          
    case PROT_SET_FILTERPARAMS_UPSTREAM_SYNC:
	case PROT_SET_FILTERPARAMS_UPSTREAM_TRANS:
          if( BackEndNode_proc_UpstreamFilterParams( be, packet ) == -1 ) {
              mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_UpstreamFilterParams() failed\n" ));
              retval = -1;
          }
          break;

    case PROT_SET_FILTERPARAMS_DOWNSTREAM:
          if( BackEndNode_proc_DownstreamFilterParams( be, packet ) == -1 ) {
              mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_DownstreamFilterParams() failed\n" ));
              retval = -1;
          }
          break;

    case PROT_DEL_STREAM:
          /* We're always a leaf node in the lightweight library, so don't have to do the InternalNode check */
            if (BackEndNode_proc_deleteStream(be, packet) == -1) {
                mrn_dbg(1, mrn_printf(FLF, stderr, "proc_deleteStream() failed\n"));
                retval = -1;
            }
          break;

    case PROT_NEW_FILTER:
          mrn_dbg(5, mrn_printf(FLF, stderr, "BE ignoring new filter; currently, lightweight backend nodes do not perform any filtering. This is different than standard MRNet behavior.\n"));
          break;

    case PROT_FAILURE_RPT:
          if( BackEndNode_proc_FailureReportFromParent( be, packet ) == -1 ){
              mrn_dbg( 1, mrn_printf(FLF, stderr,
                                     "proc_FailureReport() failed\n" ));
              retval = -1;
          }
          break;
    case PROT_NEW_PARENT_RPT:
          if( BackEndNode_proc_NewParentReportFromParent( be, packet ) == -1 ){
              mrn_dbg( 1, mrn_printf(FLF, stderr,
                                     "proc_NewParentReport() failed\n" ));
              retval = -1;
          }
          break;
    case PROT_TOPOLOGY_RPT:
          if( ChildNode_proc_TopologyReport( be, packet ) == -1 ){
              mrn_dbg( 1, mrn_printf(FLF, stderr,
                                     "proc_TopologyReport() failed\n" ));
              retval = -1;
          }
          break;
    case PROT_RECOVERY_RPT:
          if( ChildNode_proc_RecoveryReport( be, packet ) == -1 ){
              mrn_dbg( 1, mrn_printf(FLF, stderr,
                                     "proc_RecoveryReport() failed\n" ));
              retval = -1;
          }
          break;
    case PROT_ENABLE_PERFDATA:
          if( ChildNode_proc_EnablePerfData( be, packet ) == -1 ) {
              mrn_dbg( 1, mrn_printf(FLF, stderr,
                                     "proc_CollectPerfData() failed\n" ));
              retval = -1;
          }
          break;
    case PROT_DISABLE_PERFDATA:
          if( ChildNode_proc_DisablePerfData( be, packet ) == -1 ) {
              mrn_dbg( 1, mrn_printf(FLF, stderr,
                                     "proc_CollectPerfData() failed\n" ));
              retval = -1;
          }
          break;
    case PROT_COLLECT_PERFDATA:
          if( ChildNode_proc_CollectPerfData( be, packet ) == -1 ) {
              mrn_dbg( 1, mrn_printf(FLF, stderr,
                                     "proc_CollectPerfData() failed\n" ));
              retval = -1;
          }
          break;
    case PROT_PRINT_PERFDATA:
          if( ChildNode_proc_PrintPerfData( be, packet ) == -1 ) {
              mrn_dbg( 1, mrn_printf(FLF, stderr,
                                     "proc_PrintPerfData() failed\n" ));
              retval = -1;
          }
          break;
    default:
          //Any Unrecognized tag is assumed to be data
          if( BackEndNode_proc_DataFromParent( be, packet ) == -1 ) {
              mrn_dbg( 1, mrn_printf(FLF, stderr,
                                     "proc_Data() failed\n" ));
              retval = -1;
          }
          break;
  }

  return retval;
}


int ChildNode_ack_DeleteSubTree(BackEndNode_t* be)
{
    Packet_t* packet;
    
    mrn_dbg_func_begin();

    packet = new_Packet_t_2(0, PROT_SHUTDOWN_ACK, "");

  if (packet != NULL) {
      if ( (PeerNode_sendDirectly(be->network->parent, packet) == -1 ) ||
            ( PeerNode_flush(be->network->parent) == -1) )
      {
        mrn_dbg(1, mrn_printf(FLF, stderr, "send failed\n"));
        return false;
      }
  }
  else {
    mrn_dbg(1, mrn_printf(FLF, stderr, "new packet() failed\n"));
    return false;
  } 

  mrn_dbg_func_end();
  return true;
      
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
  int stream_id;
  Stream_t* strm;
  int metric, context;

  mrn_dbg_func_begin();

  stream_id = ipacket->stream_id;
  strm = Network_get_Stream(be->network, stream_id);
  if (strm == NULL) {
    mrn_dbg(1, mrn_printf(FLF, stderr, "stream %d lookup failed\n", stream_id));
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
    int stream_id;
    Stream_t* strm;
    int metric, context;

    mrn_dbg_func_begin();

    stream_id = ipacket->stream_id;
    strm = Network_get_Stream(be->network, stream_id);
    if (strm == NULL) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "stream %d lookup failed\n", stream_id));
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
    int stream_id;
    Stream_t* strm;
    int metric, context, aggr_strm_id;
    Packet_t* pkt;
    Stream_t* aggr_strm;

    mrn_dbg_func_begin();

    stream_id = ipacket->stream_id;
    strm = Network_get_Stream(be->network, stream_id);
    if (strm == NULL) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "stream %d lookup failed\n", stream_id));
        return -1;
    }

    Packet_unpack(ipacket, "%d %d %d", &metric, &context, &aggr_strm_id);

    // collect
    pkt = Stream_collect_PerfData(strm, (perfdata_metric_t)metric, (perfdata_context_t)context, aggr_strm_id);

    // send
    aggr_strm = Network_get_Stream(be->network, aggr_strm_id);
    if (aggr_strm == NULL) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "aggr stream %d lookup failed\n", aggr_strm_id));
        return -1;
    }

    Stream_send_aux(aggr_strm, pkt->tag, pkt->fmt_str, pkt);

    mrn_dbg_func_end();
    return 0;
}

int ChildNode_proc_PrintPerfData(BackEndNode_t* be, Packet_t* ipacket)
{
    int stream_id;
    Stream_t* strm;
    int metric, context;

    mrn_dbg_func_begin();

    stream_id = ipacket->stream_id;
    strm = Network_get_Stream(be->network, stream_id);
    if (strm == NULL) { 
        mrn_dbg (1, mrn_printf(FLF,stderr, "stream %d lookup failed\n", stream_id));
        return -1;
    }

    // local print
    Packet_unpack(ipacket, "%d %d", &metric, &context);
    Stream_print_PerfData(strm, (perfdata_metric_t)metric, (perfdata_context_t)context); 

    mrn_dbg_func_end();
    return 0;
}

int ChildNode_proc_TopologyReport(BackEndNode_t* be, Packet_t* ipacket)
{
    char* topology_ptr;
    char* topology;
   
    mrn_dbg_func_begin();

    Packet_unpack(ipacket, "%s", &topology_ptr);
    topology = topology_ptr;

    if (!Network_reset_Topology(be->network, topology)) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Topology->reset() failed\n"));
        return -1;
    }
    
    // send ack to parent
    if (!ChildNode_ack_TopologyReport(be)) {
        mrn_dbg(1, mrn_printf(FLF,stderr, "ack_TopologyReport() failed\n"));
        return -1;
    }

    mrn_dbg_func_end();

    return 0;

}

int ChildNode_ack_TopologyReport(BackEndNode_t* be)
{
    Packet_t* packet = new_Packet_t_2(0, PROT_TOPOLOGY_ACK, "");

    mrn_dbg_func_begin();

    if (packet != NULL) {
        if ((PeerNode_sendDirectly(be->network->parent, packet) == -1) ||
            (PeerNode_flush(be->network->parent) == -1)) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "send/flush failed\n"));
            return 0;
        }
    }
    else {
        mrn_dbg(1, mrn_printf(FLF, stderr, "new packet() failed\n"));
        return 0;
    }

    mrn_dbg_func_end();

    return 1;
}
