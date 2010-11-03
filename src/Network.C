/****************************************************************************
 *  Copyright 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <sys/types.h>
#ifndef os_windows
#include <sys/socket.h>
#endif

#include "config.h"
#include "utils.h"
#include "BackEndNode.h"
#include "ChildNode.h"
#include "EventDetector.h"
#include "Filter.h"
#include "FrontEndNode.h"
#include "InternalNode.h"
#include "ParentNode.h"
#include "ParsedGraph.h"
#include "PeerNode.h"
#include "TimeKeeper.h"

#include "mrnet/MRNet.h"
#include "xplat/NetUtils.h"
#include "xplat/SocketUtils.h"
#include "xplat/Thread.h"
using namespace XPlat;

extern FILE *mrnin;

using namespace std;

namespace MRN
{

int mrnparse( );

extern const char* mrnBufPtr;
extern unsigned int mrnBufRemaining;

const int MIN_OUTPUT_LEVEL=0;
const int MAX_OUTPUT_LEVEL=5;
int CUR_OUTPUT_LEVEL=1;

const Port UnknownPort = (Port)-1;
const Rank UnknownRank = (Rank)-1;

const char *empty_str="";

void set_OutputLevelFromEnvironment(void);

void init_local(void)
{
#if !defined(os_windows)
    // Make sure any data sockets we create have descriptors != {1,2}
    // to avoid nasty bugs where fprintf(stdout/stderr) writes to data sockets.
    int fd;
    while( (fd = socket( AF_INET, SOCK_STREAM, 0 )) <= 2 ) {
        /* Code for closing descriptor on exec*/
        int fdflag = fcntl(fd, F_GETFD );
        if( fdflag == -1 )
        {
            // failed to retrive the fd  flags
	    fprintf(stderr, "F_GETFD failed!\n");
        }
        int fret = fcntl( fd, F_SETFD, fdflag | FD_CLOEXEC );
        if( fret == -1 )
        {
            // we failed to set the fd flags
           fprintf(stderr, "F_SETFD failed!\n");
        }

        if( fd == -1 ) break;
    } 
    if( fd > 2 ) XPlat::SocketUtils::Close(fd);

#else
    // init Winsock
    WORD version = MAKEWORD(2, 2); /* socket version 2.2 supported by all modern Windows */
    WSADATA data;
    if( WSAStartup(version, &data) != 0 )
        fprintf(stderr, "WSAStartup failed!\n");
#endif
}

void cleanup_local(void)
{
#if defined(os_windows)
    // cleanup Winsock
    WSACleanup();
#endif
}

/*===========================================================*/
/*             Network DEFINITIONS        */
/*===========================================================*/
Network::Network(void)
    : _local_port(UnknownPort), _local_rank(UnknownRank),_network_topology(NULL), 
      _failure_manager(NULL), _bcast_communicator(NULL), 
      _local_front_end_node(NULL), _local_back_end_node(NULL), 
      _local_internal_node(NULL), _local_time_keeper( new TimeKeeper() ),
      _edt( new EventDetector(this) ), next_stream_id(1), _evt_mgr( new EventMgr() ),
      _threaded(true), _recover_from_failures(true), 
      _terminate_backends(true), _was_shutdown(false)
{
    init_local();

    set_OutputLevelFromEnvironment();

    _shutdown_sync.RegisterCondition( NETWORK_TERMINATION );
}

Network::~Network(void)
{
    shutdown_Network( );
    cleanup_local( );
    if( parsed_graph != NULL ) {
        delete parsed_graph;
        parsed_graph = NULL;
    }
}

void Network::close_Streams(void)
{
    map< unsigned int, Stream* >::iterator iter;
    _streams_sync.Lock();
    for( iter=_streams.begin(); iter != _streams.end(); iter++ ) {
        (*iter).second->close();
    }
    _streams_sync.Unlock();
}

void Network::shutdown_Network(void)
{
    if( ! is_ShutDown() ) {

        _shutdown_sync.Lock();
        _was_shutdown = true;
        _shutdown_sync.Unlock();

        // kill streams
        close_Streams();
    
        XPlat::Thread::Id my_id = 0;
        tsd_t *tsd = ( tsd_t * )tsd_key.Get();
        if( tsd != NULL )
            my_id = tsd->thread_id;

        if( is_LocalNodeFrontEnd() ) {

            if( ( _network_topology != NULL ) && 
                ( _network_topology->get_NumNodes() > 0 ) ) {

                char delete_backends = 'f';
                if( _terminate_backends )
                    delete_backends = 't';
            
                PacketPtr packet( new Packet(0, PROT_SHUTDOWN, 
                                             "%c", delete_backends) );
                get_LocalFrontEndNode()->proc_DeleteSubTree( packet );
            }
        }
        else if( is_LocalNodeBackEnd() ) {

            if( _parent != PeerNode::NullPeerNode ) {
                // send shutdown notice to parent, which will cause send thread to exit
                if( ! get_LocalChildNode()->ack_DeleteSubTree() ) {
                    mrn_dbg( 1, mrn_printf(FLF, stderr, "ack_DeleteSubTree() failed\n" ));
                }

                // cancel recv thread, if that's not this thread
                if( _parent->recv_thread_id != my_id ) {
                    // turn off debug output to prevent mrn_printf deadlock
                    MRN::set_OutputLevel( -1 );
                    XPlat::Thread::Cancel( _parent->recv_thread_id );
                }

                // let send/recv threads exit
                sleep(1);
            }
        }

        // tell EDT to go away, if that's not this thread
        if( _edt != NULL ) {
            if( _edt->_thread_id != my_id)
                _edt->stop();
            delete _edt;
            _edt = NULL;
        }
        
        if( _network_topology != NULL ) {
            string empty("");
            reset_Topology(empty);
        }

        _evt_mgr->clear_Events();

        signal_ShutDown();        
    }
}

bool Network::is_ShutDown(void) const
{
    bool rc;
    _shutdown_sync.Lock();
    rc = _was_shutdown;
    _shutdown_sync.Unlock();
    return rc;
}

void Network::waitfor_ShutDown(void) const
{
    mrn_dbg_func_begin();

    _shutdown_sync.Lock();
    
    while( ! _was_shutdown )
        _shutdown_sync.WaitOnCondition( NETWORK_TERMINATION );

    _shutdown_sync.Unlock();

    mrn_dbg_func_end();
}

void Network::signal_ShutDown(void)
{
    mrn_dbg_func_begin();

    // make sure _was_shutdown has been set to true
    _shutdown_sync.Lock();
    if( ! _was_shutdown )
        _was_shutdown = true;
    _shutdown_sync.Unlock();

    // notify any blocking recv() calls
    _streams_sync.Lock();
    _streams_sync.SignalCondition( STREAMS_NONEMPTY );
    _streams_sync.Unlock();

    _shutdown_sync.Lock();
    _shutdown_sync.SignalCondition( NETWORK_TERMINATION );
    _shutdown_sync.Unlock();

    mrn_dbg_func_end();
}

const char* Network::FindCommnodePath(void)
{
    const char* path = getenv( "MRN_COMM_PATH" );
    if( path == NULL )
        path = COMMNODE_EXE;
    return path;
}

// deprecated: back-ends should call exit() after waitfor_ShutDown()
//             if termination is desired
void Network::set_TerminateBackEndsOnShutdown( bool terminate )
{
    mrn_printf(FLF, stderr, 
               "Network::set_TerminateBackEndsOnShutdown() is deprecated.\n");
    _terminate_backends = terminate;
}

