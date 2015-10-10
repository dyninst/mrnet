/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <set>

#ifndef os_windows
#include "mrnet_config.h"
#endif
#include "ChildNode.h"
#include "EventDetector.h"
#include "Filter.h"
#include "InternalNode.h"
#include "ParentNode.h"
#include "PeerNode.h"
#include "Router.h"
#include "SerialGraph.h"
#include "utils.h"

#include "mrnet/MRNet.h"
#include "xplat/Process.h"
#include "xplat/Error.h"

using namespace XPlat;

namespace MRN
{

/*====================================================*/
/*  ParentNode CLASS METHOD DEFINITIONS            */
/*====================================================*/
ParentNode::ParentNode( Network* inetwork,
                        std::string const& UNUSED(myhost),
                        Rank UNUSED(myrank),
                        int listeningSocket /* = -1 */,
                        Port UNUSED(listeningPort) /* = UnknownPort */ )
    : _network( inetwork ),
      _num_children( 0 ),
      _num_children_to_del( 0 ),
      _num_children_reported_init( 0 ),
      _num_children_reported_del( 0 ),
      _num_failed_children( 0 ),
      _filter_children_reported( 0 ),
      listening_sock_fd( listeningSocket )
{
    mrn_dbg( 5, mrn_printf(FLF, stderr, "ParentNode(local[%u]:\"%s:%u\")\n",
                           _rank, _hostname.c_str(), _port) );

    if( listeningSocket == -1 ) {
        listening_sock_fd = 0; // force system to create socket and assign port
        _port = UnknownPort;

        mrn_dbg( 3, mrn_printf(FLF, stderr, "Calling bind_to_port(%d)\n", _port ));

        if( bindPort( &listening_sock_fd, &_port, true /*non-blocking*/ ) == -1 ) {
            error( ERR_SYSTEM, _rank, "bindPort(%d)", _port );
            return;
        }
    }

    subtreereport_sync.RegisterCondition( ALL_NODES_REPORTED );
    initdonereport_sync.RegisterCondition( ALL_NODES_REPORTED );

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Leaving ParentNode()\n" ));
}

ParentNode::~ParentNode( void )
{
}

int ParentNode::proc_PacketsFromChildren( std::list< PacketPtr > & ipackets )
{
    int retval = 0;

    mrn_dbg_func_begin();

    std::list< PacketPtr >::iterator iter = ipackets.begin();
    for( ; iter != ipackets.end(); iter++ ) {
        if( proc_PacketFromChildren(*iter) == -1 )
            retval = -1;
    }
 
    mrn_dbg( 3, mrn_printf(FLF, stderr, "proc_PacketsFromChildren() %s",
                           (retval == -1 ? "failed\n" : "succeeded\n")) );
    ipackets.clear(  );
    return retval;
}

int ParentNode::proc_PacketFromChildren( PacketPtr cur_packet )
{
    int retval = 0;
    
    int tag = cur_packet->get_Tag();

    if( (tag > PROT_FIRST) && (tag < PROT_LAST) ) {
        switch ( cur_packet->get_Tag() ) {
        case PROT_SUBTREE_INITDONE_RPT:
            if( proc_SubTreeInitDoneReport(cur_packet) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "proc_SubTreeInitDoneReport() failed\n" ));
                retval = -1;
            }
            break;
        case PROT_SHUTDOWN_ACK:
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                        "WARNING: PROT_SHUTDOWN_ACK deprecated\n") );
            break;
        case PROT_NEW_STREAM_ACK:
            if( proc_ControlProtocolAck(cur_packet) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "proc_ControlProtocolAck() failed\n" ));
                retval = -1;
            }
            break;
        case PROT_EVENT:
            if( proc_Event(cur_packet) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_Event() failed\n" ));
                retval = -1;
            }
            break;
        case PROT_RECOVERY_RPT:
            if( proc_RecoveryReport(cur_packet) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_RecoveryReport() failed\n" ));
                retval = -1;
            }
            break;
        case PROT_TOPO_UPDATE:      // not control stream, treat as data
        case PROT_COLLECT_PERFDATA: 
            if( proc_DataFromChildren(cur_packet) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "proc_DataFromChildren() failed\n" ));
                retval = -1;
            }
            break;
        default:
            //Any unrecognized tag is assumed to be data
            mrn_dbg( 1, mrn_printf(FLF, stderr, 
                                   "internal protocol tag %d is unhandled\n", tag) );
            retval = -1;
        }
    }
    else if( tag >= FirstApplicationTag ) {
        if( proc_DataFromChildren(cur_packet) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "proc_DataFromChildren() failed\n" ));
            retval = -1;
        }
    }
    else {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "tag %d is invalid\n", tag) );
        retval = -1;
    }

    return retval;
}

