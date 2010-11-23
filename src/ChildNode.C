/****************************************************************************
 *  Copyright 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "BackEndNode.h"
#include "ChildNode.h"
#include "InternalNode.h"
#include "PeerNode.h"
#include "utils.h"
#include "mrnet/MRNet.h"
#include "SerialGraph.h"

namespace MRN
{

/*===================================================*/
/*  ChildNode CLASS METHOD DEFINITIONS               */
/*===================================================*/
ChildNode::ChildNode( Network * inetwork,
                      std::string const& ihostname, Rank irank,
                      std::string const& iphostname, Port ipport, Rank iprank )
    : CommunicationNode(ihostname, UnknownPort, irank),
      _network(inetwork), _incarnation(0)
{
    PeerNodePtr parent =
        _network->new_PeerNode( iphostname, ipport, iprank, true, true );
}

int ChildNode::proc_PacketsFromParent( std::list< PacketPtr > & packets )
{
    int retval = 0;

    mrn_dbg_func_begin();

    std::list < PacketPtr >::iterator iter = packets.begin();
    for( ; iter != packets.end(); iter++ ) {
        mrn_dbg( 5, mrn_printf(FLF, stderr, "tag is %d\n", (*iter)->get_Tag() ));
        if( proc_PacketFromParent( *iter ) == -1 )
            retval = -1;
    }

    packets.clear();

    mrn_dbg( 3, mrn_printf(FLF, stderr, "proc_Packets() %s",
                           ( retval == -1 ? "failed\n" : "succeeded\n" ) ));
    return retval;
}

