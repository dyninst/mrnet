/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <fstream>
#include <vector>
#include <algorithm>

#include "config.h"
#include "BackEndNode.h"
#include "ChildNode.h"
#include "Filter.h"
#include "FrontEndNode.h"
#include "InternalNode.h"
#include "ParentNode.h"
#include "ParsedGraph.h"
#include "PeerNode.h"
#include "utils.h"

#include "mrnet/MRNet.h"
#include "xplat/NetUtils.h"
#include "xplat/Thread.h"
using namespace XPlat;

extern FILE *mrnin;
//extern int mrn_flex_debug;

using namespace std;

namespace MRN
{
Network * network=NULL;
int mrnparse( );

extern int mrndebug;
extern const char* mrnBufPtr;
extern unsigned int mrnBufRemaining;

const int MIN_OUTPUT_LEVEL=0;
const int MAX_OUTPUT_LEVEL=5;
int CUR_OUTPUT_LEVEL=1;

const Port UnknownPort = (Port)-1;
const Rank UnknownRank = (Rank)-1;

const char *node_type="";
const char *empty_str="";

void set_OutputLevelFromEnvironment( void );

void init_local( void )
{
#if !defined(os_windows)
    // ignore SIGPIPE
    signal( SIGPIPE, SIG_IGN );
#else
    // init Winsock
    WORD version = MAKEWORD(2, 2); /* socket version 2.2 supported by all modern Windows */
    WSADATA data;
    if( WSAStartup(version, &data) != 0 )
        fprintf(stderr, "WSAStartup failed!\n");
#endif
}

void cleanup_local( void )
{
#if defined(os_windows)
    // cleanup Winsock
    WSACleanup();
#endif
}

/*===========================================================*/
/*             Network DEFINITIONS        */
/*===========================================================*/
Network::Network( const char * itopology, const char * ibackend_exe,
                  const char **ibackend_args, bool irank_backends,
                  bool iusing_mem_buf )
    : _local_port(UnknownPort), _local_rank(UnknownRank),_network_topology(NULL), 
      _failure_manager(NULL), _bcast_communicator(NULL), 
      _local_front_end_node(NULL), _local_back_end_node(NULL), 
      _local_internal_node(NULL), _threaded(true),
      _recover_from_failures(true), _terminate_backends(true)
{
    init_local();
    network=this;
    set_OutputLevelFromEnvironment();
    node_type="fe";
    _streams_sync.RegisterCondition( STREAMS_NONEMPTY );
    _parent_sync.RegisterCondition( PARENT_NODE_AVAILABLE );

    if( parse_Configuration( itopology, iusing_mem_buf ) == -1 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Failure: parse_Configuration( \"%s\" )!\n",
                              ( iusing_mem_buf ? "memory buffer" : itopology )));
        return;
    }

    if( ! parsed_graph->validate( ) ) {
        parsed_graph->perror( "ParsedGraph not valid." );
        return;
    }

    parsed_graph->assign_NodeRanks( irank_backends );

    //TLS: setup thread local storage for frontend
    //I am "FE(hostname:port)"
    string prettyHost;
    string rootHost = parsed_graph->get_Root()->get_HostName();
    Rank rootRank = parsed_graph->get_Root( )->get_Rank();
    setrank(rootRank);
    XPlat::NetUtils::GetHostName( rootHost, prettyHost );
    char port_str[128];
    sprintf( port_str, "%d", rootRank );
    string name( "FE(" );
    name += prettyHost;
    name += ":";
    name += port_str;
    name += ")";

    // check that topology root is same host we're on
    char this_host[256];
    gethostname( this_host, 256 );
    string this_pretty;
    XPlat::NetUtils::GetHostName( string(this_host), this_pretty );
    if( (this_pretty != prettyHost) && (prettyHost != "localhost") ) {
        char warnmsg[1024];
        sprintf( warnmsg, "FE host (%s) != Topology Root (%s)",
                 this_pretty.c_str(), prettyHost.c_str() );
        error( ERR_TOPOLOGY_FORMAT, UnknownRank, warnmsg );
        return;
    }

    int status;
    tsd_t *local_data = new tsd_t;
    local_data->thread_id = XPlat::Thread::GetId( );
    local_data->thread_name = strdup( name.c_str( ) );
    status = tsd_key.Set( local_data );

    if( status != 0 ) {
        error( ERR_SYSTEM, rootRank, "XPlat::TLSKey::Set(): %s\n", strerror( status ) );
        return;
    }

    new FrontEndNode( this, rootHost, rootRank );

    const char* mrn_commnode_path = getenv( "MRN_COMM_PATH" );

    if( mrn_commnode_path == NULL ) {
        mrn_commnode_path = COMMNODE_EXE;
    }
    if( ibackend_exe == NULL ) {
        ibackend_exe = empty_str;
    }
    unsigned int backend_argc=0;
    if( ibackend_args != NULL ){
        for(unsigned int i=0; ibackend_args[i] != NULL; i++){
            backend_argc++;
        }
    }