//This function was changed to by default, function similarly to
//RSHParentNode::proc_PortUpdates
//This will require a workaround for Cray, either a special CrayParentNode implementation
//or a case check, if(Cray) -> Do nothing

int ParentNode::proc_PortUpdates( PacketPtr ipacket ) const
{
    
    mrn_dbg_func_begin();

    Stream* port_strm = NULL;

    NetworkTopology* net_topo = _network->get_NetworkTopology();
    unsigned net_size = net_topo->get_NumNodes();

    if( _network->is_LocalNodeFrontEnd() && (net_size > 1) ) {
        // create a waitforall topology update stream
        Communicator* bcast_comm = _network->get_BroadcastCommunicator();
        port_strm = _network->new_InternalStream( bcast_comm, TFILTER_TOPO_UPDATE, 
                                                  SFILTER_WAITFORALL );
        if( NULL == port_strm ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, 
                                   "failed to create port update stream\n") );
            return -1;
        }
        assert( port_strm->get_Id() == PORT_STRM_ID );
    }

    // request port updates from children
    if( ( _network->send_PacketToChildren(ipacket) == -1 ) ||
        ( _network->flush_PacketsToChildren() == -1 ) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, 
                               "send/flush_PacketToChildren() failed\n") );
        return -1;
    }

    if( _network->is_LocalNodeFrontEnd() && (net_size > 1) ) {
        // block until updates received
        mrn_dbg( 5, mrn_printf(FLF, stderr, "blocking until port updates received.\n"));
        int tag;
        PacketPtr p;
        port_strm->recv( &tag, p );
        mrn_dbg( 5, mrn_printf(FLF, stderr, "Received port updates\n"));

        // broadcast the accumulated updates
        _network->send_TopologyUpdates();
    }

    mrn_dbg_func_end();
    return 0;
}

// This function fails if ANY of our direct children have failed at ANY time.
// This is because it is currently only used during startup.
bool ParentNode::waitfor_ControlProtocolAcks( int ack_tag, 
                                              unsigned num_acks_expected ) const
{
    mrn_dbg_func_begin();
    bool ret_val = true;
    
    if( num_acks_expected > 0 ) {

        XPlat::Monitor wait_sync;
        std::map< int, struct ControlProtocol >::iterator cps_iter;

        wait_sync.RegisterCondition( NODE_REPORTED );

        cps_sync.Lock();
        cps_iter = active_ctl_protocols.find( ack_tag );
        if( cps_iter == active_ctl_protocols.end() ) {
            struct ControlProtocol &wait_info = active_ctl_protocols[ ack_tag ];
            wait_info.sync = &wait_sync;
            wait_info.num_acked = 0;
            wait_info.aborted = false;
            cps_iter = active_ctl_protocols.find( ack_tag );
        }
        else {
            cps_sync.Unlock();
            mrn_dbg(1, mrn_printf(FLF, stderr, 
                                  "ctl_protocol_sync for tag %d is in use\n", 
                                  ack_tag));
            return false;
        }
        cps_sync.Unlock();
    
        wait_sync.Lock();

        unsigned num_acked = cps_iter->second.num_acked;
        bool abort = ((_num_failed_children > 0) || cps_iter->second.aborted);

        while( num_acked < num_acks_expected ) {
            // ONLY true if a node has failed, direct child or not
            if(abort) {
                mrn_dbg(3, mrn_printf(FLF, stderr,
                           "Protocol(%d) has been aborted!\n", ack_tag));
                ret_val = false;
                break;
            }

            mrn_dbg(5, mrn_printf(FLF, stderr, 
                                  "Waiting for %u of %u protocol(%d) acks ...\n",
                                  num_acks_expected - num_acked, 
                                  num_acks_expected, 
                                  ack_tag));
            wait_sync.WaitOnCondition( NODE_REPORTED );
 
            num_acked = cps_iter->second.num_acked;    
            mrn_dbg(3, mrn_printf(FLF, stderr,
                                  "%d of %d have ack'd protocol(%d).\n",
                                  num_acked, num_acks_expected, ack_tag));

            abort = ((_num_failed_children > 0) || cps_iter->second.aborted);
        }

        wait_sync.Unlock();

        cps_sync.Lock();
        if(ret_val)
            active_ctl_protocols.erase( cps_iter );
        cps_sync.Unlock();
    }

    mrn_dbg_func_end();
    return ret_val;
}