void Network::update_BcastCommunicator(void)
{
    //add end-points to broadcast communicator
    _endpoints_mutex.Lock();

    if( _bcast_communicator == NULL )
        _bcast_communicator = new Communicator( this );

    map< Rank, CommunicationNode * >::const_iterator iter;
    for( iter=_end_points.begin(); iter!=_end_points.end(); iter++ )
        _bcast_communicator->add_EndPoint( iter->first );

    _endpoints_mutex.Unlock();
    mrn_dbg(5, mrn_printf(FLF, stderr, "Bcast communicator complete \n" ));
}

// initialize a Network in its role as front end
void Network::init_FrontEnd( const char * itopology,
                             const char * ibackend_exe,
                             const char **ibackend_args,
                             const std::map<std::string,std::string>* iattrs,
                             bool irank_backends,
                             bool iusing_mem_buf )
{
    _streams_sync.RegisterCondition( STREAMS_NONEMPTY );
    _parent_sync.RegisterCondition( PARENT_NODE_AVAILABLE );

    if( parse_Configuration( itopology, iusing_mem_buf ) == -1 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "Failure: parse_Configuration( \"%s\" )!\n",
                              ( iusing_mem_buf ? "memory buffer" : itopology )));
        return;
    }

    if( ! parsed_graph->validate( ) ) {
        parsed_graph->perror( "ParsedGraph not valid." );
        return;
    }

    parsed_graph->assign_NodeRanks( irank_backends );

    Rank rootRank = parsed_graph->get_Root( )->get_Rank();
    _local_rank = rootRank;

    string prettyHost, rootHost;
    rootHost = parsed_graph->get_Root()->get_HostName();
    XPlat::NetUtils::GetHostName( rootHost, prettyHost );
    std::ostringstream nameStream;
    nameStream << "FE("
                << prettyHost
                << ":"
                << rootRank
                << ")"
                << std::ends;

    if( ! XPlat::NetUtils::IsLocalHost( prettyHost ) ) {
	string lhost;
	XPlat::NetUtils::GetLocalHostName(lhost);
        mrn_dbg( 1, mrn_printf(FLF, stderr, 
                               "WARNING: Topology Root (%s) is not local host (%s)\n",
                               prettyHost.c_str(), lhost.c_str()) );
    }

    //TLS: setup thread local storage for frontend
    int status;
    tsd_t *local_data = new tsd_t;
    local_data->thread_id = XPlat::Thread::GetId( );
    local_data->thread_name = strdup( nameStream.str().c_str() );
    local_data->process_rank = _local_rank;
    local_data->node_type = FE_NODE;

    status = tsd_key.Set( local_data );

    if( status != 0 ) {
        error( ERR_SYSTEM, rootRank, 
               "XPlat::TLSKey::Set(): %s\n", strerror( status ) );
        return;
    }

    _bcast_communicator = new Communicator( this );
    
    FrontEndNode* fen = CreateFrontEndNode( this, rootHost, rootRank );
    assert( fen != NULL );
    if( fen->has_Error() ) 
        error( ERR_SYSTEM, rootRank, 
               "Failed to initialize via CreateFrontEndNode()\n" );

    const char* mrn_commnode_path = FindCommnodePath();
    assert( mrn_commnode_path != NULL );
    
    if( ibackend_exe == NULL ) {
        ibackend_exe = empty_str;
    }
    unsigned int backend_argc=0;
    if( ibackend_args != NULL ){
        for(unsigned int i=0; ibackend_args[i] != NULL; i++){
            backend_argc++;
        }
    }

    // spawn and connect processes that constitute our network
    this->Instantiate( parsed_graph, 
                       mrn_commnode_path, 
                       ibackend_exe, 
                       ibackend_args, 
                       backend_argc,
                       iattrs );
    delete parsed_graph;
    parsed_graph = NULL;

    mrn_dbg(5, mrn_printf(FLF, stderr, "Waiting for subtrees to report ... \n" ));
    
    if( ! get_LocalFrontEndNode()->waitfor_SubTreeInitDoneReports() )
            error( ERR_INTERNAL, rootRank, "waitfor_SubTreeReports() failed");
   

    mrn_dbg(5, mrn_printf(FLF, stderr, "Updating bcast communicator ... \n" ));
    update_BcastCommunicator( );

    // create topology propagation stream
    Stream* s = new_Stream( _bcast_communicator, TFILTER_TOPO_UPDATE, 
                            SFILTER_TIMEOUT, TFILTER_TOPO_UPDATE_DOWNSTREAM );
    assert(s->get_Id() == 1);
    s->set_FilterParameters( FILTER_UPSTREAM_SYNC, "%ud", 250 );

    /* collect port updates and broadcast them
     * - this is a no-op on XT
     */
    PacketPtr packet( new Packet( 0, PROT_PORT_UPDATE, "" ) );
    if( -1 == get_LocalFrontEndNode()->proc_PortUpdates( packet ) )
        error( ERR_INTERNAL, rootRank, "proc_PortUpdates() failed");
}

void Network::send_TopologyUpdates(void)
{
    _network_topology->send_updates_buffer();
}


void Network::init_BackEnd(const char *iphostname, Port ipport, Rank iprank,
                           const char *imyhostname, Rank imyrank )
{
    _streams_sync.RegisterCondition( STREAMS_NONEMPTY );
    _parent_sync.RegisterCondition( PARENT_NODE_AVAILABLE );

    _local_rank = imyrank;

    string prettyHost, myhostname;
    XPlat::NetUtils::GetNetworkName( imyhostname, myhostname );
    XPlat::NetUtils::GetHostName( myhostname, prettyHost );
    std::ostringstream nameStream;
    nameStream << "BE("
            << prettyHost
            << ":"
            << imyrank
            << ")"
            << std::ends;

    //TLS: setup thread local storage for backend
    int status;
    tsd_t *local_data = new tsd_t;
    local_data->thread_id = XPlat::Thread::GetId();
    local_data->thread_name = strdup( nameStream.str().c_str() );
    local_data->process_rank = _local_rank;
    local_data->node_type = BE_NODE;
    if( ( status = tsd_key.Set( local_data ) ) != 0 ) {
        //TODO: add event to notify upstream
        error( ERR_SYSTEM, imyrank, "XPlat::TLSKey::Set(): %s\n", strerror( status ) );
        mrn_dbg( 1, mrn_printf(FLF, stderr, "XPlat::TLSKey::Set(): %s\n",
                    strerror( status ) ));
    }

    BackEndNode* ben = CreateBackEndNode( this, myhostname, imyrank,
                                         iphostname, ipport, iprank );
    assert( ben != NULL );
    if( ben->has_Error() ) 
        error( ERR_SYSTEM, imyrank, "Failed to initialize via CreateBackEndNode()\n" );
}

