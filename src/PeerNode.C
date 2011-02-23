/****************************************************************************
 *  Copyright 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <sstream>

#include "config.h"
#include "utils.h"
#include "ChildNode.h"
#include "ParentNode.h"
#include "PeerNode.h"

#include "mrnet/MRNet.h"
#include "xplat/SocketUtils.h"
#include "xplat/Error.h"
#include "xplat/NetUtils.h"

using namespace std;

namespace MRN
{

typedef struct send_recv_thr_args {
    Network* net;
    Rank r;
} send_recv_args_t;

/*====================================================*/
/*  PeerNode CLASS METHOD DEFINITIONS            */
/*====================================================*/

PeerNodePtr PeerNode::NullPeerNode;

PeerNode::PeerNode( Network * inetwork, std::string const& ihostname, Port iport,
                    Rank irank, bool iis_parent, bool iis_internal )
    : CommunicationNode(ihostname, iport, irank ), _network(inetwork),
      _data_sock_fd(0), _event_sock_fd(0),
      _is_internal_node(iis_internal), _is_parent(iis_parent), 
      _recv_thread_started(false), _send_thread_started(false), 
      _available(true)
{
    _sync.RegisterCondition( MRN_FLUSH_COMPLETE );
    _sync.RegisterCondition( MRN_RECV_THREAD_STARTED );
    _sync.RegisterCondition( MRN_SEND_THREAD_STARTED );
}

int PeerNode::connect_DataSocket(void)
{
    mrn_dbg( 3, mrn_printf(FLF, stderr, "Creating data connection to (%s:%d) ...\n",
                          _hostname.c_str(), _port) );

    if( connectHost(&_data_sock_fd, _hostname.c_str(), _port) == -1) {
        error( ERR_SYSTEM, _rank, "connectHost() failed" );
        mrn_dbg( 1, mrn_printf(FLF, stderr, "connectHost() failed\n") );
        return -1;
    }
    
    mrn_dbg( 3, mrn_printf(FLF, stderr,
                          "new data socket %d\n", _data_sock_fd) );
    return 0;
}

int PeerNode::connect_EventSocket(void)
{
    mrn_dbg( 3, mrn_printf(FLF, stderr, "Creating event connection to (%s:%d) ...\n",
                          _hostname.c_str(), _port) );

    if( connectHost(&_event_sock_fd, _hostname.c_str(), _port) == -1 ) {
        error( ERR_SYSTEM, _rank, "connectHost() failed" );
        mrn_dbg( 1, mrn_printf(FLF, stderr, "connectHost() failed\n") );
        return -1;
    }
    
    mrn_dbg( 3, mrn_printf(FLF, stderr,
                          "new event socket %d\n", _event_sock_fd) );
    return 0;
}

int PeerNode::start_CommunicationThreads(void)
{
    int retval = 0;
    send_recv_args_t* args = (send_recv_args_t*) malloc( sizeof(send_recv_args_t) );
    if( args == NULL ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "malloc() failed...\n") );
        return -1;
    }
    args->net = _network;
    args->r = _rank;

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Creating send_thread ...\n") );
    retval = XPlat::Thread::Create( send_thread_main, (void*)args, &send_thread_id );
    mrn_dbg( 3, mrn_printf(FLF, stderr, "id: 0x%x\n", send_thread_id) );
    if( retval != 0 ) {
        error( ERR_SYSTEM, _rank, "XPlat::Thread::Create() failed: %s\n",
               strerror(errno) );
        mrn_dbg( 1, mrn_printf(FLF, stderr, "send_thread creation failed...\n") );
        return retval;
    }

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Creating recv_thread ...\n") );
    retval = XPlat::Thread::Create( recv_thread_main, (void*)args, &recv_thread_id  );
    mrn_dbg( 3, mrn_printf(FLF, stderr, "id: 0x%x\n", recv_thread_id) );
    if( retval != 0 ) {
        error( ERR_SYSTEM, _rank, "XPlat::Thread::Create() failed: %s\n",
               strerror(errno) );
        mrn_dbg( 1, mrn_printf(FLF, stderr, "recv_thread creation failed...\n") );
        return retval;
    }

    waitfor_CommunicationThreads();

    return retval;
}