    // save the serialized graph string in a variable on the stack,
    // so that we don't build a packet with a pointer into a temporary
    string sg = parsed_graph->get_SerializedGraphString();

    PacketPtr packet( new Packet( 0, PROT_NEW_SUBTREE, "%s%s%s%as", sg.c_str( ),
                                  mrn_commnode_path, ibackend_exe, ibackend_args,
                                  backend_argc ) );
    
    mrn_dbg(5, mrn_printf(FLF, stderr, "Instantiating network ... \n" ));
    if( get_LocalFrontEndNode()->proc_newSubTree( packet ) == -1 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Failure: FrontEndNode::proc_newSubTree()!\n" ));
        error( ERR_INTERNAL, rootRank, "");
    }

    mrn_dbg(5, mrn_printf(FLF, stderr, "Waiting for subtrees to report ... \n" ));
    if( get_LocalFrontEndNode()->waitfor_SubTreeReports( ) == -1 ) {
        error( ERR_INTERNAL, rootRank, "");
    }
  
    //We should have a topology available after subtrees report
    _network_topology->print( stderr );

    //broadcast topology, not necessary since sent for subtree creation, right?
    char * topology = _network_topology->get_TopologyStringPtr();
    packet = PacketPtr( new Packet( 0, PROT_TOPOLOGY_RPT, "%s", topology ) );
    mrn_dbg(5, mrn_printf(FLF, stderr, "Broadcasting topology ... \n" ));
    if( send_PacketToChildren( packet, false ) == -1 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Failure: send_DownStream()!\n" ));
        error( ERR_INTERNAL, rootRank, "");
    }
    free( topology );
  
    mrn_dbg(5, mrn_printf(FLF, stderr, "Creating bcast communicator ... \n" ));
    _bcast_communicator = new Communicator( this );
    update_BcastCommunicator( );
}

// back-end constructors
Network::Network( const char *iphostname, Port ipport, Rank iprank,
                  const char *imyhostname, Rank imyrank )
    : _local_port(UnknownPort), _local_rank(UnknownRank), _network_topology(NULL),
      _failure_manager(NULL), _bcast_communicator(NULL),
      _local_front_end_node(NULL), _local_back_end_node(NULL),
      _local_internal_node(NULL), _threaded(true),
      _recover_from_failures( true )
{
    init_local();
    network=this;
    set_OutputLevelFromEnvironment();
    _streams_sync.RegisterCondition( STREAMS_NONEMPTY );
    _parent_sync.RegisterCondition( PARENT_NODE_AVAILABLE );

    init_BackEnd( iphostname, ipport, iprank, imyhostname, imyrank );
}

Network::Network( int argc, char **argv )
    : _local_port(UnknownPort), _local_rank(UnknownRank), _network_topology(NULL),
      _failure_manager(NULL), _bcast_communicator(NULL),
      _local_front_end_node(NULL), _local_back_end_node(NULL),
      _local_internal_node(NULL), _threaded(true),
      _recover_from_failures( true )
{
    init_local();
    network=this;
    set_OutputLevelFromEnvironment();
    _streams_sync.RegisterCondition( STREAMS_NONEMPTY );
    _parent_sync.RegisterCondition( PARENT_NODE_AVAILABLE );

    const char * phostname = argv[argc-5];
    Port pport = (Port) strtoul( argv[argc-4], NULL, 10 );
    Rank prank = (Rank) strtoul( argv[argc-3], NULL, 10 );
    const char * myhostname = argv[argc-2];
    Rank myrank = (Rank) strtoul( argv[argc-1], NULL, 10 );
    init_BackEnd( phostname, pport, prank, myhostname, myrank );
}

//internal node constructor
Network::Network( )
    : _local_port(UnknownPort), _local_rank(UnknownRank), _network_topology(NULL), 
      _failure_manager(NULL), _bcast_communicator(NULL), 
      _local_front_end_node(NULL), _local_back_end_node(NULL),
      _local_internal_node(NULL), _threaded(true),
      _recover_from_failures(true)
{
    init_local();
    network=this;
    set_OutputLevelFromEnvironment();
    node_type="comm";    
}

Network::~Network( )
{
    shutdown_Network( );
    cleanup_local( );
    if( parsed_graph != NULL ) {
        delete parsed_graph;
        parsed_graph = NULL;
    }
    network=NULL;
}

void Network::shutdown_Network( void )
{
    if( is_LocalNodeFrontEnd() && _network_topology->get_NumNodes() ) {

        char delete_backends;
        if( _terminate_backends )
            delete_backends = 't';
        else
            delete_backends = 'f';

        PacketPtr packet( new Packet( 0, PROT_DEL_SUBTREE, "%c", delete_backends ) );
        get_LocalFrontEndNode()->proc_DeleteSubTree( packet );
    }
    string empty("");
    reset_Topology(empty);
    mrn_dbg(5, mrn_printf(FLF, stderr, "Clearing %u leftover events\n",
                          Event::get_NumEvents() ));
    Event::clear_Events();
}

