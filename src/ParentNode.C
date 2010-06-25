/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
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
                           _rank, _hostname.c_str(), _port ));

    if( listeningSocket == -1 )
    {
        listening_sock_fd = 0; // force system to create socket and assign port
        _port = UnknownPort;

        mrn_dbg( 3, mrn_printf(FLF, stderr, "Calling bind_to_port(%d)\n", _port ));

        if( bindPort( &listening_sock_fd, &_port ) == -1 ) {
            error( ERR_SYSTEM, _rank, "bindPort(%d): %s\n", _port, strerror(errno) );
            mrn_dbg( 1, mrn_printf(FLF, stderr, "bind_to_port() failed\n" ));
            return;
        }
    }

    subtreereport_sync.RegisterCondition( ALLNODESREPORTED );
    
    mrn_dbg( 3, mrn_printf(FLF, stderr, "Leaving ParentNode()\n" ));
}

ParentNode::~ParentNode( void )
{
}

int ParentNode::proc_PacketsFromChildren( std::list< PacketPtr > & ipackets )
{
    int retval = 0;
    PacketPtr cur_packet;

    mrn_dbg_func_begin();

    std::list < PacketPtr >::iterator iter = ipackets.begin(  );
    for( ; iter != ipackets.end(  ); iter++ ) {
        if( proc_PacketFromChildren( *iter ) == -1 )
            retval = -1;
    }
 
    mrn_dbg( 3, mrn_printf(FLF, stderr, "proc_PacketsFromChildren() %s",
                           ( retval == -1 ? "failed\n" : "succeeded\n" ) ));
    ipackets.clear(  );
    return retval;
}

int ParentNode::proc_PacketFromChildren( PacketPtr cur_packet )
{
    int retval = 0;

    switch ( cur_packet->get_Tag(  ) ) {
    case PROT_CLOSE_STREAM:
        if( proc_closeStream( cur_packet ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "proc_closeStream() failed\n" ));
            retval = -1;
        }
        break;
    case PROT_NEW_SUBTREE_RPT:
        if( proc_newSubTreeReport( cur_packet ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "proc_newSubTreeReport() failed\n" ));
            retval = -1;
        }
        break;
    case PROT_SHUTDOWN_ACK:
        if( proc_DeleteSubTreeAck( cur_packet ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "proc_DeleteSubTreeAck() failed\n" ));
            retval = -1;
        }
        break;
    case PROT_TOPOLOGY_ACK:
        if( proc_TopologyReportAck( cur_packet ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "proc_TopologyReportAck() failed\n" ));
            retval = -1;
        }
        break;
    case PROT_EVENT:
        if( proc_Event( cur_packet ) == -1 ){
            mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_Event() failed\n" ));
            retval = -1;
        }
        break;
    case PROT_FAILURE_RPT:
        if( proc_FailureReport( cur_packet ) == -1 ){
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "proc_FailureReport() failed\n" ));
            retval = -1;
        }
        break;
    case PROT_RECOVERY_RPT:
        if( proc_RecoveryReport( cur_packet ) == -1 ){
            mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_RecoveryReport() failed\n" ));
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
    default:
        //Any unrecognized tag is assumed to be data
        if( proc_DataFromChildren( cur_packet ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "proc_DataFromChildren() failed\n" ));
            retval = -1;
        }
    }

    return retval;
}

bool ParentNode::waitfor_SubTreeReports( void ) const
{
    std::list < PacketPtr >packet_list;

    subtreereport_sync.Lock( );

    while( _num_children > _num_children_reported ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr, "Waiting for %u of %u subtree reports ...\n",
                               _num_children - _num_children_reported,
                               _num_children ));
        subtreereport_sync.WaitOnCondition( ALLNODESREPORTED );
        mrn_dbg( 3, mrn_printf(FLF, stderr,
                               "%d of %d children have checked in.\n",
                               _num_children_reported, _num_children ));
    }

    subtreereport_sync.Unlock( );

    mrn_dbg( 3, mrn_printf(FLF, stderr, "All %d children nodes have reported\n",
                _num_children ));
    return true;
}