void Network::init_InternalNode( const char* iphostname,
                                 Port ipport,
                                 Rank iprank,
                                 const char* imyhostname,
                                 Rank imyrank,
                                 int idataSocket,
                                 Port idataPort )
{
    _parent_sync.RegisterCondition( PARENT_NODE_AVAILABLE );

    _local_rank = imyrank;

    string prettyHost, myhostname;
    XPlat::NetUtils::GetNetworkName( imyhostname, myhostname );
    XPlat::NetUtils::GetHostName( myhostname, prettyHost );
    std::ostringstream nameStream;
    nameStream << "COMM("
            << prettyHost
            << ":"
            << imyrank
            << ")"
            << std::ends;

    //TLS: set up thread name
    int status;
    tsd_t *local_data = new tsd_t;
    local_data->thread_id = XPlat::Thread::GetId();
    local_data->thread_name = strdup( nameStream.str().c_str() );
    local_data->process_rank = _local_rank;
    local_data->node_type = CP_NODE;    
    if( ( status = tsd_key.Set( local_data ) ) != 0 )
    {
        //TODO: add event to notify upstream
        error(ERR_SYSTEM, imyrank, "XPlat::TLSKey::Set(): %s\n", strerror( status ) );
        mrn_dbg( 1, mrn_printf(FLF, stderr, "XPlat::TLSKey::Set(): %s\n",
                    strerror( status ) ));
    }

    InternalNode* in = CreateInternalNode( this, myhostname, imyrank,
                                           iphostname, ipport, iprank,
                                           idataSocket, idataPort );
    assert( in != NULL );
    if( in->has_Error() )
        error( ERR_SYSTEM, imyrank, "Failed to initialize using CreateInternalNode()\n" );
}

int Network::parse_Configuration( const char* itopology, bool iusing_mem_buf )
{
    int status;

    if( iusing_mem_buf ) {
        // set up to parse config from a buffer in memory
        mrnBufPtr = itopology;
        mrnBufRemaining = strlen( itopology );
    }
    else {
        // set up to parse config from the file named by our
        // 'filename' member variable
        mrnin = fopen( itopology, "r" );
        if( mrnin == NULL ) {
            error( ERR_SYSTEM, get_LocalRank(), "fopen(%s): %s", itopology,
                   strerror( errno ) );
            return -1;
        }
    }

    status = mrnparse( );

    if( status != 0 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "Failure: mrnparse(\"%s\"): Parse Error\n",
                               itopology ));
        error( ERR_TOPOLOGY_FORMAT, get_LocalRank(), "%s", itopology );
        return -1;
    }

    return 0;
}


void Network::print_error( const char *s )
{
    perror( s );
}

int Network::send_PacketsToParent( std::vector <PacketPtr> &ipackets )
{
    assert( is_LocalNodeChild() );

    vector< PacketPtr >::const_iterator iter;

    for( iter = ipackets.begin(); iter!= ipackets.end(); iter++ ) {
        send_PacketToParent( *iter );
    }

    return 0;
}

int Network::send_PacketToParent( PacketPtr ipacket )
{
    mrn_dbg_func_begin();

    get_ParentNode()->send( ipacket );

    return 0;
}

int Network::flush_PacketsToParent(void)
{
    assert( is_LocalNodeChild() );
    return _parent->flush();
}

int Network::send_PacketsToChildren( std::vector <PacketPtr> &ipackets  )
{
    assert( is_LocalNodeParent() );

    vector< PacketPtr >::const_iterator iter;

    for( iter = ipackets.begin(); iter!= ipackets.end(); iter++ ) {
        send_PacketToChildren( *iter );
    }

    return 0;
}

int Network::send_PacketToChildren( PacketPtr ipacket,
                                    bool iinternal_only /* =false */ )
{
    int retval = 0;
    Stream *stream;

    mrn_dbg_func_begin();

    std::set< PeerNodePtr > peers;

    if( ipacket->get_StreamId( ) == 0 ) {   //stream id 0 => control stream
        peers = get_ChildPeers();
    }
    else {
        std::set< Rank > peer_ranks;
        std::set< Rank >::const_iterator iter;
        stream = get_Stream( ipacket->get_StreamId() );   
        if( stream == NULL ){
            mrn_dbg( 1, mrn_printf(FLF, stderr, "stream %d lookup failed\n",
                                   ipacket->get_StreamId( ) ));
            return -1;
        }
        stream->get_ChildPeers( peer_ranks );
	if(peer_ranks.empty() )
	   mrn_dbg( 5, mrn_printf(FLF, stderr, "child ranks are empty\n") );

        for( iter= peer_ranks.begin(); iter!=peer_ranks.end(); iter++) {
            PeerNodePtr cur_peer = get_OutletNode( *iter );
            if( cur_peer != PeerNode::NullPeerNode ) {
                peers.insert( cur_peer );
            }
	    else
	       mrn_dbg( 5, mrn_printf(FLF, stderr, "outlet node returns null peer\n") );

        }
    }
    
    std::set < PeerNodePtr >::const_iterator iter;
    if(peers.empty() )
       mrn_dbg(5,mrn_printf( FLF, stderr, " peers is empty\n") );


    for( iter=peers.begin(); iter!=peers.end(); iter++ ) {
        PeerNodePtr cur_node = *iter;
        mrn_dbg(3, mrn_printf( FLF, stderr, "node \"%s:%d:%d\": %s, %s\n",
                               cur_node->get_HostName().c_str(),
                               cur_node->get_Rank(),
                               cur_node->get_Port(),
                               ( cur_node->is_parent() ? "parent" : "child" ),
                               ( cur_node->is_internal() ? "internal" : "end-point" ) ));

        //Never send packet back to src
        if( cur_node->get_Rank() == ipacket->get_InletNodeRank() )
            continue;

        //if internal_only, don't send to non-internal nodes
        if( iinternal_only && !cur_node->is_internal( ) )
            continue;

        cur_node->send( ipacket );
    }

    mrn_dbg( 3, mrn_printf(FLF, stderr, "send_PacketToChildren %s",
                           retval == 0 ? "succeeded\n" : "failed\n" ));
    return retval;
}

int Network::flush_PacketsToChildren(void) const
{
    int retval = 0;
    
    mrn_dbg_func_begin();
    
    const std::set< PeerNodePtr > peers = get_ChildPeers();

    std::set < PeerNodePtr >::const_iterator iter;
    for( iter=peers.begin(); iter!=peers.end(); iter++ ) {
        PeerNodePtr cur_node = *iter;

        mrn_dbg( 3, mrn_printf(FLF, stderr, "Calling child[%d].flush() ...\n",
                               cur_node->get_Rank() ));
        if( cur_node->flush() == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "child.flush() failed\n" ));
            retval = -1;
        }
        mrn_dbg( 3, mrn_printf(FLF, stderr, "child.flush() succeeded\n" ));
    }

    mrn_dbg( 3, mrn_printf(FLF, stderr, "flush_PacketsToChildren %s",
                retval == 0 ? "succeeded\n" : "failed\n" ));
    return retval;
}

int Network::recv( bool iblocking )
{
    mrn_dbg_func_begin();

    if( is_ShutDown() )
        return -1;

    if( is_LocalNodeFrontEnd() ) { 
        if( iblocking ){
            return waitfor_NonEmptyStream();
        }
        return 0;
    }
    else if( is_LocalNodeBackEnd() ){
        std::list <PacketPtr> packet_list;
        mrn_dbg(3, mrn_printf(FLF, stderr, "In backend.recv(%s)...\n",
                              (iblocking? "blocking" : "non-blocking") ));

        //if not blocking and no data present, return immediately
        if( ! iblocking && ! this->has_PacketsFromParent() ){
            return 0;
        }

        //check if we already have data
        if( is_LocalNodeThreaded() )
            return waitfor_NonEmptyStream();
        else {
            mrn_dbg(3, mrn_printf(FLF, stderr, "Calling recv_packets()\n"));
            if( recv_PacketsFromParent( packet_list ) == -1){
                mrn_dbg(1, mrn_printf(FLF, stderr, "recv_packets() failed\n"));
                return -1;
            }

            if( packet_list.size() == 0 ) {
                mrn_dbg(3, mrn_printf(FLF, stderr, "No packets read!\n"));
                return 0;
            }
            else {
                mrn_dbg(3, mrn_printf(FLF, stderr,
                                      "Calling proc_packets()\n"));
                if( get_LocalChildNode()->proc_PacketsFromParent(packet_list) == -1){
                    mrn_dbg(1, mrn_printf(FLF, stderr, "proc_packets() failed\n"));
                    return -1;
                }
                mrn_dbg(5, mrn_printf(FLF, stderr, "Found/processed packets! Returning\n"));
                return 1;
            }
        }
    }

    return -1; //shouldn't get here
}

