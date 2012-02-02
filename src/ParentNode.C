/****************************************************************************
 *  Copyright 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <set>

#include "config.h"
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

namespace MRN
{

/*====================================================*/
/*  ParentNode CLASS METHOD DEFINITIONS            */
/*====================================================*/
ParentNode::ParentNode( Network* inetwork,
                        std::string const& myhost,
                        Rank myrank,
                        int listeningSocket,
                        Port listeningPort )
    : CommunicationNode( myhost, listeningPort, myrank ),
      _network(inetwork),
      _num_children( 0 ),
      _num_children_reported( 0 ),
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

    if( (tag >= FirstSystemTag) && (tag < PROT_LAST) ) {
        switch ( cur_packet->get_Tag() ) {
        case PROT_SUBTREE_INITDONE_RPT:
            if( proc_SubTreeInitDoneReport(cur_packet) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "proc_SubTreeInitDoneReport() failed\n" ));
                retval = -1;
            }
            break;
        case PROT_SHUTDOWN_ACK:
            if( proc_DeleteSubTreeAck(cur_packet) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "proc_DeleteSubTreeAck() failed\n" ));
                retval = -1;
            }
            break;
        case PROT_NEW_STREAM_ACK:
            if( proc_ControlProtocolAck(PROT_NEW_STREAM_ACK) == -1 ) {
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

int ParentNode::proc_PortUpdates( PacketPtr ) const
{
    return 1;
}

bool ParentNode::waitfor_ControlProtocolAcks( int ack_tag, 
                                              unsigned num_acks_expected ) const
{    
    mrn_dbg_func_begin();
    
    if( num_acks_expected > 0 ) {

        XPlat::Monitor wait_sync;
        std::map< int, std::pair< XPlat::Monitor*, unsigned > >::iterator cps_iter;

        wait_sync.RegisterCondition( NODE_REPORTED );

        cps_sync.Lock();
        cps_iter = ctl_protocol_syncs.find( ack_tag );
        if( cps_iter == ctl_protocol_syncs.end() ) {
            std::pair< XPlat::Monitor*, unsigned >& wait_info = ctl_protocol_syncs[ ack_tag ];
            wait_info.first = &wait_sync;
            wait_info.second = 0;
            cps_iter = ctl_protocol_syncs.find( ack_tag );
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

        unsigned num_acked = cps_iter->second.second;

        while( num_acked < num_acks_expected ) {

            mrn_dbg(5, mrn_printf(FLF, stderr, 
                                  "Waiting for %u of %u protocol(%d) acks ...\n",
                                  num_acks_expected - num_acked, 
                                  num_acks_expected, 
                                  ack_tag));
            wait_sync.WaitOnCondition( NODE_REPORTED );
 
            num_acked = cps_iter->second.second;    
            mrn_dbg(3, mrn_printf(FLF, stderr,
                                  "%d of %d have ack'd protocol(%d).\n",
                                  num_acked, num_acks_expected, ack_tag));
        }

        wait_sync.Unlock();

        cps_sync.Lock();
        ctl_protocol_syncs.erase( cps_iter );
        cps_sync.Unlock();
    }

    mrn_dbg_func_end();
    return true;
}

int ParentNode::proc_ControlProtocolAck( int ack_tag ) const
{
    mrn_dbg_func_begin();

    XPlat::Monitor* wait_sync = NULL;

    std::map< int, std::pair< XPlat::Monitor*, unsigned > >::iterator cps_iter;
    do {
        cps_sync.Lock();
        cps_iter = ctl_protocol_syncs.find( ack_tag );
        if( cps_iter != ctl_protocol_syncs.end() ) {
            wait_sync = (*cps_iter).second.first;
        }
        else {
            // an ack can arrive before we have even started waiting
            mrn_dbg(5, mrn_printf(FLF, stderr, 
                                  "ctl_protocol_sync for tag %d was not found, spin-waiting\n", 
                                  ack_tag));
        }
        cps_sync.Unlock();
    } while( wait_sync == NULL );

    wait_sync->Lock();
    (*cps_iter).second.second++;
    wait_sync->SignalCondition( NODE_REPORTED );
    wait_sync->Unlock();

    mrn_dbg_func_end();
    return 0;
}

bool ParentNode::waitfor_SubTreeReports( void ) const
{
    subtreereport_sync.Lock( );

    while( _num_children > _num_children_reported ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr, "Waiting for %u of %u subtree reports ...\n",
                               _num_children - _num_children_reported,
                               _num_children ));
        subtreereport_sync.WaitOnCondition( ALL_NODES_REPORTED );
        mrn_dbg( 3, mrn_printf(FLF, stderr,
                               "%d of %d children have checked in.\n",
                               _num_children_reported, _num_children ));
    }

    subtreereport_sync.Unlock( );

    mrn_dbg( 3, mrn_printf(FLF, stderr, "All %d children nodes have reported\n",
                _num_children ));
    return true;
}