bool ParentNode::waitfor_DeleteSubTreeAcks( void ) const
{
    mrn_dbg_func_begin();

    subtreereport_sync.Lock( );

    while( _num_children > _num_children_reported ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr, "Waiting for %u of %u delete subtree acks ...\n",
                               _num_children - _num_children_reported,
                               _num_children ));
        subtreereport_sync.WaitOnCondition( ALLNODESREPORTED );
        mrn_dbg( 3, mrn_printf(FLF, stderr,
                               "%d of %d children have ack'd.\n",
                               _num_children, _num_children_reported ));
    }

    subtreereport_sync.Unlock( );

    mrn_dbg_func_end();
    return true;
}

int ParentNode::proc_DeleteSubTree( PacketPtr ipacket ) const
{
    mrn_dbg_func_begin();

    subtreereport_sync.Lock( );
    
    _num_children_reported = _num_children = 0;
    const std::set < PeerNodePtr > peers = _network->get_ChildPeers();
    std::set < PeerNodePtr >::const_iterator iter;
    for( iter=peers.begin(); iter!=peers.end(); iter++ ) {
        if( (*iter)->is_child() ) {
            _num_children++;
        }
    }

    subtreereport_sync.Unlock( );

    //processes will be exiting -- disable failure recovery
    _network->disable_FailureRecovery();

    //Send ack to parent, if any
    if( _network->is_LocalNodeChild() ) {
        if( ! _network->get_LocalChildNode()->ack_DeleteSubTree() ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "ack_DeleteSubTree() failed\n" ));
        }
    }

    //send delete_subtree message to all children
    /* note: don't request flush as send threads will exit 
       before notifying flush completion */
    if( _network->send_PacketToChildren( ipacket ) == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n" ));
    }

    //wait for acks -- so children don't initiate failure recovery when we exit
    if( ! waitfor_DeleteSubTreeAcks() ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "waitfor_DeleteSubTreeAcks() failed\n" ));
    }

    //if internal, signal network termination
    if( _network->is_LocalNodeInternal() ) {
        _network->signal_ShutDown();

        // exit recv thread from parent
        mrn_dbg( 5, mrn_printf(FLF, stderr, "I'm going away now!\n" ));
        XPlat::Thread::Exit(NULL);
    }

    mrn_dbg_func_end();
    return 0;
}

bool ParentNode::waitfor_TopologyReportAcks( void ) const
{
    mrn_dbg_func_begin();

    subtreereport_sync.Lock( );

    while( _num_children > _num_children_reported ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr, "Waiting for %u of %u topol report acks ...\n",
                               _num_children - _num_children_reported,
                               _num_children ));
        subtreereport_sync.WaitOnCondition( ALLNODESREPORTED );
        mrn_dbg( 3, mrn_printf(FLF, stderr,
                               "%d of %d children have ack'd.\n",
                               _num_children, _num_children_reported ));
    }

    subtreereport_sync.Unlock( );

    mrn_dbg_func_end();
    return true;
}

int ParentNode::proc_TopologyReport( PacketPtr ipacket ) const
{
    mrn_dbg_func_begin();

    subtreereport_sync.Lock( );

    _num_children_reported = _num_children = 0;
    const std::set < PeerNodePtr > peers = _network->get_ChildPeers();
    std::set < PeerNodePtr >::const_iterator iter;
    for( iter=peers.begin(); iter!=peers.end(); iter++ ) {
        if( (*iter)->is_child() ) {
            _num_children++;
        }
    }

    subtreereport_sync.Unlock( );

    //send message to all children
    if( ( _network->send_PacketToChildren( ipacket ) == -1 ) ||
        ( _network->flush_PacketsToChildren( ) == -1 ) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "send/flush_PacketToChildren() failed\n" ));
        return -1;
    }

    //wait for acks
    if( ! waitfor_TopologyReportAcks() ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "waitfor_TopologyReportAcks() failed\n" ));
        return -1;
    }

    //Send ack to parent, if any
    if( _network->is_LocalNodeChild() ) {
        if( ! _network->get_LocalChildNode()->ack_TopologyReport() ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "ack_TopologyReport() failed\n" ));
            return -1;
        }
    }

    mrn_dbg_func_end();
    return 0;
}