int ParentNode::proc_ControlProtocolAck( PacketPtr ipacket )
{
    mrn_dbg_func_begin();

    char recv_char;
    ipacket->unpack("%c", &recv_char);
    bool abort = (recv_char == (char)0) ? true : false; 
    int ack_tag = ipacket->get_Tag();
    
    XPlat::Monitor* wait_sync = NULL;

    std::map< int, struct ControlProtocol >::iterator cps_iter;
    do {
        cps_sync.Lock();
        cps_iter = active_ctl_protocols.find( ack_tag );
        if( cps_iter != active_ctl_protocols.end() ) {
            wait_sync = (*cps_iter).second.sync;
        }
        else {
            // an ack can arrive before we have even started waiting
            mrn_dbg(5, mrn_printf(FLF, stderr, 
                                  "ctl_protocol_sync for tag %d was not found, spin-waiting\n", 
                                  ack_tag));
        }
        cps_sync.Unlock();
    } while( wait_sync == NULL && !abort );

    if(abort) {
        if( cps_iter != active_ctl_protocols.end() ) {
            // This is the first failed ack we have received
            abort_ControlProtocol(cps_iter->second);
        }
    } else {
        wait_sync->Lock();
        (*cps_iter).second.num_acked++;
        wait_sync->SignalCondition( NODE_REPORTED );
        wait_sync->Unlock();
    }

    mrn_dbg_func_end();
    return 0;
}

int ParentNode::abort_ControlProtocol( struct ControlProtocol &cp ) {
    mrn_dbg_func_begin();

    if(cp.aborted) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "Already aborted!\n"));
        return -1;
    }

    XPlat::Monitor *wait_sync = cp.sync;

    // Signal the correct CV so the waiting
    // thread can see the protocol has failed and act accordingly
    wait_sync->Lock();
    cp.aborted = true;
    mrn_dbg(5, mrn_printf(FLF, stderr, "Signalling NODE_REPORTED to abort\n"));
    wait_sync->SignalCondition( NODE_REPORTED );
    wait_sync->Unlock();

    mrn_dbg_func_end();
    return 0;
}

int ParentNode::abort_ActiveControlProtocols() {
    mrn_dbg_func_begin();
    std::map<int, struct ControlProtocol >::iterator cps_iter;
    int ret_val = 0;
    cps_sync.Lock();
    if(active_ctl_protocols.size() == 0) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "No active control protocols to abort!\n"));
        cps_sync.Unlock();
        return -1;
    } else {
        cps_iter = active_ctl_protocols.begin();
        for(; cps_iter != active_ctl_protocols.end(); cps_iter++) {
            if(abort_ControlProtocol(cps_iter->second) == -1) {
                ret_val = -1;
            }
        }
    }
    cps_sync.Unlock();

    mrn_dbg_func_end();
    return ret_val;
}

int ParentNode::send_LaunchInfo( PeerNodePtr ) const
{
    // nothing to see here, try the platform-specific code
    return 0;
}