void PeerNode::waitfor_CommunicationThreads(void) const
{
    _sync.Lock();
    while( ! _recv_thread_started ) {
        // recv thread waits for send thread, so just wait for recv
        _sync.WaitOnCondition( MRN_RECV_THREAD_STARTED );
    }
    _sync.Unlock();
}

void PeerNode::send( const PacketPtr ipacket ) const
{
    mrn_dbg( 5, mrn_printf(FLF, stderr,
                           "node[%d].msg(%p).add_Packet()\n", _rank, &_msg_out) );

    _msg_out.add_Packet( ipacket );
}

int PeerNode::sendDirectly( const PacketPtr ipacket ) const
{
    int retval = 0;

    send( ipacket );

    if( _msg_out.send(_data_sock_fd) == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "msg.send() failed\n") );
        retval = -1;
    }

    return retval;
}

bool PeerNode::has_data() const
{
    struct timeval zeroTimeout;
    zeroTimeout.tv_sec = 0;
    zeroTimeout.tv_usec = 0;

    // set up file descriptor set for the poll
    fd_set rfds;
    FD_ZERO( &rfds );
    FD_SET( _data_sock_fd, &rfds );

    // check if data is available
    int sret = select( _data_sock_fd + 1, &rfds, NULL, NULL, &zeroTimeout );
    if( sret == -1 ){
        mrn_dbg( 1, mrn_printf(FLF, stderr, "select() failed\n") );
        return false;
    }

    // We only put one descriptor in the read set.  Therefore, if the return 
    // value from select() is 1, that descriptor has data available.
    if( sret == 1 ){
        mrn_dbg( 5, mrn_printf(FLF, stderr, "select(): data to be read.\n") );
        return true;
    }

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Leaving PeerNode.has_data(). No data available\n") );
    return false;
}


int PeerNode::flush( bool ignore_threads /*=false*/ ) const
{
    mrn_dbg_func_begin();
    int retval=0;

    if( ignore_threads ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr, "Calling msg.send()\n") );
        if( _msg_out.send( _data_sock_fd ) == -1){
            mrn_dbg( 1, mrn_printf(FLF, stderr, "msg.send() failed\n") );
            retval = -1;
        }
    }
    else if( _network->is_LocalNodeThreaded() ) {
        if( waitfor_FlushCompletion() == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "Flush() failed\n") );
            retval = -1;
        }
    }

    mrn_dbg_func_end();
    return retval;
}

void * PeerNode::recv_thread_main( void* iargs )
{
    std::list< PacketPtr > packet_list;

    send_recv_args_t* args = (send_recv_args_t*) iargs;
    Network* net = args->net;
    Rank rank = args->r;

    PeerNodePtr peer_node = net->get_PeerNode( rank );
    assert( peer_node != PeerNode::NullPeerNode );

    //TLS: setup thread local storage for recv thread
    std::ostringstream namestr;
    if( peer_node->is_parent() )
        namestr << "FROMPARENT(";
    else
        namestr << "FROMCHILD(";
    namestr << peer_node->get_HostName()
            << ':'
            << rank
            << ')'
            << std::ends;
    net->init_ThreadState( UNKNOWN_NODE, namestr.str().c_str() );

    mrn_dbg_func_begin();

    peer_node->_sync.Lock();
    // make sure send thread is ready first, prevents a startup deadlock
    while( ! peer_node->_send_thread_started ) {
        peer_node->_sync.WaitOnCondition( PeerNode::MRN_SEND_THREAD_STARTED );
    }
    peer_node->_recv_thread_started = true;
    peer_node->_sync.SignalCondition( PeerNode::MRN_RECV_THREAD_STARTED );
    peer_node->_sync.Unlock();

    while(true) {

        if( peer_node->has_Failed() )
            break;

        // block for data
        mrn_dbg( 3, mrn_printf(FLF, stderr, "Calling blocking recv() for data\n") );
        int rret = peer_node->recv( packet_list );
        if( (rret == -1) || ((rret == 0) && (packet_list.size() == 0)) ) {
            if( rret == -1 ) {
                mrn_dbg( 3, mrn_printf(FLF, stderr, 
                           "PeerNode.recv() failed!\n") );
            }
            peer_node->mark_Failed();
            break;
        }

        if( peer_node->is_parent() ) {
            if( peer_node->_network->get_LocalChildNode()->proc_PacketsFromParent(packet_list) == -1 )
                mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_PacketsFromParent() failed\n") );
        }
        else {
            if( peer_node->_network->get_LocalParentNode()->proc_PacketsFromChildren(packet_list) == -1 )
                mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_PacketsFromChildren() failed\n") );
        }
    }

    if( net->is_ShuttingDown() ) {
        // handle case where child goes away before sending shutdown ack
        if( peer_node->is_child() ) {
            net->get_LocalParentNode()->proc_DeleteSubTreeAck( Packet::NullPacket );
        }
    }

    mrn_dbg( 3, mrn_printf(FLF, stderr, "I'm going away now!\n") );
    XPlat::Thread::Exit(NULL);

    // this is redundant, but the compiler doesn't know that
    return NULL;
}