void Network::set_TerminateBackEndsOnShutdown( bool terminate )
{
   _terminate_backends = terminate;
}

void Network::update_BcastCommunicator( void )
{
    set< NetworkTopology::Node * > backends;
    _network_topology->get_BackEndNodes(backends);

    //add end-points to broadcast communicator
    set< NetworkTopology::Node * >::const_iterator iter;
    for( iter=backends.begin(); iter!=backends.end(); iter++ ){
        Rank cur_rank = (*iter)->get_Rank();
        if( _end_points.find( cur_rank ) == _end_points.end() ) {
            string cur_hostname = (*iter)->get_HostName();
            _end_points[ cur_rank ] =
                new CommunicationNode( cur_hostname,
                                       (*iter)->get_Port(),
                                       cur_rank );
            _bcast_communicator->add_EndPoint( cur_rank );
        }
    }
    mrn_dbg(5, mrn_printf(FLF, stderr, "Bcast communicator complete \n" ));
}

void Network::init_BackEnd(const char *iphostname, Port ipport, Rank iprank,
                           const char * imyhostname, Rank imyrank )
{
    node_type="be";
    setrank( imyrank );
    string myhostname;
    XPlat::NetUtils::GetNetworkName( imyhostname, myhostname );

    //TLS: setup thread local storage for frontend
    //I am "BE(host:port)"
    string prettyHost;
    XPlat::NetUtils::GetHostName( myhostname, prettyHost );
    char rank_str[16];
    sprintf( rank_str, "%u", imyrank );
    string name( "BE(" );
    name += prettyHost;
    name += ":";
    name += rank_str;
    name += ")";
    int status;

    tsd_t *local_data = new tsd_t;
    local_data->thread_id = XPlat::Thread::GetId(  );
    local_data->thread_name = strdup( name.c_str(  ) );
    if( ( status = tsd_key.Set( local_data ) ) != 0 ) {
        //TODO: add event to notify upstream
        error( ERR_SYSTEM, imyrank, "XPlat::TLSKey::Set(): %s\n", strerror( status ) );
        mrn_dbg( 1, mrn_printf(FLF, stderr, "XPlat::TLSKey::Set(): %s\n",
                    strerror( status ) ));
    }

    set_BackEndNode( new BackEndNode( this, myhostname, imyrank,
                                                 iphostname, ipport, iprank ) );

    if( get_LocalBackEndNode()->has_Error() ){
        error( ERR_SYSTEM, imyrank, "Failed to initialize via BackEndNode()\n" );
    }
}

int Network::parse_Configuration( const char* itopology, bool iusing_mem_buf )
{
    int status;
    mrndebug=0;
    //mrn_flex_debug=0;

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

    if( get_ParentNode()->send( ipacket ) == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "upstream.send() failed()\n" ));
        return -1;
    }

    mrn_dbg_func_end();
    return 0;
}

int Network::flush_PacketsToParent( void )
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

    std::set < PeerNodePtr > peers;

    if( ipacket->get_StreamId( ) == 0 ) {   //stream id 0 => control stream
        peers = get_ChildPeers();
    }
    else {
        std::set < Rank > peer_ranks;
        std::set < Rank >::const_iterator iter;
        stream = get_Stream( ipacket->get_StreamId() );   
        if( stream == NULL ){
            mrn_dbg( 1, mrn_printf(FLF, stderr, "stream %d lookup failed\n",
                                   ipacket->get_StreamId( ) ));
            return -1;
        }
        peer_ranks = stream->get_ChildPeers();
        for( iter= peer_ranks.begin(); iter!=peer_ranks.end(); iter++) {
            PeerNodePtr cur_peer = get_OutletNode( *iter );
            if( cur_peer != PeerNode::NullPeerNode ) {
                peers.insert( cur_peer );
            }
        }
    }
    
    std::set < PeerNodePtr >::const_iterator iter;
    for( iter=peers.begin(); iter!=peers.end(); iter++ ) {
        PeerNodePtr cur_node = *iter;
        mrn_dbg(3, mrn_printf( FLF, stderr, "node \"%s:%d:%d\": %s, %s\n",
                               cur_node->get_HostName().c_str(),
                               cur_node->get_Rank(),
                               cur_node->get_Port(),
                               ( cur_node->is_parent() ? "parent" : "child" ),
                               ( cur_node->is_internal() ? "internal" : "end-point" ) ));

        //Never send packet back to src
        if ( cur_node->get_Rank() == ipacket->get_InletNodeRank() ){
            continue;
        }

        //if internal_only, don't send to non-internal nodes
        if( iinternal_only && !cur_node->is_internal( ) ){
            continue;
        }
        mrn_dbg( 3, mrn_printf( FLF, stderr, "Calling peer[%d].send() ...\n",
                                cur_node->get_Rank() ));
        if( cur_node->send( ipacket ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "peer.send() failed\n" ));
            retval = -1;
        }
        mrn_dbg( 3, mrn_printf(FLF, stderr, "peer.send() succeeded\n" ));
    }

    mrn_dbg( 3, mrn_printf(FLF, stderr, "send_PacketToChildren %s",
                           retval == 0 ? "succeeded\n" : "failed\n" ));
    return retval;
}