int ParentNode::proc_SubTreeInitDoneReport( PacketPtr ) const
{
    mrn_dbg_func_begin();

    initdonereport_sync.Lock();

    if( _num_children_reported_init == _num_children ) {
        // already saw reports from known children, must be a newborn
        _num_children++;
        _num_children_reported_init++;
    }
    else {
        _num_children_reported_init++;
        mrn_dbg(3, mrn_printf(FLF, stderr, "%d of %d descendants reported\n",
                              _num_children_reported_init, _num_children));
        if( _num_children_reported_init == _num_children ) {
            mrn_dbg(5, mrn_printf(FLF, stderr, "Signaling ALL_NODES_REPORTED\n"));
            initdonereport_sync.SignalCondition( ALL_NODES_REPORTED );
            mrn_dbg(5, mrn_printf(FLF, stderr, "Signaling done\n"));
        }
    }

    initdonereport_sync.Unlock();

    mrn_dbg_func_end();
    return 0;
}

bool ParentNode::waitfor_SubTreeInitDoneReports( void ) const
{
    int ret;
    initdonereport_sync.Lock();

    while( _num_children > _num_children_reported_init ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr, "Waiting for %u of %u subtree reports ...\n",
                               _num_children - _num_children_reported_init,
                               _num_children ));
        ret = initdonereport_sync.TimedWaitOnCondition( ALL_NODES_REPORTED, 
                _network->get_StartupTimeout()*1000 );
        if(ret != 0) {
            mrn_dbg( 3, mrn_printf(FLF, stderr, "TimedWait returned %d: %s\n", ret, strerror(ret)));
            initdonereport_sync.Unlock();
            return false;
        }
        mrn_dbg( 3, mrn_printf(FLF, stderr,
                               "%d of %d children have checked in.\n",
                               _num_children_reported_init, _num_children ));
    }

    initdonereport_sync.Unlock();

    mrn_dbg( 3, mrn_printf(FLF, stderr, "All %d children nodes have reported\n",
                _num_children )); 

    if( _network->is_LocalNodeFrontEnd() )
      _network->get_NetworkTopology()->update_Router_Table();
     
    return true;
}

int ParentNode::proc_FilterLoadEvent( PacketPtr ipacket ) const 
{
    /* Process messages about filter load status from children. */
    char ** hostnames;
    char ** so_filenames;
    unsigned * func_ids; 
    unsigned l1, l2, l3;
    ipacket->unpack("%as %as %aud",&hostnames, &l1, &so_filenames, 
        &l2, &func_ids, &l3);
    mrn_dbg( 3, mrn_printf(FLF, stderr, "recv packet ipacket with %ud elements\n",
                l1 )); 
    
    event_lock.Lock();
    _filter_children_reported++;

    // we have an error
    if (l1 > 0 && l2 > 0 && l3 > 0) {
        assert((l1 == l2) && (l2 == l3));
        for (unsigned u = 0; u < l1; u++){
            _filter_error_host.push_back(hostnames[u]);
            _filter_error_soname.push_back(so_filenames[u]);
            _filter_error_funcname.push_back(func_ids[u]);
        }
        mrn_dbg( 3, mrn_printf(FLF, stderr, "error in filter load from child\n" )); 
    }
    mrn_dbg( 3, mrn_printf(FLF, stderr, "Determining if sending packet or storing info (%ud - recv, %ud - expected)\n",
                _filter_children_reported, _filter_children_waiting + 1)); 
    if (_filter_children_waiting + 1 == _filter_children_reported && !_network->is_LocalNodeFrontEnd() ) {
        char ** comp_hostnames = &(_filter_error_host[0]);
        char ** so_names = &(_filter_error_soname[0]);
        unsigned * funcids = &(_filter_error_funcname[0]);
        unsigned length =  (unsigned) _filter_error_host.size();
        PacketPtr packet(new Packet(CTL_STRM_ID, PROT_EVENT, "%as %as %aud",
                             comp_hostnames, length, so_names, length, funcids, length));
        mrn_dbg( 3, mrn_printf(FLF, stderr, "Sending packet to parent\n")); 
        _network->send_PacketToParent(packet);
        _network->flush();
        mrn_dbg( 3, mrn_printf(FLF, stderr, "Finished sending packet to parent\n" )); 

    } else if (_network->is_LocalNodeFrontEnd() && _network->get_NumChildren() == _filter_children_reported) {
        mrn_dbg( 3, mrn_printf(FLF, stderr, "Signaling frontend that we are complete\n")); 

        _network->signal_ProtEvent(_filter_error_host, _filter_error_soname, _filter_error_funcname);
        mrn_dbg( 3, mrn_printf(FLF, stderr, "Signaling frontend completed\n"));
        _filter_children_reported = 0;
        _filter_error_host.clear();
        _filter_error_soname.clear();
        _filter_error_funcname.clear();
    }
    event_lock.Unlock();

    return 0;    
}