int ChildNode::proc_PacketFromParent( PacketPtr cur_packet )
{
    int retval = 0;

    switch ( cur_packet->get_Tag() ) {

    case PROT_SHUTDOWN:
        if( _network->is_LocalNodeParent() ) {
            if( _network->get_LocalParentNode()->proc_DeleteSubTree( cur_packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_deleteSubTree() failed\n" ));
                retval = -1;
            }
        }
        else{
            if( _network->get_LocalBackEndNode()->proc_DeleteSubTree( cur_packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_deleteSubTree() failed\n" ));
                retval = -1;
            }
        }
        break;
        
    case PROT_NEW_HETERO_STREAM:
    case PROT_NEW_STREAM:
        if( _network->is_LocalNodeInternal() ){
            if( _network->get_LocalInternalNode()->proc_newStream( cur_packet ) == NULL ){
                mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_newStream() failed\n" ));
                retval = -1;
            }
        }
        else{
            if( _network->get_LocalBackEndNode()->proc_newStream( cur_packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_newStream() failed\n" ));
                retval = -1;
            }
        }
        break;
        
    case PROT_SET_FILTERPARAMS_UPSTREAM_TRANS: {
	FilterType ftype = FILTER_UPSTREAM_TRANS;
        if( _network->is_LocalNodeInternal() ){
	     if( _network->get_LocalInternalNode()->proc_FilterParams( ftype,
								       cur_packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_FilterParams() failed\n" ));
                retval = -1;
            }
        }
        else{
	     if( _network->get_LocalBackEndNode()->proc_FilterParams( ftype,
								      cur_packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_FilterParams() failed\n" ));
                retval = -1;
            }
        }
        break;
    }
    case PROT_SET_FILTERPARAMS_UPSTREAM_SYNC: {
        FilterType ftype = FILTER_UPSTREAM_SYNC;
        if( _network->is_LocalNodeInternal() ){
	     if( _network->get_LocalInternalNode()->proc_FilterParams( ftype,
								       cur_packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_FilterParams() failed\n" ));
                retval = -1;
            }
        }
        else{
	     if( _network->get_LocalBackEndNode()->proc_FilterParams( ftype,
								      cur_packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_FilterParams() failed\n" ));
                retval = -1;
            }
        }
        break;
    }
    case PROT_SET_FILTERPARAMS_DOWNSTREAM: {
        FilterType ftype = FILTER_DOWNSTREAM_TRANS;
        if( _network->is_LocalNodeInternal() ){
	    if( _network->get_LocalInternalNode()->proc_FilterParams( ftype,
								      cur_packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_FilterParams() failed\n" ));
                retval = -1;
            }
        }
        else{
	    if( _network->get_LocalBackEndNode()->proc_FilterParams( ftype,
								     cur_packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_FilterParams() failed\n" ));
                retval = -1;
            }
        }
        break;
    }
    case PROT_DEL_STREAM:
        if( _network->is_LocalNodeInternal() ) {
            if( _network->get_LocalInternalNode()->proc_deleteStream( cur_packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_delStream() failed\n" ));
                retval = -1;
            }
        } else if (_network->is_LocalNodeBackEnd()) {
            if (_network->get_LocalBackEndNode()->proc_deleteStream(cur_packet) == -1) {
                mrn_dbg(1, mrn_printf(FLF, stderr, "proc_deleteStream() failed\n"));
                retval = -1;
            }
        }
        break;

    case PROT_NEW_FILTER:
        if( _network->is_LocalNodeInternal() ){
            if( _network->get_LocalInternalNode()->proc_newFilter( cur_packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_newFilter() failed\n" ));
                retval = -1;
            }
        }
        else {
            if( _network->get_LocalBackEndNode()->proc_newFilter( cur_packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_newFilter() failed\n" ));
                retval = -1;
            }
        }
        break;

    case PROT_FAILURE_RPT:
        if( proc_FailureReportFromParent( cur_packet ) == -1 ){
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "proc_FailureReport() failed\n" ));
            retval = -1;
        }
        break;
    case PROT_NEW_PARENT_RPT:
        if( proc_NewParentReportFromParent( cur_packet ) == -1 ){
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "proc_NewParentReport() failed\n" ));
            retval = -1;
        }
        break;
    case PROT_TOPOLOGY_RPT:
        if( proc_TopologyReport( cur_packet ) == -1 ){
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "proc_TopologyReport() failed\n" ));
            retval = -1;
        }
        break;
    case PROT_RECOVERY_RPT:
        if( proc_RecoveryReport( cur_packet ) == -1 ){
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "proc_RecoveryReport() failed\n" ));
            retval = -1;
        }
        break;
    case PROT_ENABLE_PERFDATA:
        if( proc_EnablePerfData( cur_packet ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "proc_CollectPerfData() failed\n" ));
            retval = -1;
        }
        break;
    case PROT_DISABLE_PERFDATA:
        if( proc_DisablePerfData( cur_packet ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "proc_CollectPerfData() failed\n" ));
            retval = -1;
        }
        break;
    case PROT_COLLECT_PERFDATA:
        if( proc_CollectPerfData( cur_packet ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "proc_CollectPerfData() failed\n" ));
            retval = -1;
        }
        break;
    case PROT_PRINT_PERFDATA:
        if( proc_PrintPerfData( cur_packet ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "proc_PrintPerfData() failed\n" ));
            retval = -1;
        }
        break;

    case PROT_ENABLE_RECOVERY:
        if( proc_EnableFailReco( cur_packet ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "proc_EnableFailReco() failed\n" ));
            retval = -1;
        }
        break;
    case PROT_DISABLE_RECOVERY:
        if( proc_DisableFailReco( cur_packet ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "proc_DisableFailReco() failed\n" ));
            retval = -1;
        }
        break;
    default:
        //Any Unrecognized tag is assumed to be data
        if( proc_DataFromParent( cur_packet ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "proc_Data() failed\n" ));
            retval = -1;
        }
        break;
    }

    return retval;
}