void * PeerNode::send_thread_main( void* iargs )
{
    send_recv_args_t* args = (send_recv_args_t*) iargs;
    Network* net = args->net;
    Rank rank = args->r;

    PeerNodePtr peer_node = net->get_PeerNode( rank );
    assert( peer_node != PeerNode::NullPeerNode );

    //TLS: setup thread local storage for send thread
    std::ostringstream namestr;
    if( peer_node->is_parent() )
        namestr << "TOPARENT(";
    else
        namestr << "TOCHILD(";
    namestr << peer_node->get_HostName()
            << ':'
            << rank
            << ')'
            << std::ends;
    net->init_ThreadState( UNKNOWN_NODE, namestr.str().c_str() );

    mrn_dbg_func_begin();

    peer_node->_sync.Lock();
    peer_node->_send_thread_started = true;
    peer_node->_sync.SignalCondition( PeerNode::MRN_SEND_THREAD_STARTED );
    peer_node->_sync.Unlock();

    while(true) {
        mrn_dbg( 3, mrn_printf(FLF, stderr, "Blocking for packets to send ...\n") );
        peer_node->_msg_out.waitfor_MessagesToSend( );

        if( peer_node->has_Failed() )
            break;

        mrn_dbg( 3, mrn_printf(FLF, stderr, "Sending packets ...\n") );
        if( peer_node->_msg_out.send(peer_node->_data_sock_fd) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "msg.send() failed! Thread Exiting\n") );
            peer_node->mark_Failed();
            peer_node->signal_FlushComplete();
            break;
        }
        peer_node->signal_FlushComplete();
    }

    XPlat::Thread::Exit(NULL);

    // this is redundant, but the compiler doesn't know that
    return NULL;
}

int PeerNode::recv(std::list <PacketPtr> &packet_list) const
{
    return _msg_in.recv( _data_sock_fd, packet_list, _rank );
}

int PeerNode::waitfor_FlushCompletion(void) const
{
    int retval = 0;
    _sync.Lock();

    while( _msg_out.size_Packets() > 0 && _available ) {
        _sync.WaitOnCondition( MRN_FLUSH_COMPLETE );
    }
    if( ! _available ) {
        retval = -1;
    }

    _sync.Unlock();

    return retval;
}

void PeerNode::signal_FlushComplete(void) const
{
    _sync.Lock();
    _sync.BroadcastCondition( MRN_FLUSH_COMPLETE );
    _sync.Unlock();
}

void PeerNode::mark_Failed(void)
{
    if( ! has_Failed() ) {
        _sync.Lock();
        _available = false;
        _sync.Unlock();

        XPlat::Thread::Id my_id = 0;
        tsd_t *tsd = ( tsd_t * )tsd_key.Get();
        if( tsd != NULL )
            my_id = tsd->thread_id;

        if( my_id != send_thread_id ) {
            // wake up send thread, if that's not this thread
            _msg_out.add_Packet( Packet::NullPacket );
        }
    }
}

bool PeerNode::has_Failed(void) const
{
    bool rc;
    _sync.Lock();
    rc = ! _available;
    _sync.Unlock();
    return rc;
}

bool PeerNode::is_backend() const 
{
    return ! _is_internal_node;
}

bool PeerNode::is_internal() const 
{
    return _is_internal_node;
}

bool PeerNode::is_parent() const 
{
    return _is_parent;
}

bool PeerNode::is_child() const 
{
    return ! _is_parent;
}

} // namespace MRN