int Network::recv( int *otag, PacketPtr  &opacket, Stream ** ostream, bool iblocking)
{
    mrn_dbg(3, mrn_printf(FLF, stderr, "blocking: \"%s\"\n",
                          (iblocking ? "yes" : "no")) );
    
    bool checked_network = false;   // have we checked sockets for input?
    PacketPtr cur_packet( Packet::NullPacket );

    if( ! have_Streams() && is_LocalNodeFrontEnd() ){
        //No streams exist -- bad for FE
        mrn_dbg(1, mrn_printf(FLF, stderr, "%s recv in FE when no streams "
                              "exist\n", (iblocking? "Blocking" : "Non-blocking") ));
        
        return -1;
    }

    // check streams for input
get_packet_from_stream_label:
    if( have_Streams() ) {

        bool packet_found=false;
        map <unsigned int, Stream *>::iterator start_iter;

        start_iter = _stream_iter;
        do {
            Stream* cur_stream = _stream_iter->second;

            mrn_dbg( 5, mrn_printf(FLF, stderr,
                                   "Checking for packets on stream[%d]...",
                                   cur_stream->get_Id() ));
            cur_packet = cur_stream->get_IncomingPacket();
            if( cur_packet != Packet::NullPacket ){
                packet_found = true;
            }
            mrn_dbg( 5, mrn_printf(0,0,0, stderr, "%s!\n",
                                   (packet_found ? "found" : "not found")));

            _streams_sync.Lock();
            _stream_iter++;
            if( _stream_iter == _streams.end() ){
                //wrap around to start of map entries
                _stream_iter = _streams.begin();
            }
            _streams_sync.Unlock();
        } while( (start_iter != _stream_iter) && ! packet_found );
    }

    if( cur_packet != Packet::NullPacket ) {
        *otag = cur_packet->get_Tag();
        *ostream = get_Stream( cur_packet->get_StreamId() );
        assert(*ostream);
        opacket = cur_packet;
        mrn_dbg(4, mrn_printf(FLF, stderr, "cur_packet tag: %d, fmt: %s\n",
                   cur_packet->get_Tag(), cur_packet->get_FormatString() ));
        return 1;
    }
    else if( iblocking || ! checked_network ) {

        // No packets are already in the stream
        // check whether there is data waiting to be read on our sockets
        int retval = recv( iblocking );
        checked_network = true;

        if( retval == -1 )
            return -1;
        else if ( retval == 1 ){
            // go back if we found a packet
            mrn_dbg( 3, mrn_printf(FLF, stderr, "Network::recv() found a packet!\n" ));
            goto get_packet_from_stream_label;
        }
    }
    mrn_dbg( 3, mrn_printf(FLF, stderr, "Network::recv() No packets found.\n" ));

    return 0;
}

CommunicationNode* Network::get_EndPoint( Rank irank ) const
{
    map< Rank, CommunicationNode * >::const_iterator iter =
        _end_points.find( irank );

    if( iter == _end_points.end() ){
        return NULL;
    }

    return (*iter).second;
}

Communicator* Network::get_BroadcastCommunicator(void) const
{
    return _bcast_communicator;
}

Communicator* Network::new_Communicator()
{
    return new Communicator(this);
}

Communicator* Network::new_Communicator( Communicator& comm )
{
    return new Communicator( this, comm );
}

Communicator* Network::new_Communicator( const set< Rank >& endpoints )
{
    return new Communicator( this, endpoints );
}

Communicator* Network::new_Communicator( set< CommunicationNode* >& endpoints )
{
    return new Communicator( this, endpoints );
}

CommunicationNode* Network::new_EndPoint( string &ihostname, 
                                          Port iport, Rank irank )
{
    return new CommunicationNode(ihostname, iport, irank);
}


//static unsigned int next_stream_id=1;  //id '0' reserved for internal communication

Stream* Network::new_Stream( Communicator *icomm, 
                             int ius_filter_id /*=TFILTER_NULL*/,
                             int isync_filter_id /*=SFILTER_WAITFORALL*/, 
                             int ids_filter_id /*=TFILTER_NULL*/ )
{
    if( NULL == icomm ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "cannot create stream from NULL communicator\n") );
        return NULL;
    }

    //get array of back-ends from communicator
    const set <CommunicationNode*>& endpoints = icomm->get_EndPoints();
    unsigned num_pts = endpoints.size();
    if( num_pts == 0 ) {
        if( icomm != _bcast_communicator ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, 
                       "cannot create stream from communicator containing zero end-points\n") );
            return NULL;
        }
    }

    Rank * backends = new Rank[ endpoints.size() ];
        
    mrn_dbg(5, mrn_printf(FLF, stderr, "backends[ " ));
    set <CommunicationNode*>:: const_iterator iter;
    unsigned  int i;
    for( i=0,iter=endpoints.begin(); iter!=endpoints.end(); i++,iter++) {
        mrn_dbg(5, mrn_printf( 0,0,0, stderr, "%d, ", (*iter)->get_Rank() ));
        backends[i] = (*iter)->get_Rank();
    }
    mrn_dbg(5, mrn_printf(0,0,0, stderr, "]\n"));
    

    PacketPtr packet( new Packet( 0, PROT_NEW_STREAM, "%d %ad %d %d %d",
                                  next_stream_id, backends, endpoints.size(),
                                  ius_filter_id, isync_filter_id, ids_filter_id ) );
    next_stream_id++;

    Stream * stream = get_LocalFrontEndNode()->proc_newStream(packet);
    if( stream == NULL ){
        mrn_dbg( 3, mrn_printf(FLF, stderr, "proc_newStream() failed\n" ));
    }

    delete [] backends;
    return stream;
}

Stream* Network::new_Stream( int iid,
                             Rank* ibackends,
                             unsigned int inum_backends,
                             int ius_filter_id,
                             int isync_filter_id,
                             int ids_filter_id )
{
    mrn_dbg_func_begin();
    Stream * stream = new Stream( this, iid, ibackends, inum_backends,
                                  ius_filter_id, isync_filter_id, ids_filter_id );

    _streams_sync.Lock();

    _streams[iid] = stream;

    if( _streams.size() == 1 ){
        _stream_iter = _streams.begin();
    }

    _streams_sync.Unlock();

    mrn_dbg_func_end();
    return stream;
}

