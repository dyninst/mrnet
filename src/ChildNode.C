/****************************************************************************
 *  Copyright 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
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
                      std::string const& UNUSED(ihostname), 
                      Rank UNUSED(irank),
                      std::string const& iphostname, Port ipport, Rank iprank )
    : _network( inetwork ), 
      _incarnation( 0 )
{
    PeerNodePtr parent = _network->new_PeerNode( iphostname, ipport, iprank, 
                                                 true, true );
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
    int tag = cur_packet->get_Tag();
    if( (tag > PROT_FIRST) && (tag < PROT_LAST) ) {

        switch ( tag ) {

        case PROT_SHUTDOWN:
            if( _network->is_LocalNodeParent() ) {
                if( _network->get_LocalParentNode()->proc_DeleteSubTree( cur_packet ) == -1 ) {
                    mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_deleteSubTree() failed\n") );
                    retval = -1;
                }
            }
            else{
                if( _network->get_LocalBackEndNode()->proc_DeleteSubTree( cur_packet ) == -1 ) {
                    mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_deleteSubTree() failed\n") );
                    retval = -1;
                }
            }
            break;
        
        case PROT_NEW_STREAM:
        case PROT_NEW_HETERO_STREAM:
        case PROT_NEW_INTERNAL_STREAM:
            if( _network->is_LocalNodeInternal() ){
                if( _network->get_LocalInternalNode()->proc_newStream( cur_packet ) == NULL ){
                    mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_newStream() failed\n") );
                    retval = -1;
                }
            }
            else{
                if( _network->get_LocalBackEndNode()->proc_newStream( cur_packet ) == -1 ) {
                    mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_newStream() failed\n") );
                    retval = -1;
                }
            }
            break;
        
        case PROT_SET_FILTERPARAMS_UPSTREAM_TRANS: {
            FilterType ftype = FILTER_UPSTREAM_TRANS;
            if( _network->is_LocalNodeInternal() ){
                if( _network->get_LocalInternalNode()->proc_FilterParams( ftype,
                                                                          cur_packet ) == -1 ) {
                    mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_FilterParams() failed\n") );
                    retval = -1;
                }
            }
            else{
                if( _network->get_LocalBackEndNode()->proc_FilterParams( ftype,
                                                                         cur_packet ) == -1 ) {
                    mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_FilterParams() failed\n") );
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
                    mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_FilterParams() failed\n") );
                    retval = -1;
                }
            }
            else{
                if( _network->get_LocalBackEndNode()->proc_FilterParams( ftype,
                                                                         cur_packet ) == -1 ) {
                    mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_FilterParams() failed\n") );
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
                    mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_FilterParams() failed\n") );
                    retval = -1;
                }
            }
            else{
                if( _network->get_LocalBackEndNode()->proc_FilterParams( ftype,
                                                                         cur_packet ) == -1 ) {
                    mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_FilterParams() failed\n") );
                    retval = -1;
                }
            }
            break;
        }
        case PROT_DEL_STREAM:
            if( _network->is_LocalNodeInternal() ) {
                if( _network->get_LocalInternalNode()->proc_deleteStream( cur_packet ) == -1 ) {
                    mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_delStream() failed\n") );
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
                    mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_newFilter() failed\n") );
                    retval = -1;
                }
            }
            else {
                if( _network->get_LocalBackEndNode()->proc_newFilter( cur_packet ) == -1 ) {
                    mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_newFilter() failed\n") );
                    retval = -1;
                }
            }
            break;
        case PROT_RECOVERY_RPT:
            if( proc_RecoveryReport( cur_packet ) == -1 ){
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "proc_RecoveryReport() failed\n") );
                retval = -1;
            }
            break;
        case PROT_ENABLE_PERFDATA:
            if( proc_EnablePerfData( cur_packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "proc_CollectPerfData() failed\n") );
                retval = -1;
            }
            break;
        case PROT_DISABLE_PERFDATA:
            if( proc_DisablePerfData( cur_packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "proc_CollectPerfData() failed\n") );
                retval = -1;
            }
            break;
        case PROT_COLLECT_PERFDATA:
            if( proc_CollectPerfData( cur_packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "proc_CollectPerfData() failed\n") );
                retval = -1;
            }
            break;
        case PROT_PRINT_PERFDATA:
            if( proc_PrintPerfData( cur_packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "proc_PrintPerfData() failed\n") );
                retval = -1;
            }
            break;

        case PROT_ENABLE_RECOVERY:
            if( proc_EnableFailReco( cur_packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "proc_EnableFailReco() failed\n") );
                retval = -1;
            }
            break;
        case PROT_DISABLE_RECOVERY:
            if( proc_DisableFailReco( cur_packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "proc_DisableFailReco() failed\n") );
                retval = -1;
            }
            break;
        case PROT_NET_SETTINGS:
            if( proc_NetworkSettings( cur_packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "proc_NetworkSettings() failed\n") );
                retval = -1;
            }
            break;
        case PROT_PORT_UPDATE:
            if( proc_PortUpdate(cur_packet) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_PortUpdate() failed\n"));
                retval = -1;
            }
            break;
        case PROT_TOPO_UPDATE: // not control stream, treat as data
            if( proc_DataFromParent( cur_packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_Data() failed\n"));
                retval = -1;
            }
            break;
        default:
            mrn_dbg( 1, mrn_printf(FLF, stderr, 
                                   "internal protocol tag %d is unhandled\n", tag) );
            break;
        }
    }
    else if( tag >= FirstApplicationTag ) {
        if( proc_DataFromParent( cur_packet ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_Data() failed\n"));
            retval = -1;
        }
    }
    else {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "tag %d is invalid\n", tag) );
        retval = -1;
    }

    return retval;
}

int ChildNode::proc_NetworkSettings( PacketPtr ipacket ) const
{
    mrn_dbg_func_begin();
    
    char* sg_byte_array = NULL;
    int* keys = NULL;
    char** vals = NULL;
    uint32_t i, count;
    NetworkTopology* nt = _network->get_NetworkTopology();    

    if( ipacket->unpack( "%s %ad %as", &sg_byte_array, 
                         &keys, &count, 
                         &vals, &count ) == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "unpack() failed\n") );
        return -1;
    }
    
    // init topology 
    std::string sg_str(sg_byte_array);
    nt->reset( sg_str, false );

    // init other network settings
    std::map< net_settings_key_t, std::string >& settingsMap = _network->get_SettingsMap();
    for( i=0; i < count; i++ ) {
       if( keys[i] == MRNET_FAILURE_RECOVERY ) {
           if( atoi(vals[i]) )
               _network->enable_FailureRecovery(); 
	   else
	       _network->disable_FailureRecovery();
       }	   
       else
           settingsMap[ (net_settings_key_t) keys[i] ] =  vals[i];

       free( vals[i] );
    }
    
    free(vals);	
    free( keys );

    _network->init_NetSettings();

    mrn_dbg_func_end();
    return 0;
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
        if( _network->send_PacketToChildren(ipacket) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n") );
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
        if( _network->send_PacketToChildren(ipacket) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n") );
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
                               stream_id) );
        return -1;
    }

    if( _network->is_LocalNodeParent() ) {
        // forward packet to children nodes
        if( _network->send_PacketToChildren( ipacket ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n") );
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
            mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n") );
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

    // Establish data detection connection w/ new Parent
    int num_retry = 15;
    if( iparent->connect_DataSocket(num_retry) == -1 ) {
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

    if( _incarnation == 1 ) {
        // handle network settings packet
        std::list< PacketPtr > packet_list;    
        int rret = iparent->recv( packet_list );
        if( (rret == -1) || ((rret == 0) && (packet_list.size() == 0)) ) {
            if( rret == -1 ) {
                mrn_dbg(3, mrn_printf(FLF, stderr, "recv() topo and env failed!\n"));
                return -1;
            }
        }
        if( proc_PacketsFromParent( packet_list ) == -1 )
            mrn_dbg(1, mrn_printf(FLF, stderr, "proc_PacketsFromParent() failed\n"));
    }

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

    PacketPtr packet( new Packet(CTL_STRM_ID, PROT_SUBTREE_INITDONE_RPT, NULL) );

    if( ! packet->has_Error() ) {
        _network->send_PacketToParent( packet );
        _network->flush_PacketsToParent();
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

bool ChildNode::ack_ControlProtocol( int ack_tag ) const
{
    mrn_dbg(3, mrn_printf(FLF, stderr, "tag=%d\n", ack_tag));

    PacketPtr packet( new Packet(CTL_STRM_ID, ack_tag, NULL) );    
    if( ! packet->has_Error() ) {
        _network->send_PacketToParent( packet );
        _network->flush_PacketsToParent();
    }
    else {
        mrn_dbg(1, mrn_printf(FLF, stderr, "new Packet() failed\n"));
        return false;
    }

    mrn_dbg_func_end();
    return true;
}

bool ChildNode::ack_DeleteSubTree( void ) const
{
    mrn_dbg_func_begin();

    PacketPtr packet( new Packet(CTL_STRM_ID, PROT_SHUTDOWN_ACK, NULL) );

    if( ! packet->has_Error() ) {
        /* note: don't request flush as send thread will exit 
                 before notifying flush completion */
        _network->send_PacketToParent( packet );
    }
    else {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "new Packet() failed\n") );
        return false;
    }

    mrn_dbg_func_end();
    return true;
}