int ParentNode::proc_newSubTreeReport( PacketPtr ipacket ) const
{
    mrn_dbg_func_begin();

    char * topo_ptr=NULL;

    ipacket->unpack( "%s", &topo_ptr ); 

    SerialGraph sg( topo_ptr );
    if( !_network->add_SubGraph( _network->get_LocalRank(), sg ) ){
        mrn_dbg(1, mrn_printf(FLF, stderr, "add_SubGraph() failed\n"));
        return -1;
    }

    subtreereport_sync.Lock( );

    if( _num_children_reported == _num_children ) {
        // already saw reports from known children, must be a newborn
        if( sg.is_RootBackEnd() ) {
            _num_children++;
            _num_children_reported++;
        }
        subtreereport_sync.Unlock( );
        if( _network->is_LocalNodeChild() ) {
            _network->get_LocalChildNode()->send_NewSubTreeReport();
        }
    }
    else {
        _num_children_reported++;
        mrn_dbg( 3, mrn_printf(FLF, stderr, "%d of %d descendants reported\n",
                               _num_children_reported, _num_children ));
        if( _num_children_reported == _num_children ) {
            subtreereport_sync.SignalCondition( ALLNODESREPORTED );
        }
        subtreereport_sync.Unlock( );
    }

    mrn_dbg_func_end();
    return 0;
}

int ParentNode::proc_DeleteSubTreeAck( PacketPtr /* ipacket */ ) const
{
    mrn_dbg_func_begin();

    subtreereport_sync.Lock( );
    _num_children_reported++;
    mrn_dbg( 3, mrn_printf(FLF, stderr, "%d of %d children ack'd\n",
                           _num_children_reported, _num_children ));
    if( _num_children_reported == _num_children ) {
        subtreereport_sync.SignalCondition( ALLNODESREPORTED );
    }
    subtreereport_sync.Unlock( );
	
    // exit recv thread from child
    mrn_dbg( 5, mrn_printf(FLF, stderr, "I'm going away now!\n" ));
    XPlat::Thread::Exit(NULL);

    return 0;
}

int ParentNode::proc_TopologyReportAck( PacketPtr /* ipacket */ ) const
{
    mrn_dbg_func_begin();

    subtreereport_sync.Lock( );
    _num_children_reported++;
    mrn_dbg( 3, mrn_printf(FLF, stderr, "%d of %d children ack'd\n",
                           _num_children_reported, _num_children ));
    if( _num_children_reported == _num_children ) {
        subtreereport_sync.SignalCondition( ALLNODESREPORTED );
    }
    subtreereport_sync.Unlock( );
	
    mrn_dbg_func_end();
    return 0;
}

int ParentNode::proc_Event( PacketPtr ipacket ) const
{
    char *edesc=NULL;
    EventType etype;
    Rank erank;

    mrn_dbg( 3, mrn_printf(FLF, stderr, "In parentnode.proc_Event()\n" ));
    if( ipacket->ExtractArgList( "%d %ud %s", &etype, &erank, &edesc ) == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "ExtractArgList failed\n" ));
        return -1;
    }

    //Event * event = new Event( etype, erank, edesc );
    //Event::add_Event( *event );
    //delete event;

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Leaving parentnode.proc_Event()\n" ));
    return 0;
}