Stream* Network::new_Stream( Communicator* icomm,
                             std::string us_filters,
                             std::string sync_filters,
                             std::string ds_filters )
{
    if( NULL == icomm ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "cannot create stream from NULL communicator\n") );
        return NULL;
    }

    //get array of back-ends from communicator
    const set <CommunicationNode*>& endpoints = icomm->get_EndPoints();
    unsigned num_pts = endpoints.size();
    if( num_pts == 0 ) {
        if( icomm != _bcast_communicator ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, 
                       "cannot create stream from communicator containing zero end-points\n") );
            return NULL;
        }
    }

    Rank * backends = new Rank[ num_pts ];

    mrn_dbg(5, mrn_printf(FLF, stderr, "backends[ " ));
    set <CommunicationNode*>:: const_iterator iter;
    unsigned  int i;
    for( i=0,iter=endpoints.begin(); iter!=endpoints.end(); i++,iter++) {
        mrn_dbg(5, mrn_printf( 0,0,0, stderr, "%d, ", (*iter)->get_Rank() ));
        backends[i] = (*iter)->get_Rank();
    }
    mrn_dbg(5, mrn_printf(0,0,0, stderr, "]\n"));

    PacketPtr packet( new Packet( 0, PROT_NEW_HETERO_STREAM, "%d %ad %s %s %s",
                                  next_stream_id, backends, num_pts,
                                  us_filters.c_str(), sync_filters.c_str(), ds_filters.c_str() ) );
    next_stream_id++;

    Stream * stream = get_LocalFrontEndNode()->proc_newStream(packet);
    if( stream == NULL ){
        mrn_dbg( 3, mrn_printf(FLF, stderr, "proc_newStream() failed\n" ));
    }

    delete [] backends;
    return stream;
}

Stream* Network::get_Stream( unsigned int iid ) const
{
    Stream* ret;
    map< unsigned int, Stream* >::const_iterator iter; 

    _streams_sync.Lock();

    iter = _streams.find( iid );
    if(  iter != _streams.end() ){
        ret = iter->second;
    }
    else{
        ret = NULL;
    }
    _streams_sync.Unlock();

    mrn_dbg(5, mrn_printf(FLF, stderr, "network(%p): stream[%d] = %p\n", this, iid, ret ));
    return ret;
}

void Network::delete_Stream( unsigned int iid )
{
    map< unsigned int, Stream* >::iterator iter; 

    _streams_sync.Lock();

    iter = _streams.find( iid );
    if( iter != _streams.end() ) {
        //if we are about to delete _stream_iter, set it to next elem. (w/wrap)
        if( iter == _stream_iter ) {
            _stream_iter++;
            if( _stream_iter == Network::_streams.end() ){
                _stream_iter = Network::_streams.begin();
            }
        }
        _streams.erase( iter );
    }

    _streams_sync.Unlock();
}

bool Network::have_Streams( )
{
    bool ret;
    _streams_sync.Lock();
    ret = !_streams.empty();
    _streams_sync.Unlock();
    return ret;
}

/* Event Management */
void Network::clear_Events(void)
{
    _evt_mgr->clear_Events();
}

unsigned int Network::num_EventsPending(void)
{
    return _evt_mgr->get_NumEvents();
}

Event* Network::next_Event(void)
{
    return _evt_mgr->get_NextEvent();
}

/* Event Notification by File Descriptor */
int Network::get_EventNotificationFd( EventClass eclass )
{
#if !defined(os_windows)
    EventPipe* ep;
    map< EventClass, EventPipe* >::iterator iter = _evt_pipes.find( eclass );
    if( iter == _evt_pipes.end() ) {
        ep = new EventPipe();
        bool rc = _evt_mgr->register_Callback( eclass, Event::EVENT_TYPE_ALL, 
                                               PipeNotifyCallbackFn, (void*)ep );
        if( ! rc ) {
            mrn_printf(FLF, stderr, "failed to register PipeNotifyCallbackFn");
            delete ep;
            return -1;
        }
        _evt_pipes[eclass] = ep;
    }
    else
        ep = iter->second;

    return ep->get_ReadFd();
#else
    return -1;
#endif
}

void Network::clear_EventNotificationFd( EventClass eclass )
{
#if !defined(os_windows)
    map< EventClass, EventPipe* >::iterator iter = _evt_pipes.find( eclass );
    if( iter != _evt_pipes.end() ) {
        EventPipe* ep = iter->second;
        ep->clear();
    }
#endif
}

void Network::close_EventNotificationFd( EventClass eclass )
{
#if !defined(os_windows)
    map< EventClass, EventPipe* >::iterator iter = _evt_pipes.find( eclass );
    if( iter != _evt_pipes.end() ) {
        EventPipe* ep = iter->second;
        bool rc = _evt_mgr->remove_Callback( PipeNotifyCallbackFn, 
                                             eclass, Event::EVENT_TYPE_ALL );
        if( ! rc ) {
            mrn_printf(FLF, stderr, "failed to remove PipeNotifyCallbackFn");
        }
        else {
            delete ep;
            _evt_pipes.erase( iter );
        }
    }
#endif
}

/* Register & Remove Callbacks */
bool Network::register_EventCallback( EventClass iclass, EventType ityp, 
                                      evt_cb_func ifunc, void* idata )
{
    bool ret = _evt_mgr->register_Callback( iclass, ityp, ifunc, idata );
    if( ret == false ) {
        mrn_printf(FLF, stderr, "failed to register callback");
        return false;
    }
    return true;
}

bool Network::remove_EventCallback( evt_cb_func ifunc, 
                                    EventClass iclass, EventType ityp )
{
    bool ret = _evt_mgr->remove_Callback( ifunc, iclass, ityp );
    if( ret == false ) {
        mrn_printf(FLF, stderr, "failed to remove Callback function for Topology Event\n");
        return false;
    }
    return true;
}

bool Network::remove_EventCallbacks( EventClass iclass, EventType ityp )
{
    bool ret = _evt_mgr->remove_Callbacks( iclass, ityp );
    if( ret == false ) {    
        mrn_printf(FLF, stderr, "failed to remove Callback function for Topology Event\n");
        return false;
    }
    return true;
}

/* Performance Data Management */
bool Network::enable_PerformanceData( perfdata_metric_t metric, 
                                      perfdata_context_t context )
{
    mrn_dbg_func_begin();
    if( metric >= PERFDATA_MAX_MET )
        return false;
    if( context >= PERFDATA_MAX_CTX )
        return false;

    map<unsigned int, Stream*>::iterator iter; 
    _streams_sync.Lock();
    for( iter = _streams.begin(); iter != _streams.end(); iter++ ) {
        bool rc = (*iter).second->enable_PerformanceData( metric, context );
        if( rc == false ) {
           mrn_dbg( 1, mrn_printf(FLF, stderr, 
                    "strm->enable_PerformanceData() failed, cancelling prior enables\n" ));
           while( iter != _streams.begin() ) {
              iter--;
              (*iter).second->disable_PerformanceData( metric, context );
           }
           _streams_sync.Unlock();
           return false;
        }
    }
    _streams_sync.Unlock();

    mrn_dbg_func_end();
    return true;
}

bool Network::disable_PerformanceData( perfdata_metric_t metric, 
                                       perfdata_context_t context )
{
    mrn_dbg_func_begin();
    if( metric >= PERFDATA_MAX_MET )
        return false;
    if( context >= PERFDATA_MAX_CTX )
        return false;

    bool ret = true;
    map< unsigned int, Stream* >::iterator iter; 
    _streams_sync.Lock();
    for( iter = _streams.begin(); iter != _streams.end(); iter++ ) {
        bool rc = (*iter).second->disable_PerformanceData( metric, context );
        if( rc == false ) {
            ret = false;
            mrn_dbg( 1, mrn_printf(FLF, stderr, 
                     "strm->disable_PerformanceData() failed for strm %d\n", iter->first));
        }
    }
    _streams_sync.Unlock();

    mrn_dbg_func_end();
    return ret;
}