int ParentNode::proc_Event( PacketPtr ipacket) const
{
    /* NOTE: At some point in the future, as the need arises, we need 
             to convert this function to be more generic and process
             event messages from multiple sources. Not just filter load 
             events */
    return proc_FilterLoadEvent(ipacket);
}

Stream * ParentNode::proc_newStream( PacketPtr ipacket ) const
{
    Stream* stream;
    Rank* backends = NULL;
    uint32_t num_backends;
    unsigned int stream_id;
    int tag, ds_filter_id, us_filter_id, sync_id;
    bool wait_success;

    mrn_dbg_func_begin();

    tag = ipacket->get_Tag();

    if( tag == PROT_NEW_HETERO_STREAM ) {

        char *us_filters = NULL;
        char *sync_filters = NULL;
        char *ds_filters = NULL;
        Rank me = _network->get_LocalRank();

        if( ipacket->unpack("%ud %ad %s %s %s", 
                            &stream_id, &backends, &num_backends, 
                            &us_filters, &sync_filters, &ds_filters) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "unpack() failed\n") );
            return NULL;
        }
        
        if( ! Stream::find_FilterAssignment(std::string(us_filters), me, us_filter_id) ) {
            mrn_dbg( 3, mrn_printf(FLF, stderr, 
                                   "Stream::find_FilterAssignment(upstream) failed\n") );
            us_filter_id = TFILTER_NULL;
        }
        if( ! Stream::find_FilterAssignment(std::string(ds_filters), me, ds_filter_id) ) {
            mrn_dbg( 3, mrn_printf(FLF, stderr, 
                                   "Stream::find_FilterAssignment(downstream) failed\n") );
            ds_filter_id = TFILTER_NULL;
        }
        if( ! Stream::find_FilterAssignment(std::string(sync_filters), me, sync_id) ) {
            mrn_dbg( 3, mrn_printf(FLF, stderr, 
                                   "Stream::find_FilterAssignment(sync) failed\n") );
            sync_id = SFILTER_WAITFORALL;
        }
        
        if( us_filters != NULL )
            free( us_filters );

        if( sync_filters != NULL )
            free( sync_filters );

        if( ds_filters != NULL )
            free( ds_filters );
    } 
    else { // PROT_NEW_STREAM or PROT_NEW_INTERNAL_STREAM

        if( ipacket->unpack("%ud %ad %d %d %d", 
                            &stream_id, &backends, &num_backends, 
                            &us_filter_id, &sync_id, &ds_filter_id) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "unpack() failed\n") );
            return NULL;
        }
    }

    // Check the range of the filter ids, since they are stored as
    // an unsigned short int
    if(us_filter_id > UINT16_MAX || us_filter_id < 0) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Filter ID too large\n"));
        return NULL;
    }
    if(sync_id > UINT16_MAX || sync_id < 0) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Filter ID too large\n"));
        return NULL;
    }
    if(ds_filter_id > UINT16_MAX || ds_filter_id < 0) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Filter ID too large\n"));
        return NULL;
    }

    if( TOPOL_STRM_ID == stream_id ) {
        if( _network->is_LocalNodeInternal() ) {
            stream = _network->get_Stream( TOPOL_STRM_ID );
        }
	else {
            // register new stream w/ network
            stream = _network->new_Stream( stream_id, backends, num_backends, 
                                           (unsigned short)us_filter_id,
                                           (unsigned short)sync_id,
                                           (unsigned short)ds_filter_id );
        }
    }
    else {
        // register new stream w/ network
        stream = _network->new_Stream( stream_id, backends, num_backends, 
                                       (unsigned short)us_filter_id,
                                       (unsigned short)sync_id,
                                       (unsigned short)ds_filter_id );

        // send packet to children nodes
        if( _network->send_PacketToChildren(ipacket) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n") );
            return NULL;
        }

        // if internal, wait for stream to be created in entire subtree
        if( tag == PROT_NEW_INTERNAL_STREAM ) {

            // wait for acks
            if( ! waitfor_ControlProtocolAcks(PROT_NEW_STREAM_ACK,
                                              _network->get_NumChildren()) ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "waitfor_ControlProtocolAcks() failed\n" ));
                // Stream creation has failed, we should clean up
                _network->delete_Stream(stream_id);
                delete stream;
                stream = NULL;
                wait_success = false;
            } else {
                wait_success = true;
            }

            // send ack to parent, if any
            if( _network->is_LocalNodeChild() ) {
                if( ! _network->get_LocalChildNode()->ack_ControlProtocol(PROT_NEW_STREAM_ACK, wait_success) ) {
                    mrn_dbg( 1, mrn_printf(FLF, stderr, "ack_ControlProtocol(PROT_NEW_STREAM_ACK) failed\n" ));
                }
            }
        }
    }

    if( backends != NULL )
        free( backends );

    mrn_dbg_func_end();
    return stream;
}