int ChildNode::proc_EnablePerfData( PacketPtr ipacket ) const
{
    unsigned int stream_id;

    mrn_dbg_func_begin();

    stream_id = ipacket->get_StreamId();
    Stream* strm = _network->get_Stream( stream_id );
    if( strm == NULL ){
        mrn_dbg( 1, mrn_printf(FLF, stderr, "stream %u lookup failed\n",
                               stream_id) );
        return -1;
    }

    if( _network->is_LocalNodeParent() ) {
        // forward packet to children nodes
        if( _network->send_PacketToChildren( ipacket ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n" ));
            return -1;
        }
    }

    // local update
    int metric, context;
    ipacket->unpack( "%d %d", &metric, &context );
    strm->enable_PerfData( (perfdata_metric_t)metric, (perfdata_context_t)context );

    mrn_dbg_func_end();
    return 0;
}

int ChildNode::proc_DisablePerfData( PacketPtr ipacket ) const
{
    unsigned int stream_id;

    mrn_dbg_func_begin();

    stream_id = ipacket->get_StreamId();
    Stream* strm = _network->get_Stream( stream_id );
    if( strm == NULL ){
        mrn_dbg( 1, mrn_printf(FLF, stderr, "stream %u lookup failed\n",
                               stream_id) );
        return -1;
    }

    if( _network->is_LocalNodeParent() ) {
        // forward packet to children nodes
        if( _network->send_PacketToChildren( ipacket ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n" ));
            return -1;
        }
    }

    // local update
    int metric, context;
    ipacket->unpack( "%d %d", &metric, &context );
    strm->disable_PerfData( (perfdata_metric_t)metric, (perfdata_context_t)context );

    mrn_dbg_func_end();
    return 0;
}

int ChildNode::proc_CollectPerfData( PacketPtr ipacket ) const
{
    unsigned int stream_id;

    mrn_dbg_func_begin();

    stream_id = ipacket->get_StreamId();
    Stream* strm = _network->get_Stream( stream_id );
    if( strm == NULL ){
        mrn_dbg( 1, mrn_printf(FLF, stderr, "stream %u lookup failed\n",
                               stream_id ));
        return -1;
    }

    if( _network->is_LocalNodeParent() ) {
        // forward packet to children nodes
        if( _network->send_PacketToChildren( ipacket ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n" ));
            return -1;
        }
    }
    else if( _network->is_LocalNodeBackEnd() ) { 
        int metric, context;
        unsigned int aggr_strm_id;
        ipacket->unpack( "%d %d %ud", &metric, &context, &aggr_strm_id );
    

        // collect
        PacketPtr pkt = strm->collect_PerfData( (perfdata_metric_t)metric, 
                                                (perfdata_context_t)context,
						aggr_strm_id );
        
        // send
        Stream* aggr_strm = _network->get_Stream( aggr_strm_id );
        if( aggr_strm == NULL ){
            mrn_dbg( 1, mrn_printf(FLF, stderr, "aggr stream %u lookup failed\n",
                                   aggr_strm_id) );
            return -1;
        }
        bool upstream = true;
        aggr_strm->send_aux( pkt, upstream );
    }

    mrn_dbg_func_end();
    return 0;
}

int ChildNode::proc_PrintPerfData( PacketPtr ipacket ) const
{
    unsigned int stream_id;

    mrn_dbg_func_begin();

    stream_id = ipacket->get_StreamId();
    Stream* strm = _network->get_Stream( stream_id );
    if( strm == NULL ){
        mrn_dbg( 1, mrn_printf(FLF, stderr, "stream %u lookup failed\n",
                               stream_id) );
        return -1;
    }

    if( _network->is_LocalNodeParent() ) {
        // forward packet to children nodes
        if( _network->send_PacketToChildren( ipacket ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n" ));
            return -1;
        }
    }

    // local print
    int metric, context;
    ipacket->unpack( "%d %d", &metric, &context );
    strm->print_PerfData( (perfdata_metric_t)metric, (perfdata_context_t)context );

    mrn_dbg_func_end();
    return 0;
}

int ChildNode::proc_TopologyReport( PacketPtr ipacket ) const 
{
    char * topology_ptr;
    mrn_dbg_func_begin();

    ipacket->unpack( "%s", &topology_ptr );
    std::string topology = topology_ptr;

    if( !_network->reset_Topology( topology ) ){
        mrn_dbg( 1, mrn_printf(FLF, stderr, "Topology->reset() failed\n" ));
        return -1;
    }

    if( _network->is_LocalNodeParent() ) {
        if( _network->get_LocalParentNode()->proc_TopologyReport( ipacket ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n" ));
            return -1;
        }
    }
    else {
        // send ack to parent
        if( ! ack_TopologyReport() ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "ack_TopologyReport() failed\n" ));
            return -1;
        }
    }

    mrn_dbg_func_end();
    return 0;
}

int ChildNode::send_EventsToParent( ) const
{
    /* NOTE: we need a sensible method for reporting events to the FE,
             e.g., an event reporting stream. until we have this,
             there's nothing to do here -- Oct 2010
    */
    return -1;
}

void ChildNode::error( ErrorCode, Rank, const char *, ... ) const
{
    /* NOTE: we need a sensible method for reporting errors to the FE,
             e.g., an error reporting stream. until we have this,
             there's nothing to do here -- Oct 2010
    */
}

int ChildNode::init_newChildDataConnection( PeerNodePtr iparent,
                                            Rank ifailed_rank /* = UnknownRank */ )
{
    mrn_dbg_func_begin();
    NetworkTopology* tmp_nt=_network->get_NetworkTopology();


    // Establish data detection connection w/ new Parent
    if( iparent->connect_DataSocket() == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "connect_DataSocket() failed\n") );
        return -1;
    }

    _incarnation++;

    char is_internal_char;
    if( _network->is_LocalNodeInternal() ) {
        is_internal_char = 't';
    }
    else{
        is_internal_char = 'f';
    }

    char* topo_ptr = _network->get_LocalSubTreeStringPtr();
    if( topo_ptr == NULL ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "failed to get local subtree\n") );
        return -1;
    }

    mrn_dbg( 5, mrn_printf(FLF, stderr, "topology: \"%s\"\n", topo_ptr) );
    PacketPtr packet( new Packet( CTL_STRM_ID, PROT_NEW_CHILD_DATA_CONNECTION,
                                  "%s %uhd %ud %uhd %ud %c %s",
                                  _hostname.c_str(),
                                  _port,
                                  _rank,
                                  _incarnation,
                                  ifailed_rank,
                                  is_internal_char,
                                  topo_ptr ) );
    mrn_dbg( 5, mrn_printf(FLF, stderr, "Send initialization info ...\n") );
    if( iparent->sendDirectly( packet ) ==  -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "send/flush() failed\n") );
        return -1;
    }
    free( topo_ptr );

    SerialGraph* init_topo = _network->read_Topology( iparent->_data_sock_fd );
    assert( init_topo != NULL );
    std::string sg_str = init_topo->get_ByteArray();

    tmp_nt->reset( sg_str, false );
    mrn_dbg( 5, mrn_printf(FLF, stderr, "topology is %s\n", 
                           tmp_nt->get_TopologyString().c_str()) );

    //Create send/recv threads
    mrn_dbg( 5, mrn_printf(FLF, stderr, "Creating comm threads for parent\n") );
    iparent->start_CommunicationThreads();

    mrn_dbg_func_end();

    return 0;
}
	