int ChildNode::proc_PortUpdate( PacketPtr ipacket ) const
{
    mrn_dbg_func_begin();

    if( _network->is_LocalNodeParent() ) {
        ParentNode* parent = _network->get_LocalParentNode();
        if( parent->get_numChildrenExpected() ) {
            // I have children, forward request
            if( parent->proc_PortUpdates( ipacket ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n" ));
                return -1;
            }
            // success, I'm done
            mrn_dbg_func_end();
            return 0;
        }
        // else, leaf commnode, so fall through
    }
    // else, i'm a back-end or a leaf commnode, so send a port update packet

    Stream *s = _network->get_Stream(PORT_STRM_ID);
    if( s != NULL ) {
        int type = NetworkTopology::TOPO_CHANGE_PORT ;
        char *host_arr = strdup("NULL"); // ugh, this needs to be fixed
        Rank send_iprank = UnknownRank;
        Rank send_myrank = _network->get_LocalRank();
        Port send_port = _network->get_LocalPort();
        
        if( _network->is_LocalNodeBackEnd() )
            s->send( PROT_TOPO_UPDATE, "%ad %aud %aud %as %auhd",
                     &type, 1,
                     &send_iprank, 1,
                     &send_myrank, 1,
                     &host_arr, 1,
                     &send_port, 1 );
        else
            s->send_internal( PROT_TOPO_UPDATE, "%ad %aud %aud %as %auhd",
                              &type, 1,
                              &send_iprank, 1,
                              &send_myrank, 1,
                              &host_arr, 1,
                              &send_port, 1 );
        s->flush();
        free(host_arr);
    }
    else {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "stream %u lookup failed\n",
                               PORT_STRM_ID) );
    }

    mrn_dbg_func_end();
    return 0;
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
            mrn_dbg( 3, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n") );
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
            mrn_dbg( 3, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n") );
            return -1;
        }
    }

    // local update
    _network->disable_FailureRecovery();

    mrn_dbg_func_end();
    return 0;
}


} // namespace MRN