Stream * ParentNode::proc_newStream( PacketPtr ipacket ) const
{
    unsigned int num_backends;
    Rank* backends;
    int stream_id, tag;
    int ds_filter_id, us_filter_id, sync_id;
    Stream* stream;

    mrn_dbg_func_begin();

    tag = ipacket->get_Tag();

    if( tag == PROT_NEW_HETERO_STREAM ) {

        char* us_filters;
        char* sync_filters;
        char* ds_filters;
        Rank me = _network->get_LocalRank();

        if( ipacket->ExtractArgList( "%d %ad %s %s %s", 
                                     &stream_id, &backends, &num_backends, 
                                     &us_filters, &sync_filters, &ds_filters ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "ExtractArgList() failed\n" ));
            return NULL;
        }
        
        if( ! Stream::find_FilterAssignment(std::string(us_filters), me, us_filter_id) ) {
            mrn_dbg( 3, mrn_printf(FLF, stderr, "Stream::find_FilterAssignment(upstream) failed, using default\n" ));
            us_filter_id = TFILTER_NULL;
        }
        if( ! Stream::find_FilterAssignment(std::string(ds_filters), me, ds_filter_id) ) {
            mrn_dbg( 3, mrn_printf(FLF, stderr, "Stream::find_FilterAssignment(downstream) failed, using default\n" ));
            ds_filter_id = TFILTER_NULL;
        }
        if( ! Stream::find_FilterAssignment(std::string(sync_filters), me, sync_id) ) {
            mrn_dbg( 3, mrn_printf(FLF, stderr, "Stream::find_FilterAssignment(sync) failed, using default\n" ));
            sync_id = SFILTER_WAITFORALL;
        }
        

    } else if( tag == PROT_NEW_STREAM ) {

        if( ipacket->ExtractArgList( "%d %ad %d %d %d", 
                                     &stream_id, &backends, &num_backends, 
                                     &us_filter_id, &sync_id, &ds_filter_id ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "ExtractArgList() failed\n" ));
            return NULL;
        }

    }
    //register new stream w/ network
    stream = _network->new_Stream( stream_id, backends, num_backends, 
                                   us_filter_id, sync_id, ds_filter_id );

    //send packet to children nodes
    if( _network->send_PacketToChildren(ipacket) == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n" ));
        return NULL;
    }

    mrn_dbg_func_end();
    return stream;
}


int ParentNode::proc_FilterParams( FilterType ftype, PacketPtr &ipacket ) const
{
    int stream_id;

    mrn_dbg_func_begin();

    stream_id = ipacket->get_StreamId();
    Stream* strm = _network->get_Stream( stream_id );
    if( strm == NULL ){
        mrn_dbg( 1, mrn_printf(FLF, stderr, "stream %d lookup failed\n",
                               stream_id ));
        return -1;
    }

    //send packet to child nodes
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
    int stream_id;

    mrn_dbg_func_begin();

    if( ipacket->ExtractArgList( "%d", &stream_id ) == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "ExtractArgList() failed\n" ));
        return -1;
    }

    if( _network->send_PacketToChildren( ipacket ) == -1 ) {
        mrn_dbg(2, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n"));
        return -1;
    }

    //delete only @ internal node, front-end stream destructor invokes this function
    if( _network->is_LocalNodeInternal() ) {
        Stream * strm = _network->get_Stream( stream_id );
        if( strm == NULL ){
            mrn_dbg( 1, mrn_printf(FLF, stderr, "stream %d lookup failed\n",
                                   stream_id ));
            return -1;
        }
        delete strm;
    }

    mrn_dbg_func_end();
    return 0;
}