int ParentNode::proc_DeleteSubTree( PacketPtr ipacket ) const
{
    mrn_dbg_func_begin();

    _network->set_ShuttingDown();

    subtreereport_sync.Lock( );

    _num_children_reported = 0;
    _num_children = _network->get_NumChildren();

    subtreereport_sync.Unlock( );

    // processes will be exiting -- disable failure recovery
    _network->disable_FailureRecovery();

    // send to all children
    /* note: don't request flush as send threads will exit 
       before notifying flush completion */
    if( _network->send_PacketToChildren(ipacket) == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n" ));
    }

    // wait for acks -- so children don't initiate failure recovery when we exit
    if( ! waitfor_DeleteSubTreeAcks() ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "waitfor_DeleteSubTreeAcks() failed\n" ));
    }

    // send ack to parent, if any
    if( _network->is_LocalNodeChild() ) {
        if( ! _network->get_LocalChildNode()->ack_DeleteSubTree() ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "ack_DeleteSubTree() failed\n" ));
        }
        else {
            // wait for send thread to finish
            PeerNodePtr parent = _network->get_ParentNode();
            if( parent != NULL ) {
                mrn_dbg( 5, mrn_printf(FLF, stderr, 
                                       "waiting for parent send thread to finish\n") );
                XPlat::Thread::Id send_id = parent->get_SendThrId();
                if( send_id )
                    XPlat::Thread::Join( send_id, (void**)NULL );
            }
        }
    }

    // if internal, signal network termination
    if( _network->is_LocalNodeInternal() ) {
        _network->signal_ShutDown();

        // exit recv/EDT thread
        mrn_dbg( 5, mrn_printf(FLF, stderr, "I'm going away now!\n" ));
        _network->free_ThreadState();
        XPlat::Thread::Exit(NULL);
    }

    mrn_dbg_func_end();
    return 0;
}

int ParentNode::proc_DeleteSubTreeAck( PacketPtr ) const
{
    mrn_dbg_func_begin();

    subtreereport_sync.Lock();

    _num_children_reported++;
    mrn_dbg(3, mrn_printf(FLF, stderr, "%d of %d children ack'd\n",
                          _num_children_reported, _num_children));

    if( _num_children_reported == _num_children ) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "Signaling ALL_NODES_REPORTED\n"));
        subtreereport_sync.SignalCondition( ALL_NODES_REPORTED );
        mrn_dbg(5, mrn_printf(FLF, stderr, "Signaling done\n"));
    }

    subtreereport_sync.Unlock();
	
    // exit recv thread from child
    mrn_dbg(5, mrn_printf(FLF, stderr, "I'm going away now!\n"));
    tsd_t* tsd = (tsd_t*)tsd_key.Get();
    if( tsd != NULL ) {
        tsd_key.Set( NULL );
        free( const_cast<char*>( tsd->thread_name ) );
        delete tsd;
    }
    XPlat::Thread::Exit(NULL);

    return 0;
}

bool ParentNode::waitfor_DeleteSubTreeAcks( void ) const
{
    mrn_dbg_func_begin();

    subtreereport_sync.Lock();

    while( _num_children > _num_children_reported ) {
        mrn_dbg(3, mrn_printf(FLF, stderr, "Waiting for %u of %u delete subtree acks ...\n",
                               _num_children - _num_children_reported,
                               _num_children));
        subtreereport_sync.WaitOnCondition( ALL_NODES_REPORTED );
        mrn_dbg(3, mrn_printf(FLF, stderr,
                               "%d of %d children have ack'd.\n",
                               _num_children, _num_children_reported));
    }

    subtreereport_sync.Unlock();

    mrn_dbg_func_end();
    return true;
}