int ChildNode::send_SubTreeInitDoneReport( ) const
{   
    mrn_dbg_func_begin();
    
    _network->get_NetworkTopology()->update_Router_Table();

    PacketPtr packet( new Packet( CTL_STRM_ID, PROT_SUBTREE_INITDONE_RPT,"") );

    if( !packet->has_Error( ) ) {
        _network->get_ParentNode()->send( packet );
        if( _network->get_ParentNode()->flush() == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "send/flush failed\n") );
            return -1;
        }
    }
    else {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "new packet() failed\n") );
        return -1;
    }

    mrn_dbg_func_end();
    return 0;
}

int ChildNode::send_NewSubTreeReport( ) const
{
    assert(!"DEPRECATED -- THIS SHOULD NEVER BE CALLED");
    return 0;
}

int ChildNode::request_SubTreeInfo( void ) const 
{
    mrn_dbg_func_begin();
    PacketPtr packet( new Packet( CTL_STRM_ID, PROT_SUBTREE_INFO_REQ, "%ud", _rank ) );

    if( !packet->has_Error( ) ) {
        _network->get_ParentNode()->send( packet );
        if( _network->get_ParentNode()->flush() == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "send/flush failed\n") );
            return -1;
        }
    }
    else {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "new packet() failed\n") );
        return -1;
    }

    mrn_dbg_func_end();
    return 0;
}