bool Network::collect_PerformanceData( map< int, rank_perfdata_map >& results,
                                       perfdata_metric_t metric, 
                                       perfdata_context_t context,
                                       int aggr_filter_id /*=TFILTER_CONCAT*/ )
{
    mrn_dbg_func_begin();
    if( metric >= PERFDATA_MAX_MET )
        return false;
    if( context >= PERFDATA_MAX_CTX )
        return false;

    bool ret = true;

    map< unsigned int, Stream* >::const_iterator citer;
    _streams_sync.Lock();
    for( citer = _streams.begin(); citer != _streams.end(); citer++ ) {

        rank_perfdata_map& strm_results = results[citer->first];
        
        bool rc = (*citer).second->collect_PerformanceData( strm_results, metric, context, 
                                                            aggr_filter_id );
        if( rc == false ) {
            ret = false;
            mrn_dbg( 1, mrn_printf(FLF, stderr, 
                     "strm->collect_PerformanceData() failed for strm %d\n", citer->first));
        }
    }
    _streams_sync.Unlock();

    mrn_dbg_func_end();
    return ret;
}

void Network::print_PerformanceData( perfdata_metric_t metric, 
                                     perfdata_context_t context )
{
    mrn_dbg_func_begin();
    if( metric >= PERFDATA_MAX_MET )
        return;
    if( context >= PERFDATA_MAX_CTX )
        return;

    map< unsigned int, Stream* >::const_iterator citer; 
    _streams_sync.Lock();
    for( citer = _streams.begin(); citer != _streams.end(); citer++ ) {
        (*citer).second->print_PerformanceData( metric, context );
    }
    _streams_sync.Unlock();

    mrn_dbg_func_end();
}


int Network::load_FilterFunc( const char* so_file, const char* func_name )
{
    // start user-defined filter ids at 100 to avoid conflicts with built-ins
    static unsigned short next_filter_id=100; 
    unsigned short cur_filter_id=next_filter_id;
    next_filter_id++;

    mrn_dbg_func_begin();

    int rc = Filter::load_FilterFunc( cur_filter_id, so_file, func_name );
    if( rc != -1 ){
        //Filter registered locally, now propagate to tree
        //TODO: ensure that filter is loaded down the entire tree
        mrn_dbg( 3, mrn_printf(FLF, stderr, 
                               "load_FilterFunc(%hu, %s, %s) successful, sending PROT_NEW_FILTER\n",
                               cur_filter_id, so_file, func_name) );
        PacketPtr packet( new Packet( 0, PROT_NEW_FILTER, "%uhd %s %s",
                                      cur_filter_id, so_file, func_name ) );
        send_PacketToChildren( packet );
        rc = cur_filter_id;
    }
    else {
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                               "load_FilterFunc(%hu, %s, %s) failed\n", 
                               cur_filter_id, so_file, func_name) );
    }
    mrn_dbg_func_end();
    return rc;
}

int Network::waitfor_NonEmptyStream(void)
{
    mrn_dbg_func_begin();

    Stream* cur_strm = NULL;
    map < unsigned int, Stream * >::const_iterator iter;
    _streams_sync.Lock();
    while( true ) { 
        for( iter = _streams.begin(); iter != _streams.end(); iter++ ){
            cur_strm = iter->second;
            mrn_dbg(5, mrn_printf(FLF, stderr, "Checking stream[%d] (%p) for data\n",
                                  cur_strm->get_Id(), cur_strm ));
            if( cur_strm->has_Data() ) {
                mrn_dbg(5, mrn_printf(FLF, stderr, "Data on stream[%d]\n",
                                      cur_strm->get_Id() ));
                _streams_sync.Unlock();
                mrn_dbg_func_end();
                return 1;
            }
            if( cur_strm->is_Closed() ) {
                mrn_dbg(5, mrn_printf(FLF, stderr, "Error on stream[%d]\n",
                                      cur_strm->get_Id() ));
                _streams_sync.Unlock();
                mrn_dbg_func_end();
                return -1;
            }
        }
        mrn_dbg(5, mrn_printf(FLF, stderr, "Waiting on CV[STREAMS_NONEMPTY] ...\n"));
        _streams_sync.WaitOnCondition( STREAMS_NONEMPTY );

        if( is_ShutDown() )
            break;
    }
    _streams_sync.Unlock();
    return -1;
}

void Network::signal_NonEmptyStream(void)
{
    _streams_sync.Lock();
    mrn_dbg(5, mrn_printf(FLF, stderr, "Signaling CV[STREAMS_NONEMPTY] ...\n"));
    _streams_sync.SignalCondition( STREAMS_NONEMPTY );

    DataEvent::DataEventData* ded = new DataEvent::DataEventData( NULL );
    DataEvent* de = new DataEvent( DataEvent::DATA_AVAILABLE, ded );
    _evt_mgr->add_Event( de );

    _streams_sync.Unlock();
}

/* Methods to access internal network state */

void Network::set_BackEndNode( BackEndNode* iback_end_node )
{
    _local_back_end_node = iback_end_node;
}

void Network::set_FrontEndNode( FrontEndNode* ifront_end_node )
{
    _local_front_end_node = ifront_end_node;
}

void Network::set_InternalNode( InternalNode* iinternal_node )
{
    _local_internal_node = iinternal_node;
}

void Network::set_NetworkTopology( NetworkTopology* inetwork_topology )
{
    _network_topology = inetwork_topology;
}

void Network::set_LocalHostName( string const& ihostname )
{
    _local_hostname = ihostname;
}

void Network::set_LocalPort( Port iport )
{
    _local_port = iport;
}

void Network::set_LocalRank( Rank irank )
{
    _local_rank = irank;
}

void Network::set_FailureManager( CommunicationNode* icomm )
{
    if( _failure_manager != NULL ) {
        delete _failure_manager;
    }
    _failure_manager = icomm;
}

NetworkTopology* Network::get_NetworkTopology(void) const
{
    return _network_topology;
}

FrontEndNode* Network::get_LocalFrontEndNode(void) const
{
    return _local_front_end_node;
}

InternalNode* Network::get_LocalInternalNode(void) const
{
    return _local_internal_node;
}

BackEndNode* Network::get_LocalBackEndNode(void) const
{
    return _local_back_end_node;
}

ChildNode* Network::get_LocalChildNode(void) const
{
    if( is_LocalNodeInternal() )
        return dynamic_cast< ChildNode* >(_local_internal_node);
    else
        return dynamic_cast< ChildNode* >(_local_back_end_node) ;
}

ParentNode* Network::get_LocalParentNode(void) const
{
    if( is_LocalNodeInternal() )
        return dynamic_cast< ParentNode* >(_local_internal_node);
    else
        return dynamic_cast< ParentNode* >(_local_front_end_node); 
}

string Network::get_LocalHostName(void) const
{
    return _local_hostname;
}

Port Network::get_LocalPort(void) const
{
    return _local_port;
}

Rank Network::get_LocalRank(void) const
{
    return _local_rank;
}

int Network::get_ListeningSocket(void) const
{
    assert( is_LocalNodeParent() );
    return get_LocalParentNode()->get_ListeningSocket();
}