int ParentNode::proc_newFilter( PacketPtr ipacket ) const
{
    int rc = 0;
    unsigned short fid = 0;
    const char *so_file = NULL, *func = NULL;

    mrn_dbg_func_begin();

    if( ipacket->ExtractArgList( "%uhd %s %s", &fid, &so_file, &func ) == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "ExtractArgList() failed\n" ));
        return -1;
    }

    rc = Filter::load_FilterFunc( fid, so_file, func );
    if( rc != 0 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                               "Filter::load_FilterFunc() failed.\n" ));
        rc = -1;
    }

    //Filter registered locally, now propagate to tree
    _network->send_PacketToChildren( ipacket );

    mrn_dbg_func_end();
    return rc;
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
    char * child_hostname_ptr=NULL;
    Port child_port;
    Rank child_rank, old_parent_rank;
    uint16_t child_incarnation;
    char is_internal_char;
    char * topo_ptr=NULL;

    ipacket->unpack( "%s %uhd %ud %uhd %ud %c %s",
                     &child_hostname_ptr,
                     &child_port,
                     &child_rank,
                     &child_incarnation,
                     &old_parent_rank,
                     &is_internal_char,
                     &topo_ptr ); 

    mrn_dbg(5, mrn_printf(FLF, stderr, "New child node[%s:%u:%u] (incarnation:%u) on socket %d\n",
                          child_hostname_ptr, child_rank, child_port,
                          child_incarnation, isock ));

    mrn_dbg(5, mrn_printf(FLF, stderr, "is_internal?: '%c'\n", is_internal_char ));
    bool is_internal = ( is_internal_char  ? true : false );

    std::string child_hostname( child_hostname_ptr );
    PeerNodePtr child_node = _network->new_PeerNode( child_hostname,
                                                     child_port,
                                                     child_rank,
                                                     false,
                                                     is_internal );

    child_node->set_DataSocketFd( isock );

    SerialGraph sg( topo_ptr );
    if( !_network->add_SubGraph( _network->get_LocalRank(), sg ) ){
        mrn_dbg(5, mrn_printf(FLF, stderr, "add_SubGraph() failed\n"));
        return -1;
    }

    //Create send/recv threads
    mrn_dbg(5, mrn_printf(FLF, stderr, "Creating comm threads for new child\n" ));
    child_node->start_CommunicationThreads();

    if( child_incarnation > 1 ) {
        //child's parent has failed
        mrn_dbg(5, mrn_printf(FLF, stderr,
                              "child[%s:%u]'s old parent: %u, new parent: %u\n",
                              child_hostname_ptr, child_rank,
                              old_parent_rank, _network->get_LocalRank() ));

        PacketPtr packet( new Packet( 0, PROT_RECOVERY_RPT, "%ud %ud %ud",
                                      child_rank,
                                      old_parent_rank,
                                      _network->get_LocalRank() ) );

        if( proc_RecoveryReport( packet ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_RecoveryReport() failed()\n" ));
        }
    }

    return 0;
}

int ParentNode::proc_FailureReport( PacketPtr ipacket ) const
{
    Rank failed_rank;

    ipacket->unpack( "%ud", &failed_rank ); 

    mrn_dbg( 5, mrn_printf(FLF, stderr, "node[%u] has failed\n", failed_rank ));

    //update local topology
    ParentNode::_network->remove_Node( failed_rank );

    //propagate to children except on incident channel
    if( _network->send_PacketToChildren( ipacket, false ) == -1 ){
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                               "send_PacketToChildren() failed()\n" ));
        return -1;
    }

    if( _network->is_LocalNodeChild() ) {
        //propagate to parent
        if( ( _network->send_PacketToParent( ipacket ) == -1 ) ||
            ( _network->flush_PacketsToChildren( ) == -1 ) ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "parent.send/flush() failed()\n" ));
            return -1;
        }
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

    //remove node, but don't update datastructs since following procedure will
    _network->remove_Node( failed_parent_rank, false );
    _network->change_Parent( child_rank, new_parent_rank );

    //send packet to all children except the one on which the packet arrived
    if( _network->send_PacketToChildren( ipacket, false ) == -1 ){
        mrn_dbg(1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n"));
        return -1;
    }

    //Internal nodes must report parent failure to parent
    if( _network->is_LocalNodeInternal() ) {
        if( ( _network->get_ParentNode()->send( ipacket ) == -1 ) ||
            ( _network->get_ParentNode()->flush( ) == -1 ) ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "send()/flush() failed\n"));
            return -1;
        }
    }
    
    mrn_dbg_func_end();
    return 0;
}

int ParentNode::proc_closeStream( PacketPtr ipacket ) const
{
    int stream_id;
    ipacket->unpack( "%d", &stream_id );

    Stream * stream = _network->get_Stream( stream_id );
    if( stream == NULL ){
        mrn_dbg( 1, mrn_printf(FLF, stderr, "stream %d lookup failed\n",
                               stream_id ));
        return -1;
    }

    stream->close_Peer( ipacket->get_InletNodeRank() );

    if( stream->is_Closed() ) {
        if( _network->is_LocalNodeChild() ) {
            //Internal Nodes should propagate "close"
            if( _network->send_PacketToParent( ipacket ) == -1 ) {
                mrn_dbg(1, mrn_printf(FLF, stderr, "send() failed\n"));
                return -1;
            }
        }
        else {
            //Front-end node should awaken any folks blocked on recv()
            assert( _network->is_LocalNodeFrontEnd() );
            stream->signal_BlockedReceivers();
        }
    }
    
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