int ParentNode::proc_FilterParams( FilterType ftype, PacketPtr &ipacket ) const
{
    unsigned int stream_id;

    mrn_dbg_func_begin();

    stream_id = ipacket->get_StreamId();
    Stream* strm = _network->get_Stream( stream_id );
    if( strm == NULL ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "stream %u lookup failed\n",
                               stream_id) );
        return -1;
    }

    // send packet to child nodes
    if( _network->send_PacketToChildren( ipacket ) == -1 ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n" ));
        return -1;
    }

    // local update
    strm->set_FilterParams( ftype, ipacket );

    mrn_dbg_func_end();
    return 0;
}

int ParentNode::proc_deleteStream( PacketPtr ipacket ) const
{
    mrn_dbg_func_begin();

    unsigned int stream_id = (*ipacket)[0]->get_uint32_t();

    if( _network->send_PacketToChildren( ipacket ) == -1 ) {
        mrn_dbg(2, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n"));
        return -1;
    }

    // delete only @ internal node, front-end ~Stream calls this function
    if( _network->is_LocalNodeInternal() ) {
        Stream * strm = _network->get_Stream( stream_id );
        if( strm == NULL ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "stream %u lookup failed\n",
                                   stream_id) );
            return -1;
        }
        delete strm;
    }

    mrn_dbg_func_end();
    return 0;
}

int ParentNode::proc_newFilter( PacketPtr ipacket ) const
{
    int retval = 0;
    uint32_t nfuncs = 0;
    char* so_file = NULL;
    char** funcs = NULL;
    unsigned short* fids = NULL;
    mrn_dbg_func_begin();

    retval = ipacket->unpack( "%s %as %auhd",
                              &so_file,
                              &funcs, &nfuncs,
                              &fids, &nfuncs );

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Starting load filter function\n")); 

    _filter_children_reported = 0;
    _filter_error_host.clear();
    _filter_error_soname.clear();
    _filter_error_funcname.clear();
    _filter_children_waiting = _network->get_NumChildren();
    mrn_dbg( 3, mrn_printf(FLF, stderr, "Sending filter load message to %ud children\n",
            _filter_children_waiting )); 
    _network->send_PacketToChildren( ipacket );

    std::vector<unsigned> error_funcs;
    if( retval == 0 ) {
        for( unsigned u=0; u < nfuncs; u++ ) {
            int rc = Filter::load_FilterFunc(_network->GetFilterInfo(), fids[u], so_file, funcs[u] );
            if( rc == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "Filter::load_FilterFunc(%s,%s) failed.\n",
                                       so_file, funcs[u]) );
                error_funcs.push_back(u);
            }
            free( funcs[u] );
        }

        // Record error/success in local node.
        if (error_funcs.size() > 0) {
            char ** hostnames = new char * [error_funcs.size()];
            unsigned * func_cstr = new unsigned[error_funcs.size()];
            char ** so_filename = new char * [error_funcs.size()];
            for (unsigned u = 0; u < error_funcs.size(); u++) {
                func_cstr[u] = error_funcs[u];
                hostnames[u] = strdup(_network->get_LocalHostName().c_str());
                so_filename[u] = strdup(so_file);
            }
            unsigned length = (unsigned) error_funcs.size();
            mrn_dbg( 3, mrn_printf(FLF, stderr, "Sending local error of packet loading falure\n")); 
            PacketPtr packet(new Packet(CTL_STRM_ID, PROT_EVENT, "%as %as %aud",
                                hostnames, length, so_filename, length, func_cstr, length));
            // Record error locally, we don't add directly to the local buffers 
            // to ensure that all children response are recieved before continuing. 
            proc_Event(packet);
        } else {
            char ** emptySend = NULL;
            unsigned ** func_empty = NULL;
            mrn_dbg( 3, mrn_printf(FLF, stderr, "local packet loaded correctly\n" )); 
            PacketPtr packet(new Packet(CTL_STRM_ID, PROT_EVENT, "%as %as %aud",
                                emptySend, 0, emptySend, 0, func_empty, 0));
            proc_Event(packet);
        }
        
        free( funcs );
        free( fids );
        free( so_file );
    }


    mrn_dbg_func_end();
    return retval;
}