CommunicationNode* Network::get_FailureManager(void) const
{
    return _failure_manager;
}

bool Network::is_LocalNodeFrontEnd(void) const
{
    return ( _local_front_end_node != NULL );
}

bool Network::is_LocalNodeBackEnd(void) const
{
    return ( _local_back_end_node != NULL );
}

bool Network::is_LocalNodeInternal(void) const
{
    return ( _local_internal_node != NULL );
}

bool Network::is_LocalNodeParent(void) const
{
    return ( is_LocalNodeFrontEnd() || is_LocalNodeInternal() );
}

bool Network::is_LocalNodeChild(void) const
{
    return ( is_LocalNodeBackEnd() || is_LocalNodeInternal() );
}

bool Network::is_LocalNodeThreaded(void) const
{
    return _threaded;
}

int Network::send_FilterStatesToParent(void)
{
    mrn_dbg_func_begin();
    map< unsigned int, Stream* >::iterator iter; 

    _streams_sync.Lock();

    for( iter = _streams.begin(); iter != _streams.end(); iter++ ) {
        (*iter).second->send_FilterStateToParent();
    }

    _streams_sync.Unlock();
    mrn_dbg_func_end();
    return 0;
}

bool Network::reset_Topology( string& itopology )
{
    if( ! _network_topology->reset(itopology) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "Topology->reset() failed\n" ));
        return false;
    }

    return true;
}

bool Network::add_SubGraph( Rank iroot_rank, SerialGraph& sg, bool iupdate ) 
{
    unsigned topsz = _network_topology->get_NumNodes();

    if( ! _network_topology->add_SubGraph(iroot_rank, sg, iupdate) ) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "add_SubGraph() failed\n"));
        return false;
    }

    return update_Streams();
}

bool Network::remove_Node( Rank ifailed_rank, bool iupdate )
{
    mrn_dbg_func_begin();

    mrn_dbg( 5, mrn_printf(FLF, stderr, 
                           "Failed rank is %d\n", ifailed_rank) );
    delete_PeerNode( ifailed_rank );
    _network_topology->remove_Node( ifailed_rank, iupdate );

    _streams_sync.Lock();
    map < unsigned int, Stream* >::iterator iter;
    for( iter=_streams.begin(); iter != _streams.end(); iter++ ) {
        (*iter).second->remove_Node( ifailed_rank );
    }
    _streams_sync.Unlock();

    bool rc = true;
    if( iupdate ) {
        if( ! update_Streams() )
            rc = false;
    }

    mrn_dbg_func_end();
    return rc;
}

bool Network::change_Parent( Rank ichild_rank, Rank inew_parent_rank )
{
    // update topology
    if( ! _network_topology->set_Parent(ichild_rank, inew_parent_rank, true) ) {
        return false;
    }

    // update streams if I'm the new parent
    if( inew_parent_rank == _local_rank )
        update_Streams();

    return true;
}

bool Network::insert_EndPoint( string& ihostname, Port iport, Rank irank )
{
    bool retval = true;
    CommunicationNode* cn = NULL;
    
    mrn_dbg_func_begin();

    _endpoints_mutex.Lock();

    map< Rank, CommunicationNode* >::iterator iter = _end_points.find( irank );
    if( iter != _end_points.end() ) {
        if( (iter->second->get_HostName() != ihostname) ||
            (iter->second->get_Port() != iport) ) {
            mrn_dbg( 5, mrn_printf(FLF, stderr, "Replacing Node[%d] [%s:%d] with [%s:%d]\n",
                                   iter->second->get_HostName().c_str(), iter->second->get_Port(),
                                   ihostname.c_str(), iport) );
            cn = iter->second;
            _end_points.erase( iter );
            if( is_LocalNodeFrontEnd() )
                _bcast_communicator->remove_EndPoint( irank );
            delete cn;
    
            cn = new_EndPoint(ihostname, iport, irank);
            _end_points[ irank ] = cn;
        }
        else
            retval = false;
    }
    else {
        cn = new_EndPoint(ihostname, iport, irank);
        _end_points[ irank ] = cn;
    }

    if( is_LocalNodeFrontEnd() && (cn != NULL) )
        _bcast_communicator->add_EndPoint( cn );
        
    _endpoints_mutex.Unlock();
    
    mrn_dbg_func_end();
    return retval;
}

bool Network::remove_EndPoint( Rank irank )
{
    _endpoints_mutex.Lock();
    _end_points.erase( irank );
    if( is_LocalNodeFrontEnd() )
        _bcast_communicator->remove_EndPoint( irank );
    _endpoints_mutex.Unlock();
    
    return true;
}

bool Network::update_Streams(void)
{
    map< unsigned int, Stream* >::iterator iter;

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Updating stream children ...\n") );

    _streams_sync.Lock();
    for( iter=_streams.begin(); iter != _streams.end(); iter++ ) {
        (*iter).second->recompute_ChildrenNodes();
    }
    _streams_sync.Unlock();

    return true;
}

void Network::close_PeerNodeConnections(void)
{
    mrn_dbg_func_begin();

    set< PeerNodePtr >::const_iterator iter;
 
    if( is_LocalNodeParent() ) {
        _children_mutex.Lock();
        for( iter=_children.begin(); iter!=_children.end(); iter++ ) {
            PeerNodePtr cur_node = *iter;
            mrn_dbg( 5, mrn_printf(FLF, stderr,
                                   "Closing data(%d) and event(%d) sockets to %s:%d\n",
                                   cur_node->_data_sock_fd,
                                   cur_node->_event_sock_fd,
                                   cur_node->get_HostName().c_str(),
                                   cur_node->get_Rank()) );
            if( XPlat::SocketUtils::Close( cur_node->_data_sock_fd ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "error on close(data_fd)\n") );
            }
            if( XPlat::SocketUtils::Close( cur_node->_event_sock_fd ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "error on close(event_fd)\n") );
            }
        }
        _children_mutex.Unlock();
    }

    if( is_LocalNodeChild() ) {
        _parent_sync.Lock();
        if( XPlat::SocketUtils::Close( _parent->_data_sock_fd ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "error on close(data_fd)\n") );
        }
        if( XPlat::SocketUtils::Close( _parent->_event_sock_fd ) == -1 ){
            mrn_dbg( 1, mrn_printf(FLF, stderr, "error on close(event_fd)\n") );
        }
        _parent_sync.Unlock();
    }

    mrn_dbg_func_end();
}

char* Network::get_TopologyStringPtr(void) const
{
    return strdup( _network_topology->get_TopologyString().c_str() );
}

char* Network::get_LocalSubTreeStringPtr(void) const
{
    std::string subtree( _network_topology->get_LocalSubTreeString() );
    if( subtree.empty() )
        return NULL;
    else
        return strdup( subtree.c_str() );
}

void Network::enable_FailureRecovery(void)
{
    _recover_from_failures = true;
}

void Network::disable_FailureRecovery(void)
{
    _recover_from_failures = false;
}

bool Network::recover_FromFailures(void) const
{
    return _recover_from_failures;
}

PeerNodePtr Network::get_ParentNode(void) const
{
    _parent_sync.Lock();

    while( _parent == PeerNode::NullPeerNode ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "Wait for parent node available\n"));
        _parent_sync.WaitOnCondition( PARENT_NODE_AVAILABLE );
    }

    _parent_sync.Unlock();
    return _parent;
}