int ParentNode::proc_SubTreeInitDoneReport( PacketPtr ) const
{
    mrn_dbg_func_begin();

    initdonereport_sync.Lock();

    if( _num_children_reported == _num_children ) {
        // already saw reports from known children, must be a newborn
        _num_children++;
        _num_children_reported++;
    }
    else {
        _num_children_reported++;
        mrn_dbg(3, mrn_printf(FLF, stderr, "%d of %d descendants reported\n",
                              _num_children_reported, _num_children));
        if( _num_children_reported == _num_children ) {
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
    initdonereport_sync.Lock();

    while( _num_children > _num_children_reported ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr, "Waiting for %u of %u subtree reports ...\n",
                               _num_children - _num_children_reported,
                               _num_children ));
        initdonereport_sync.WaitOnCondition( ALL_NODES_REPORTED );
        mrn_dbg( 3, mrn_printf(FLF, stderr,
                               "%d of %d children have checked in.\n",
                               _num_children_reported, _num_children ));
    }

    initdonereport_sync.Unlock();

    mrn_dbg( 3, mrn_printf(FLF, stderr, "All %d children nodes have reported\n",
                _num_children )); 

    if( _network->is_LocalNodeFrontEnd() )
      _network->get_NetworkTopology()->update_Router_Table();
     
    return true;
}

int ParentNode::proc_Event( PacketPtr ) const
{
    /* NOTE: we need a sensible method for reporting errors to the FE,
             e.g., an error reporting stream. until we have this,
             there's nothing to do here -- Oct 2010 */
    return 0;
}

Stream * ParentNode::proc_newStream( PacketPtr ipacket ) const
{
    Stream* stream;
    Rank* backends = NULL;
    uint64_t num_backends;
    unsigned int stream_id;
    int tag, ds_filter_id, us_filter_id, sync_id;

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

    // register new stream w/ network
    stream = _network->new_Stream( stream_id, backends, num_backends, 
                                   us_filter_id, sync_id, ds_filter_id );

    if( backends != NULL )
        free( backends );

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
            mrn_dbg( 1, mrn_printf(FLF, stderr, "waitfor_DeleteSubTreeAcks() failed\n" ));
        }

        // send ack to parent, if any
        if( _network->is_LocalNodeChild() ) {
            if( ! _network->get_LocalChildNode()->ack_ControlProtocol(PROT_NEW_STREAM_ACK) ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "ack_ControlProtocol(PROT_NEW_STREAM_ACK) failed\n" ));
            }
        }
    }

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
    unsigned nfuncs = 0;
    char* so_file = NULL;
    char** funcs = NULL;
    unsigned short* fids = NULL;
    uint64_t discard;
    mrn_dbg_func_begin();

    retval = ipacket->unpack( "%s %as %auhd",
                              &so_file,
                              &funcs, &nfuncs,
                              &fids, &discard );

    // propagate before local load
    _network->send_PacketToChildren( ipacket );

    if( retval == 0 ) {
        for( unsigned u=0; u < nfuncs; u++ ) {
            int rc = Filter::load_FilterFunc( fids[u], so_file, funcs[u] );
            if( rc == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "Filter::load_FilterFunc(%s,%s) failed.\n",
                                       so_file, funcs[u]) );
                // TODO: notify FE of failure to load
            }
            free( funcs[u] );
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

    child_node->set_DataSocketFd( isock );
    if( child_hostname_ptr != NULL )
        free( child_hostname_ptr );

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

    NetworkTopology* nt = _network->get_NetworkTopology();
    std::string topo_str = nt->get_TopologyString();

    PacketPtr pkt( new Packet(CTL_STRM_ID, PROT_NET_SETTINGS, "%s %ad %as", 
                              strdup( topo_str.c_str() ),
                              keys, uint64_t(count), 
                              vals, count) );
    pkt->set_DestroyData( true );
    child_node->sendDirectly( pkt );
    
    Rank my_rank = _network->get_LocalRank();
    if( NULL == nt->find_Node(child_rank) ) {
        std::string child_topo(topo_ptr);
        mrn_dbg( 5, mrn_printf(FLF, stderr, "topology is %s before adding child subgraph %s\n", 
                               nt->get_TopologyString().c_str(),
                               child_topo.c_str()) );
        SerialGraph sg( child_topo );
        nt->add_SubGraph( my_rank, sg, true );
    }
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
                                  &type, uint64_t(1), 
                                  &my_rank, uint64_t(1), 
                                  &child_rank, uint64_t(1), 
                                  &dummy_host, 1, 
                                  &dummy_port, uint64_t(1) );
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


} // namespace MRN