int Network::flush_PacketsToChildren( void ) const
{
    int retval = 0;
    
    mrn_dbg_func_begin();
    
    const std::set < PeerNodePtr > peers = get_ChildPeers();

    std::set < PeerNodePtr >::const_iterator iter;
    for( iter=peers.begin(); iter!=peers.end(); iter++ ) {
        PeerNodePtr cur_node = *iter;

        mrn_dbg( 3, mrn_printf(FLF, stderr, "Calling child[%d].flush() ...\n",
                               cur_node->get_Rank() ));
        if( cur_node->flush( ) == -1 ) {
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

    if( is_LocalNodeFrontEnd() ) { 
        if( iblocking ){
            waitfor_NonEmptyStream();
            return 1;
        }
        return 0;
    }
    else if( is_LocalNodeBackEnd() ){
        std::list <PacketPtr> packet_list;
        mrn_dbg(3, mrn_printf(FLF, stderr, "In backend.recv(%s)...\n",
                              (iblocking? "blocking" : "non-blocking") ));

        //if not blocking and no data present, return immediately
        if( !iblocking && !this->has_PacketsFromParent() ){
            return 0;
        }

        //check if we already have data
        if( is_LocalNodeThreaded() ) {
            waitfor_NonEmptyStream();
        }
        else {
            mrn_dbg(3, mrn_printf(FLF, stderr, "Calling recv_packets()\n"));
            if( recv_PacketsFromParent( packet_list ) == -1){
                mrn_dbg(1, mrn_printf(FLF, stderr, "recv_packets() failed\n"));
                return -1;
            }

            if(packet_list.size() == 0){
                mrn_dbg(3, mrn_printf(FLF, stderr, "No packets read!\n"));
            }
            else {
                mrn_dbg(3, mrn_printf(FLF, stderr,
                                      "Calling proc_packets()\n"));
                if( get_LocalChildNode()->proc_PacketsFromParent(packet_list) == -1){
                    mrn_dbg(1, mrn_printf(FLF, stderr, "proc_packets() failed\n"));
                    return -1;
                }
            }
        }
        
        //if we get here, we have found data to return
        mrn_dbg(5, mrn_printf(FLF, stderr, "Found/processed packets! Returning\n"));
        
        return 1;
    }

    return -1; //shouldn't get here
}

int Network::recv( int *otag, PacketPtr  &opacket, Stream ** ostream, bool iblocking)
{
    mrn_dbg(2, mrn_printf(FLF, stderr, "blocking: \"%s\"\n",
               ( iblocking ? "yes" : "no") ));

    bool checked_network = false;   // have we checked sockets for input?
    PacketPtr cur_packet( Packet::NullPacket );

    if( !have_Streams() && is_LocalNodeFrontEnd() ){
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
        do{
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
        } while( (start_iter != _stream_iter) && !packet_found );
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
    else if( iblocking || !checked_network ) {

        // No packets are already in the stream
        // check whether there is data waiting to be read on our sockets
        int retval = recv( iblocking );
        checked_network = true;

        if( retval == -1 ){
            mrn_dbg( 1, mrn_printf(FLF, stderr, "Network::recv() failed.\n" ));
            return -1;
        }
        else if ( retval == 1 ){
            // go back if we found a packet
            mrn_dbg( 3, mrn_printf(FLF, stderr, "Network::recv() found a packet!\n" ));
            goto get_packet_from_stream_label;
        }
    }
    mrn_dbg( 3, mrn_printf(FLF, stderr, "Network::recv() No packets found.\n" ));

    return 0;
}

CommunicationNode * Network::get_EndPoint( Rank irank ) const
{
    map< Rank, CommunicationNode * >::const_iterator iter =
        _end_points.find( irank );

    if( iter == _end_points.end() ){
        return NULL;
    }

    return (*iter).second;
}

Communicator * Network::get_BroadcastCommunicator(void) const
{
    return _bcast_communicator;
}

Communicator * Network::new_Communicator()
{
    return new Communicator(this);
}

Communicator * Network::new_Communicator( Communicator& comm )
{
    return new Communicator( this, comm );
}

Communicator * Network::new_Communicator( set <CommunicationNode *> & endpoints )
{
    return new Communicator( this, endpoints );
}

CommunicationNode * Network::new_EndPoint(string &ihostname,
                                          Port iport, Rank irank )
{
    return new CommunicationNode(ihostname, iport, irank);
}

Stream * Network::new_Stream( Communicator *icomm, 
                              int ius_filter_id /*=TFILTER_NULL*/,
                              int isync_filter_id /*=SFILTER_WAITFORALL*/, 
                              int ids_filter_id /*=TFILTER_NULL*/ )
{
    static unsigned int next_stream_id=1;  //id '0' reserved for internal communication

    //get array of back-ends from communicator
    const set <CommunicationNode*> endpoints = icomm->get_EndPoints();
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

Stream * Network::new_Stream( int iid,
                              Rank *ibackends,
                              unsigned int inum_backends,
                              int ius_filter_id,
                              int isync_filter_id,
                              int ids_filter_id
                            )
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

Stream * Network::new_Stream( Communicator * icomm,
                              std::string us_filters,
                              std::string sync_filters,
                              std::string ds_filters )
{
    static unsigned int next_stream_id=1;  //id '0' reserved for internal communication

    //get array of back-ends from communicator
    const set <CommunicationNode*> endpoints = icomm->get_EndPoints();
    Rank * backends = new Rank[ endpoints.size() ];

    mrn_dbg(5, mrn_printf(FLF, stderr, "backends[ " ));
    set <CommunicationNode*>:: const_iterator iter;
    unsigned  int i;
    for( i=0,iter=endpoints.begin(); iter!=endpoints.end(); i++,iter++) {
        mrn_dbg(5, mrn_printf( 0,0,0, stderr, "%d, ", (*iter)->get_Rank() ));
        backends[i] = (*iter)->get_Rank();
    }
    mrn_dbg(5, mrn_printf(0,0,0, stderr, "]\n"));

    PacketPtr packet( new Packet( 0, PROT_NEW_HETERO_STREAM, "%d %ad %s %s %s",
                                  next_stream_id, backends, endpoints.size(),
                                  us_filters.c_str(), sync_filters.c_str(), ds_filters.c_str() ) );
    next_stream_id++;

    Stream * stream = get_LocalFrontEndNode()->proc_newStream(packet);
    if( stream == NULL ){
        mrn_dbg( 3, mrn_printf(FLF, stderr, "proc_newStream() failed\n" ));
    }

    delete [] backends;
    return stream;
}

Stream * Network::get_Stream( unsigned int iid ) const
{
    Stream * ret;
    map<unsigned int, Stream*>::const_iterator iter; 

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
    map<unsigned int, Stream*>::iterator iter; 

    _streams_sync.Lock();

    iter = _streams.find( iid );
    if(  iter != _streams.end() ){

        //if we are about to delete start_iter, set it to next elem. (w/wrap)
        if( iter == _stream_iter ){
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

// int Network::get_DataSocketFds( int **ofds, unsigned int *onum_fds ) const
// {
//     if( is_LocalNodeFrontEnd() ) {
//         _children_mutex.Lock();
//         std::set < PeerNodePtr >::const_iterator iter;

//         *onum_fds = _children.size( );
//         *ofds = new int[*onum_fds];

//         unsigned int i;
//         for( i=0,iter=_children.begin(); iter != _children.end(); iter++,i++ ) {
//             (*ofds)[i] = (*iter)->get_DataSocketFd();
//         }
//         _children_mutex.Unlock();
//     }
//     else if ( is_LocalNodeBackEnd() ) {
//         _parent_sync.Lock();
//         *onum_fds = 1;
//         *ofds = new int;

//         *ofds[0] = _parent->get_DataSocketFd();

//         _parent_sync.Unlock();
//     }

//     return 0;
// }

int Network::get_EventNotificationFd( EventType etyp )
{
#if !defined(os_windows)
    EventPipe* ep;
    map< EventType, EventPipe* >::iterator iter = _evt_pipes.find( etyp );
    if( iter == _evt_pipes.end() ) {
        ep = new EventPipe();
        bool rc = Event::register_EventCallback( etyp, PipeNotifyCallbackFn, (void*)ep );
        if( ! rc ) {
            mrn_printf(FLF, stderr, "failed to register PipeNotifyCallbackFn");
            delete ep;
            return -1;
        }
        _evt_pipes[etyp] = ep;
    }
    else
        ep = iter->second;

    return ep->get_ReadFd();
#else
    return -1;
#endif
}

void Network::clear_EventNotificationFd( EventType etyp )
{
#if !defined(os_windows)
    map< EventType, EventPipe* >::iterator iter = _evt_pipes.find( etyp );
    if( iter != _evt_pipes.end() ) {
        EventPipe* ep = iter->second;
        ep->clear();
    }
#endif
}

void Network::close_EventNotificationFd( EventType etyp )
{
#if !defined(os_windows)
    map< EventType, EventPipe* >::iterator iter = _evt_pipes.find( etyp );
    if( iter != _evt_pipes.end() ) {
        EventPipe* ep = iter->second;
        bool rc = Event::remove_EventCallback( etyp );
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
    map<unsigned int, Stream*>::iterator iter; 
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
                                       int aggr_filter_id /*=TFILTER_CONCAT*/
                                       )
{
    mrn_dbg_func_begin();
    if( metric >= PERFDATA_MAX_MET )
        return false;
    if( context >= PERFDATA_MAX_CTX )
        return false;

    bool ret = true;

    map<unsigned int, Stream*>::const_iterator citer;
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

    map<unsigned int, Stream*>::const_iterator citer; 
    _streams_sync.Lock();
    for( citer = _streams.begin(); citer != _streams.end(); citer++ ) {
        (*citer).second->print_PerformanceData( metric, context );
    }
    _streams_sync.Unlock();

    mrn_dbg_func_end();
}


int Network::load_FilterFunc( const char *so_file, const char *func )
{
    mrn_dbg_func_begin();

    int fid = Filter::load_FilterFunc( so_file, func );
    
    if( fid != -1 ){
        //Filter registered locally, now propagate to tree
        //TODO: ensure that filter is loaded down the entire tree
        mrn_dbg(3, mrn_printf(FLF, stderr, "sending PROT_NEW_FILTER\n" ));
        PacketPtr packet( new Packet( 0, PROT_NEW_FILTER, "%uhd %s %s",
                                      fid, so_file, func ) );
        send_PacketToChildren( packet );
    }
    mrn_dbg_func_end();
    return fid;
}


void Network::set_BlockingTimeOut( int timeout )
{
    mrn_dbg(2, mrn_printf(FLF, stderr, "set_BlockingTimeOut(%d)\n", timeout ));

    PeerNode::set_BlockingTimeOut( timeout );
}

int Network::get_BlockingTimeOut(  )
{
    return PeerNode::get_BlockingTimeOut(  );
}

void Network::waitfor_NonEmptyStream( void )
{
    mrn_dbg_func_begin();

    map < unsigned int, Stream * >::const_iterator iter;
    _streams_sync.Lock();
    while( true ){ 
        for( iter=_streams.begin(); iter != _streams.end(); iter++ ){
            mrn_dbg(5, mrn_printf(FLF, stderr, "Checking stream[%d] (%p) for data\n",
                                  (*iter).second->get_Id(), (*iter).second ));
            if( (*iter).second->has_Data() ){
                mrn_dbg(5, mrn_printf(FLF, stderr, "Data found on stream[%d]\n",
                                      (*iter).second->get_Id() ));
                _streams_sync.Unlock();
                mrn_dbg_func_end();
                return;
            }
        }
        mrn_dbg(5, mrn_printf(FLF, stderr, "Waiting on CV[STREAMS_NONEMPTY] ...\n"));
        _streams_sync.WaitOnCondition( STREAMS_NONEMPTY );
    }
}

void Network::signal_NonEmptyStream( void )
{
    _streams_sync.Lock();
    mrn_dbg(5, mrn_printf(FLF, stderr, "Signaling CV[STREAMS_NONEMPTY] ...\n"));
    _streams_sync.SignalCondition( STREAMS_NONEMPTY );
    Event::new_Event( DATA_EVENT, STREAMS_NONEMPTY, _local_rank );
    _streams_sync.Unlock();
}

/* Methods to access internal network state */

void Network::set_BackEndNode( BackEndNode * iback_end_node )
{
    _local_back_end_node = iback_end_node;
}

void Network::set_FrontEndNode( FrontEndNode * ifront_end_node )
{
    _local_front_end_node = ifront_end_node;
}

void Network::set_InternalNode( InternalNode * iinternal_node )
{
    _local_internal_node = iinternal_node;
}

void Network::set_NetworkTopology( NetworkTopology * inetwork_topology )
{
    _network_topology = inetwork_topology;
}

void Network::set_LocalHostName( string & ihostname )
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

void Network::set_FailureManager( CommunicationNode * icomm )
{
    if( _failure_manager != NULL ) {
        delete _failure_manager;
    }
    _failure_manager = icomm;
}

NetworkTopology * Network::get_NetworkTopology( void ) const
{
    return _network_topology;
}

FrontEndNode * Network::get_LocalFrontEndNode( void ) const
{
    return _local_front_end_node;
}

InternalNode * Network::get_LocalInternalNode( void ) const
{
    return _local_internal_node;
}

BackEndNode * Network::get_LocalBackEndNode( void ) const
{
    return _local_back_end_node;
}

ChildNode * Network::get_LocalChildNode( void ) const
{
    if( is_LocalNodeInternal() )
        return dynamic_cast<ChildNode*>(_local_internal_node);
    else
        return dynamic_cast<ChildNode*>(_local_back_end_node) ;
}

ParentNode * Network::get_LocalParentNode( void ) const
{
    if( is_LocalNodeInternal() )
        return dynamic_cast<ParentNode*>(_local_internal_node);
    else
        return dynamic_cast<ParentNode*>(_local_front_end_node); 
}

string Network::get_LocalHostName( void ) const
{
    return _local_hostname;
}

Port Network::get_LocalPort( void ) const
{
    return _local_port;
}

Rank Network::get_LocalRank( void ) const
{
    return _local_rank;
}

int Network::get_ListeningSocket( void ) const
{
    assert( is_LocalNodeParent() );
    return get_LocalParentNode()->get_ListeningSocket();
}

CommunicationNode * Network::get_FailureManager( void ) const
{
    return _failure_manager;
}

bool Network::is_LocalNodeFrontEnd( void ) const
{
    return ( _local_front_end_node != NULL );
}

bool Network::is_LocalNodeBackEnd( void ) const
{
    return ( _local_back_end_node != NULL );
}

bool Network::is_LocalNodeInternal( void ) const
{
    return ( _local_internal_node != NULL );
}

bool Network::is_LocalNodeParent( void ) const
{
    return ( is_LocalNodeFrontEnd() || is_LocalNodeInternal() );
}

bool Network::is_LocalNodeChild( void ) const
{
    return ( is_LocalNodeBackEnd() || is_LocalNodeInternal() );
}

bool Network::is_LocalNodeThreaded( void ) const
{
    return _threaded;
}

int Network::send_FilterStatesToParent( void )
{
    mrn_dbg_func_begin();
    map<unsigned int, Stream*>::iterator iter; 

    _streams_sync.Lock();

    for( iter = _streams.begin(); iter != _streams.end(); iter++ ) {
        (*iter).second->send_FilterStateToParent( );
    }

    _streams_sync.Unlock();
    mrn_dbg_func_end();
    return 0;
}

bool Network::reset_Topology( string & itopology )
{
    if( !_network_topology->reset( itopology ) ){
        mrn_dbg( 1, mrn_printf(FLF, stderr, "Topology->reset() failed\n" ));
        return false;
    }

    return true;
}

bool Network::add_SubGraph( Rank iroot_rank, SerialGraph &sg )
{
    unsigned topsz = _network_topology->get_NumNodes();

    if( !_network_topology->add_SubGraph( iroot_rank, sg, true ) ){
        mrn_dbg(5, mrn_printf(FLF, stderr, "add_SubGraph() failed\n"));
        return false;
    }

    if( is_LocalNodeFrontEnd() && 
        ( _bcast_communicator != NULL ) &&
        ( _network_topology->get_NumNodes() > topsz ) ) {
        // new node joined network (likely a new BE)   
        // update list of network endpoints and bcast communicator
        update_BcastCommunicator( );

        // TODO: safely inform rest of network about topology change
    }

    update();

    return true;

}

bool Network::remove_Node( Rank ifailed_rank, bool iupdate )
{
    mrn_dbg_func_begin();

    mrn_dbg(3, mrn_printf( FLF, stderr, "Deleing PeerNode: node[%u] ...\n", ifailed_rank ));
    delete_PeerNode( ifailed_rank );

    mrn_dbg(3, mrn_printf( FLF, stderr, "Removing from Topology: node[%u] ...\n", ifailed_rank ));
    _network_topology->remove_Node( ifailed_rank, iupdate );

    mrn_dbg(3, mrn_printf( FLF, stderr, "Removing from Streams: node[%u] ...\n", ifailed_rank ));
    _streams_sync.Lock();
    map < unsigned int, Stream * >::iterator iter;
    for( iter=_streams.begin(); iter != _streams.end(); iter++ ) {
        (*iter).second->remove_Node( ifailed_rank );
    }
    _streams_sync.Unlock();

    bool retval=true;
    if( iupdate ) {
        if( !update() ) {
            retval = false;
        }
    }

    mrn_dbg_func_end();
    return retval;
}

bool Network::change_Parent( Rank ichild_rank, Rank inew_parent_rank )
{
    //update topology
    if( !_network_topology->set_Parent( ichild_rank, inew_parent_rank, true ) ) {
        return false;
    }

    if( !update() ) {
        return false;
    }

    return true;
}

bool Network::update( void )
{
    //update stream
    mrn_dbg(3, mrn_printf( FLF, stderr, "Updating stream children ...\n"));
    _streams_sync.Lock();
    map < unsigned int, Stream * >::iterator iter;
    for( iter=_streams.begin(); iter != _streams.end(); iter++ ) {
        (*iter).second->recompute_ChildrenNodes();
    }
    _streams_sync.Unlock();

    return true;
}

#include <signal.h>
void Network::cancel_IOThreads( void )
{
    mrn_dbg_func_begin();

    set< PeerNodePtr >::const_iterator iter;

    if( is_LocalNodeParent() ) {
        _children_mutex.Lock();
        for( iter=_children.begin(); iter!=_children.end(); iter++ ) {
            PeerNodePtr cur_node = *iter;
            
            //flush outgoing packets for children threads
            if( cur_node->flush() != 0 ) {
                mrn_dbg(1, mrn_printf(FLF, stderr, "Peer[%u]: flush() failed\n",
                                      cur_node->get_Rank() ));
            }

            mrn_dbg(5, mrn_printf(FLF, stderr,
                                  "Cancelling I/O threads to %s:%d\n",
                                  cur_node->get_HostName().c_str(),
                                  cur_node->get_Rank() ));
            if( XPlat::Thread::Cancel( cur_node->send_thread_id ) != 0 ) {
                mrn_dbg(1, mrn_printf(FLF, stderr, "Thread::Cancel(%d) failed\n",
                                      cur_node->send_thread_id ));
                ::perror( "Thread::Cancel()\n");
            }
            if( XPlat::Thread::Cancel( cur_node->recv_thread_id ) != 0 ) {
                mrn_dbg(1, mrn_printf(FLF, stderr, "Thread::Cancel(%d) failed\n",
                                      cur_node->recv_thread_id ));
                ::perror( "Thread::Cancel()\n");
            }
        }
        _children_mutex.Unlock();
    }

    if( is_LocalNodeChild() ) {
        mrn_dbg(5, mrn_printf(FLF, stderr,
                              "Cancelling I/O threads to %s:%d\n",
                              _parent->get_HostName().c_str(),
                              _parent->get_Rank() ));
        if( XPlat::Thread::Cancel( _parent->send_thread_id ) != 0 ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "Thread::Cancel(%d) failed\n",
                                  _parent->send_thread_id ));
            ::perror( "Thread::Cancel()\n");
        }
        if( XPlat::Thread::Cancel( _parent->recv_thread_id ) != 0 ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "Thread::Cancel(%d) failed\n",
                                  _parent->recv_thread_id ));
            ::perror( "Thread::Cancel()\n");
        }
    }

    mrn_dbg_func_end();
}

void Network::close_PeerNodeConnections( void )
{
    mrn_dbg_func_begin();

    set< PeerNodePtr >::const_iterator iter;
 
    if( is_LocalNodeParent() ) {
        _children_mutex.Lock();
        for( iter=_children.begin(); iter!=_children.end(); iter++ ) {
            PeerNodePtr cur_node = *iter;
            mrn_dbg(5, mrn_printf(FLF, stderr,
                                  "Closing data(%d) and event(%d) sockets to %s:%d\n",
                                  cur_node->_data_sock_fd,
                                  cur_node->_event_sock_fd,
                                  cur_node->get_HostName().c_str(),
                                  cur_node->get_Rank() ));
            if( close( cur_node->_data_sock_fd ) == -1 ) {
                perror("close(data_fd)");
            }
            if( close( cur_node->_event_sock_fd ) == -1 ){
                perror("close(event_fd)");
            }
        }
        _children_mutex.Unlock();
    }

    if( is_LocalNodeChild() ) {
        _parent_sync.Lock();
        if( close( _parent->_data_sock_fd ) == -1 ) {
            perror("close(data_fd)");
        }
        if( close( _parent->_event_sock_fd ) == -1 ){
            perror("close(event_fd)");
        }
        _parent_sync.Unlock();
    }

    mrn_dbg_func_end();
}

char * Network::get_TopologyStringPtr( void ) const
{
    return _network_topology->get_TopologyStringPtr();
}

char * Network::get_LocalSubTreeStringPtr( void ) const
{
    return _network_topology->get_LocalSubTreeStringPtr();
}

void Network::enable_FailureRecovery( void )
{
    _recover_from_failures = true;
}

void Network::disable_FailureRecovery( void )
{
    _recover_from_failures = false;
}

bool Network::recover_FromFailures( void ) const
{
    return _recover_from_failures;
}

PeerNodePtr Network::get_ParentNode( void ) const
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
    PeerNodePtr node( new PeerNode( this, ihostname, iport, irank,
                                    iis_parent, iis_internal ) );

    mrn_dbg( 5, mrn_printf(FLF, stderr, "new peer node: %s:%d (%p) \n",
                           node->get_HostName().c_str(), node->get_Rank(),
                           node.get() )); 

    if( iis_parent ) {
        _parent_sync.Lock();
        _parent = node;
        _parent_sync.Unlock();
    }
    else {
        _children_mutex.Lock();
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
        if( _parent->get_Rank() == irank ) {
            _parent == PeerNode::NullPeerNode;
            _parent_sync.Unlock();
            return true;
        }
        _parent_sync.Unlock();
    }

    _children_mutex.Lock();
    for( iter=_children.begin(); iter!=_children.end(); iter++ ) {
        if( (*iter)->get_Rank() == irank ) {
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
    PeerNodePtr peer=PeerNode::NullPeerNode;

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
        for( iter=_children.begin(); iter!=_children.end(); iter++ ) {
            if( (*iter)->get_Rank() == irank ) {
                peer = *iter;
                break;
            }
        }
        _children_mutex.Unlock();
    }

    return peer;
}

const set < PeerNodePtr > Network::get_ChildPeers( void ) const
{
    _children_mutex.Lock();
    const set< PeerNodePtr> children = _children;
    _children_mutex.Unlock();

    return children;
}

PeerNodePtr Network::get_OutletNode( Rank irank ) const
{
    return _network_topology->get_OutletNode( irank );
}

bool Network::has_PacketsFromParent( void )
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

void set_OutputLevel(int l){
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

void set_OutputLevelFromEnvironment( void )
{
    char * output_level = getenv( "MRNET_OUTPUT_LEVEL" );

    if( output_level ) {
        int l = atoi( output_level );
        set_OutputLevel( l );
    }
}

}  // namespace MRN