void Network::set_ParentNode( PeerNodePtr ip )
{
    _parent_sync.Lock();

    _parent = ip;
    if( _parent != PeerNode::NullPeerNode ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr, "Signal parent node available\n"));
        _parent_sync.SignalCondition( PARENT_NODE_AVAILABLE );
    }

    _parent_sync.Unlock();
}

PeerNodePtr Network::new_PeerNode( string const& ihostname, Port iport,
                                   Rank irank, bool iis_parent,
                                   bool iis_internal )
{
    PeerNodePtr node( new PeerNode(this, ihostname, iport, irank,
                                   iis_parent, iis_internal) );

    mrn_dbg( 5, mrn_printf(FLF, stderr, "new peer node: %s:%d (%p) \n",
                           node->get_HostName().c_str(), node->get_Rank(),
                           node.get() )); 

    if( iis_parent ) {
        set_ParentNode( node );
    }
    else {
        _children_mutex.Lock();
	 mrn_dbg( 5, mrn_printf(FLF, stderr, "inserted into children\n") );
        _children.insert( node );
        _children_mutex.Unlock();
    }

    return node;
}

bool Network::delete_PeerNode( Rank irank )
{
    set< PeerNodePtr >::const_iterator iter;

    if( is_LocalNodeChild() ) {
        _parent_sync.Lock();
        if( _parent != PeerNode::NullPeerNode ) { 
            if( _parent->get_Rank() == irank ) {
                _parent = PeerNode::NullPeerNode;
                _parent_sync.Unlock();
                return true;
            }
        }
        _parent_sync.Unlock();
    }

    _children_mutex.Lock();
    for( iter = _children.begin(); iter != _children.end(); iter++ ) {
        if( (*iter)->get_Rank() == irank ) {
	     mrn_dbg( 5, mrn_printf(FLF, stderr, "deleted into children\n") );
            _children.erase( *iter );
            _children_mutex.Unlock();
            return true;
        }
    }
    _children_mutex.Unlock();

    return false;
}

PeerNodePtr Network::get_PeerNode( Rank irank )
{
    PeerNodePtr peer = PeerNode::NullPeerNode;

    set< PeerNodePtr >::const_iterator iter;

    if( is_LocalNodeChild() ) {
        _parent_sync.Lock();
        if( _parent->get_Rank() == irank ) {
            peer = _parent;
        }
        _parent_sync.Unlock();
    }

    if( peer == PeerNode::NullPeerNode ) {
        _children_mutex.Lock();
	if(_children.empty())
	   mrn_dbg( 5, mrn_printf(FLF, stderr, "children is empty\n") );
        for( iter = _children.begin(); iter != _children.end(); iter++ ) {
	    mrn_dbg( 5, mrn_printf(FLF, stderr, "rank is %d, irank is %d\n", 
                                   (*iter)->get_Rank(), irank) ); 
            if( (*iter)->get_Rank() == irank ) {
                peer = *iter;
                break;
            }
        }
        _children_mutex.Unlock();
    }

    return peer;
}

const set< PeerNodePtr > Network::get_ChildPeers(void) const
{
    _children_mutex.Lock();
    const set< PeerNodePtr > children = _children;
    _children_mutex.Unlock();

    return children;
}

PeerNodePtr Network::get_OutletNode( Rank irank ) const
{
    return _network_topology->get_OutletNode( irank );
}

bool Network::has_PacketsFromParent(void)
{
    assert( is_LocalNodeChild() );
    return _parent->has_data();
}

int Network::recv_PacketsFromParent( std::list <PacketPtr> & opackets ) const
{
    assert( is_LocalNodeChild() );
    return _parent->recv( opackets );
}

bool Network::node_Failed( Rank irank  )
{
    return _network_topology->node_Failed( irank );
}

TimeKeeper* Network::get_TimeKeeper(void) 
{ 
    return _local_time_keeper;
}

void set_OutputLevel(int l)
{
    if( l <= MIN_OUTPUT_LEVEL ) {
        CUR_OUTPUT_LEVEL = MIN_OUTPUT_LEVEL;
    }
    else if( l >= MAX_OUTPUT_LEVEL ) {
        CUR_OUTPUT_LEVEL = MAX_OUTPUT_LEVEL;
    }
    else {
        CUR_OUTPUT_LEVEL = l;
    }
}

void set_OutputLevelFromEnvironment(void)
{
    char* output_level = getenv( "MRNET_OUTPUT_LEVEL" );
    if( output_level != NULL ) {
        int l = atoi( output_level );
        set_OutputLevel( l );
    }
}

/* Propagate topology */
SerialGraph* Network::read_Topology( int fd )
{
    mrn_dbg_func_begin(); 

    char* sTopology = NULL;
    uint32_t sTopologyLen = 0;
    
    // obtain topology from our parent
    ::recv( fd, (char*)&sTopologyLen, sizeof(sTopologyLen), 0);
    mrn_dbg(5, mrn_printf(FLF, stderr, "read topo len=%d\n", (int)sTopologyLen ));

    sTopology = new char[sTopologyLen + 1];
    char* currBufPtr = sTopology;
    size_t nRemaining = sTopologyLen;
    while( nRemaining > 0 ) {
        ssize_t nread = ::recv( fd, currBufPtr, nRemaining, 0);
        nRemaining -= nread;
        currBufPtr += nread;
    }
    *currBufPtr = 0;

    mrn_dbg(5, mrn_printf(FLF, stderr, "read topo=%s\n", sTopology));

    SerialGraph* sg = new SerialGraph( sTopology );
    delete[] sTopology;

    return sg;
}

void Network::write_Topology( int fd )
{
    mrn_dbg_func_begin();

    std::string sTopology( _network_topology->get_TopologyString() );
    uint32_t sTopologyLen = (uint32_t) sTopology.length();

    mrn_dbg(5, mrn_printf(FLF, stderr, "sending topology=%s\n",
                          sTopology.c_str() ));

    // send serialized topology size
    ssize_t nwritten = ::send( fd, (char*)&sTopologyLen, sizeof(sTopologyLen), 0);

    // send the topology itself
    // NOTE this code assumes the byte array underneath the std::string
    // remains valid throughout this function
    size_t nRemaining = sTopologyLen;
    const char* currBufPtr = sTopology.c_str();
    while( nRemaining > 0 ) {
        nwritten = ::send( fd, currBufPtr, nRemaining, 0);
        nRemaining -= nwritten;
        currBufPtr += nwritten;
    }
}

/* Failure Recovery */
bool Network::set_FailureRecovery( bool enable_recovery )
{
    int tag;

    mrn_dbg_func_begin();

    if( is_LocalNodeFrontEnd() ) {

	if( enable_recovery ) {
            tag = PROT_ENABLE_RECOVERY;
            enable_FailureRecovery();
	}
	else {
            tag = PROT_DISABLE_RECOVERY;
            disable_FailureRecovery();
	}
	
        PacketPtr packet( new Packet(0, tag, "%d", (int)enable_recovery) );
	if( packet->has_Error() ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "new packet() fail\n") );
            return false;
        }

        if( send_PacketToChildren( packet ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n") );
            return false;
        }
    }
    else {
        mrn_dbg( 1, mrn_printf(FLF, stderr, 
			       "set_FailureRecovery() can only be used by Network front-end\n") ); 
        return false;
    }

    mrn_dbg_func_end();
    return true;
}

}  // namespace MRN