bool lt_PeerNodePtr( PeerNode * p1, PeerNode * p2 )
{
    assert( p1 && p2 );

    if( p1->get_HostName( ) < p2->get_HostName( ) ) {
        return true;
    }
    else if( p1->get_HostName( ) == p2->get_HostName( ) ) {
        return ( p1->get_Port( ) < p2->get_Port( ) );
    }
    else {
        return false;
    }
}

bool equal_PeerNodePtr( PeerNode * p1, PeerNode * p2 )
{
    return p1 == p2;        //tmp hack, should be something like below

    if( p1->get_HostName( ) != p2->get_HostName( ) ) {
        return false;
    }
    else {
        return ( p1->get_Port( ) == p2->get_Port( ) );
    }
}

int ParentNode::proc_NewChildDataConnection( PacketPtr ipacket, int isock )
{
    char* topo_ptr = NULL;
    char* child_hostname_ptr = NULL;
    Port child_port;
    Rank child_rank, old_parent_rank;
    uint16_t child_incarnation;
    char is_internal_char;

    Rank my_rank = _network->get_LocalRank();
    NetworkTopology* nt = _network->get_NetworkTopology();

    ipacket->unpack( "%s %uhd %ud %uhd %ud %c %s",
                     &child_hostname_ptr,
                     &child_port,
                     &child_rank,
                     &child_incarnation,
                     &old_parent_rank,
                     &is_internal_char,
                     &topo_ptr ); 

    mrn_dbg(5, mrn_printf(FLF, stderr, 
                          "New child node[%s:%u:%u] (incarnation:%u) on socket %d\n",
                          child_hostname_ptr, child_rank, child_port,
                          child_incarnation, isock) );

    mrn_dbg(5, mrn_printf(FLF, stderr, "is_internal?: '%c'\n", is_internal_char ));
    bool is_internal = ( is_internal_char == 't' ? true : false );

    std::string child_hostname( child_hostname_ptr );
    PeerNodePtr child_node = _network->new_PeerNode( child_hostname,
                                                     child_port,
                                                     child_rank,
                                                     false,
                                                     is_internal );
    if( child_hostname_ptr != NULL )
        free( child_hostname_ptr );

    child_node->set_DataSocketFd( isock );


    if( child_incarnation == 1 ) {
        // propagate initial network settings
        const std::map< net_settings_key_t, std::string >& envMap = _network->get_SettingsMap();
        int* keys = (int*) calloc( envMap.size() + 1, sizeof(int) );
        char** vals = (char**) calloc( envMap.size() + 1, sizeof(char*) ); 
   
        unsigned int count = 0;
        std::map< net_settings_key_t, std::string >::const_iterator env_it = envMap.begin();
        for( ; env_it != envMap.end(); env_it++, count++ ) {
            keys[count] = env_it->first;
            vals[count] = strdup( (env_it->second).c_str() );
        }

        keys[ count ] = MRNET_FAILURE_RECOVERY;
        if( _network->_recover_from_failures )
            vals[ count ] = strdup( "1" );
        else 
            vals[ count ] = strdup( "0" );
        count++;

        // send current topology, before adding child's subtree
        std::string topo_str = nt->get_TopologyString();
        char* topo_dup = strdup( topo_str.c_str() );
        PacketPtr pkt( new Packet(CTL_STRM_ID, PROT_NET_SETTINGS, "%s %ad %as", 
                                  topo_dup,
                                  keys, count, 
                                  vals, count) );
        pkt->set_DestroyData( true );
        child_node->sendDirectly( pkt );
    }
    
    // add new child's subtree to my topology
    std::string child_topo(topo_ptr);
    mrn_dbg( 5, mrn_printf(FLF, stderr, "topology is %s before adding child subgraph %s\n", 
                           nt->get_TopologyString().c_str(),
                           child_topo.c_str()) );
    SerialGraph sg( child_topo );
    nt->add_SubGraph( my_rank, sg, true );
    if( topo_ptr != NULL )
        free( topo_ptr );

    //Create send/recv threads
    mrn_dbg( 5, mrn_printf(FLF, stderr, "Creating comm threads for new child\n") );
    child_node->start_CommunicationThreads();

    if( child_incarnation > 1 ) {
        //child's parent has failed
        mrn_dbg(3, mrn_printf(FLF, stderr,
                              "child[%s:%u]'s old parent: %u, new parent: %u\n",
                              child_hostname_ptr, child_rank,
                              old_parent_rank, my_rank ));

#if OLD_FAILURE_NOTIFICATION
        PacketPtr packet( new Packet(CTL_STRM_ID, PROT_RECOVERY_RPT, "%ud %ud %ud",
                                     child_rank,
                                     old_parent_rank,
                                     my_rank) );

        if( proc_RecoveryReport(packet) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_RecoveryReport() failed()\n") );
        }
#else
        // generate topology update for child reparenting
        if( _network->is_LocalNodeInternal() ) {
            Stream *s = _network->get_Stream( TOPOL_STRM_ID );
            if( s != NULL ) {
                int type = NetworkTopology::TOPO_CHANGE_PARENT; 
                Port dummy_port = UnknownPort;
                char* dummy_host = strdup("NULL"); // ugh, this needs to be fixed
                s->send_internal( PROT_TOPO_UPDATE, "%ad %aud %aud %as %auhd", 
                                  &type, 1, 
                                  &my_rank, 1, 
                                  &child_rank, 1, 
                                  &dummy_host, 1, 
                                  &dummy_port, 1 );
                free( dummy_host );
            }
        }
        else { // FE
            nt->update_changeParent( my_rank, child_rank, true );
        }
#endif
    }

    return 0;
}