int ChildNode::proc_RecoveryReport( PacketPtr ipacket ) const
{
    mrn_dbg_func_begin();

    Rank child_rank, failed_parent_rank, new_parent_rank;

    ipacket->unpack( "%ud %ud %ud",
                     &child_rank,
                     &failed_parent_rank,
                     &new_parent_rank );

    //remove node, but don't update datastructs since following procedure will
    _network->remove_Node( failed_parent_rank, false );
    _network->change_Parent( child_rank, new_parent_rank );

    //Internal nodes must report parent failure to children
    if( _network->is_LocalNodeInternal() ) {
        if( _network->send_PacketToChildren( ipacket ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n") );
            return -1;
        }
    }

    mrn_dbg_func_end();
    return 0;
}

bool ChildNode::ack_DeleteSubTree( void ) const
{
    mrn_dbg_func_begin();

    PacketPtr packet( new Packet(CTL_STRM_ID, PROT_SHUTDOWN_ACK, "") );

    if( ! packet->has_Error() ) {
        /* note: don't request flush as send thread will exit 
                 before notifying flush completion */
        _network->get_ParentNode()->send( packet );
    }
    else {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "new packet() failed\n") );
        return false;
    }

    mrn_dbg_func_end();
    return true;
}

bool ChildNode::ack_TopologyReport( void ) const
{
    mrn_dbg_func_begin();

    PacketPtr packet( new Packet(CTL_STRM_ID, PROT_TOPOLOGY_ACK, "") );

    if( ! packet->has_Error() ) {
        _network->get_ParentNode()->send( packet );
        if( _network->get_ParentNode()->flush() == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "send/flush failed\n") );
            return false;
        }
    }
    else {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "new packet() failed\n") );
        return false;
    }

    mrn_dbg_func_end();
    return true;
}

int ChildNode::proc_PortUpdate( PacketPtr ) const
{
    // virtual method implementation, this shouldn't be called
    return -1;
}

int ChildNode::recv_PacketsFromParent(std::list <PacketPtr> &packet_list) const 
{
    return _network->recv_PacketsFromParent(packet_list);
}

bool ChildNode::has_PacketsFromParent( ) const
{
    return _network->has_PacketsFromParent();
}

/* Failure Recovery Code */

int ChildNode::proc_EnableFailReco( PacketPtr ipacket ) const
{
    mrn_dbg_func_begin();

    if( _network->is_LocalNodeParent() ) {
        // forward packet to children nodes
        if( _network->send_PacketToChildren( ipacket ) == -1 ) {
            mrn_dbg( 3, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n" ));
            return -1;
        }
    }

    // local update
    _network->enable_FailureRecovery();

    mrn_dbg_func_end();
    return 0;
}

int ChildNode::proc_DisableFailReco( PacketPtr ipacket ) const
{
    mrn_dbg_func_begin();

    if( _network->is_LocalNodeParent() ) {
        // forward packet to children nodes
        if( _network->send_PacketToChildren( ipacket ) == -1 ) {
            mrn_dbg( 3, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n" ));
            return -1;
        }
    }

    // local update
    _network->disable_FailureRecovery();

    mrn_dbg_func_end();
    return 0;
}


} // namespace MRN