int ParentNode::proc_RecoveryReport( PacketPtr ipacket ) const
{
    mrn_dbg_func_begin();

    Rank child_rank, failed_parent_rank, new_parent_rank;

    ipacket->unpack( "%ud %ud %ud",
                     &child_rank,
                     &failed_parent_rank,
                     &new_parent_rank );

    // remove node, but don't update datastructs since following procedure will
    _network->remove_Node( failed_parent_rank, false );
    _network->change_Parent( child_rank, new_parent_rank );

    // send packet to all children except the one on which the packet arrived
    if( _network->send_PacketToChildren(ipacket, false) == -1 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n"));
        return -1;
    }

    // internal nodes must report parent failure to parent
    if( _network->is_LocalNodeInternal() ) {
        _network->get_ParentNode()->send( ipacket );
        if( _network->get_ParentNode()->flush() == -1 ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "send()/flush() failed\n"));
            return -1;
        }
    }
    
    mrn_dbg_func_end();
    return 0;
}

void ParentNode::init_numChildrenExpected( SerialGraph& sg )
{
    _num_children = 0;

    sg.set_ToFirstChild();
    SerialGraph* currSubtree = sg.get_NextChild();
    for( ; currSubtree != NULL; currSubtree = sg.get_NextChild() )
        _num_children++;
}

void ParentNode::notify_OfChildFailure()
{
    failed_sync.Lock();
    _num_failed_children++;
    failed_sync.Unlock();
}

// Added by Taylor:
void ParentNode::set_numChildrenExpected( const int num_children )
{
    subtreereport_sync.Lock( );
    _num_children = num_children;
    subtreereport_sync.Unlock( );
}

} // namespace MRN
