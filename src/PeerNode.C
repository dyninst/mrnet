/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "PeerNode.h"
#include "ChildNode.h"
#include "ParentNode.h"

#include "mrnet/Network.h"
#include "xplat/SocketUtils.h"
#include "xplat/Error.h"
#include "xplat/NetUtils.h"

#include <sstream>
#include <cxxabi.h>
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
      _data_sock_fd(XPlat::SocketUtils::InvalidSocket), 
      _event_sock_fd(XPlat::SocketUtils::InvalidSocket),
      _is_internal_node(iis_internal), _is_parent(iis_parent), 
      _recv_thread_started(false), _send_thread_started(false),
      recv_thread_id(0), send_thread_id(0), 
      _available(true), _msg_out( new Message(inetwork)),
      _msg_in(new Message(inetwork)), _failed_without_ack(false)
{
    _sync.RegisterCondition( MRN_FLUSH_COMPLETE );
    _sync.RegisterCondition( MRN_RECV_THREAD_STARTED );
    _sync.RegisterCondition( MRN_SEND_THREAD_STARTED );
}

PeerNode::~PeerNode()
{
    delete _msg_out;
    delete _msg_in;
}

int PeerNode::connect_DataSocket( int num_retry /* =0 */ )
{
    XPlat_Socket data_fd = XPlat::SocketUtils::InvalidSocket;
    mrn_dbg( 3, mrn_printf(FLF, stderr, "Creating data connection to (%s:%d) ...\n",
                          _hostname.c_str(), _port) );

    if( connectHost(&data_fd, _hostname, _port, num_retry) == -1) {
        error( ERR_SYSTEM, _rank, "connectHost() failed" );
        return -1;
    }
    
    set_DataSocketFd( data_fd );
    return 0;
}

int PeerNode::connect_EventSocket( int num_retry /* =0 */ )
{
    XPlat_Socket evt_fd = XPlat::SocketUtils::InvalidSocket;
    mrn_dbg( 3, mrn_printf(FLF, stderr, "Creating event connection to (%s:%d) ...\n",
                          _hostname.c_str(), _port) );

    if( connectHost(&evt_fd, _hostname, _port, num_retry) == -1 ) {
        error( ERR_SYSTEM, _rank, "connectHost() failed" );
        return -1;
    }
    
    set_EventSocketFd( evt_fd );
    return 0;
}

void PeerNode::set_DataSocketFd( XPlat_Socket data_sock_fd )
{
    _sync.Lock();
    _data_sock_fd = data_sock_fd;
    _sync.Unlock();
    mrn_dbg( 3, mrn_printf(FLF, stderr,
                           "new data socket %d\n", data_sock_fd) );
}

void PeerNode::set_EventSocketFd( XPlat_Socket evt_sock_fd )
{
    _sync.Lock();
    _event_sock_fd = evt_sock_fd;
    _sync.Unlock();
    mrn_dbg( 3, mrn_printf(FLF, stderr,
                           "new event socket %d\n", evt_sock_fd) );
}

void PeerNode::close_Sockets(void)
{
    mrn_dbg_func_begin();
    
    close_DataSocket();
    close_EventSocket();
}

void PeerNode::close_DataSocket(void)
{
    mrn_dbg(5, mrn_printf(FLF, stderr,
                          "Closing data(%d) socket to %s:%u\n",
                          _data_sock_fd,
                          _hostname.c_str(),
                          _rank));
    _sync.Lock();
    if( _data_sock_fd != XPlat::SocketUtils::InvalidSocket) {
        if( ! XPlat::SocketUtils::Close(_data_sock_fd) ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, 
                                  "error on close(data_sock_fd) for peer %u\n", _rank));
        }
        _data_sock_fd = XPlat::SocketUtils::InvalidSocket;
    }
    _sync.Unlock();   
}

void PeerNode::close_EventSocket(void)
{
    mrn_dbg(5, mrn_printf(FLF, stderr,
                          "Closing event(%d) socket to %s:%u\n",
                          _event_sock_fd,
                          _hostname.c_str(),
                          _rank));
    _sync.Lock();
    if( _event_sock_fd != XPlat::SocketUtils::InvalidSocket) {
        if( ! XPlat::SocketUtils::Close(_event_sock_fd) ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, 
                                  "error on close(event_sock_fd) for peer %u\n", _rank));
        }
        _event_sock_fd = XPlat::SocketUtils::InvalidSocket;
    }
    _sync.Unlock();   
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

    XPlat::Thread::Id send_id, recv_id;

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Creating send_thread ...\n") );
    retval = XPlat::Thread::Create( send_thread_main, (void*)args, &send_id );
    mrn_dbg( 3, mrn_printf(FLF, stderr, "id: 0x%x\n", send_id) );
    if( retval != 0 ) {
        string err = XPlat::Error::GetErrorString( retval );
        error( ERR_SYSTEM, _rank, "XPlat::Thread::Create() failed: %s",
               err.c_str() );
        return retval;
    }

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Creating recv_thread ...\n") );
    retval = XPlat::Thread::Create( recv_thread_main, (void*)args, &recv_id  );
    mrn_dbg( 3, mrn_printf(FLF, stderr, "id: 0x%x\n", recv_id) );
    if( retval != 0 ) {
        string err = XPlat::Error::GetErrorString( retval );
        error( ERR_SYSTEM, _rank, "XPlat::Thread::Create() failed: %s",
               err.c_str() );
        return retval;
    }
    
    _sync.Lock();
    send_thread_id = send_id;
    recv_thread_id = recv_id;
    _sync.Unlock();

    waitfor_CommunicationThreads();

    free( args );

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

bool PeerNode::stop_CommunicationThreads(void) {
    bool ret_val = true;
    XPlat::Thread::Id my_id = XPlat::XPlat_TLSKey->GetTid();
    if(my_id == get_SendThrId() || my_id == get_RecvThrId()) {
        mrn_dbg(1, mrn_printf(FLF, stderr,
                    "Comm threads cannot be stopped by a comm thread!\n"));
        return false;
    }

    _sync.Lock();
    // mark as failed
    _available = false;

    // Clear the send buffer on this socket
    _msg_out->send(_data_sock_fd);

    // wake up recv thread
    // This relies on the remote end closing the socket once it receives EOF
    mrn_dbg(5, mrn_printf(FLF, stderr, "Calling shutdown() on data socket\n"));
    errno = 0;
    if( -1 == XPlat::SocketUtils::Shutdown(_data_sock_fd, XPLAT_SHUT_WR) ) {
        mrn_dbg(1, mrn_printf(FLF, stderr,
                    "Failed to shutdown() data socket: %s\n",
                    strerror(errno)));
        ret_val = false;
    }

    // wake up send thread to shut it down
    // shutdown packet send will fail, but that's expected
    mrn_dbg(5, mrn_printf(FLF, stderr, "stopping send thread\n"));
    PacketPtr packet( new Packet(CTL_STRM_ID, PROT_SHUTDOWN, NULL) );
    _msg_out->add_Packet( packet );

    // Wait until the remote end has done an orderly shutdown to close socket
    size_t bb_size = 8;
    char *bogus_buffer = (char *)malloc(sizeof(char) * bb_size);
    while(1) {
        //mrn_dbg(5, mrn_printf(FLF, stderr, "receiving on data socket\n"));
        ssize_t recv_ret = XPlat::SocketUtils::recv(_data_sock_fd, bogus_buffer, bb_size);
        if( recv_ret <= 0 ) break;
    }
    free(bogus_buffer);
    mrn_dbg(5, mrn_printf(FLF, stderr, "closing data socket\n"));
    close_DataSocket();

    _sync.Unlock();

    return ret_val;
}

void PeerNode::send(PacketPtr ipacket ) const
{
    mrn_dbg(5, mrn_printf(FLF, stderr,
                          "msg(%p).add_Packet()\n", _msg_out));

    _msg_out->add_Packet( ipacket );
}

int PeerNode::sendDirectly( PacketPtr ipacket ) const
{
    int retval = 0;
    send( ipacket );
    if( _msg_out->send(_data_sock_fd) == -1 ) {
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

    if (_data_sock_fd < 0)
        return false;

    FD_SET( _data_sock_fd, &rfds );

    // check if data is available
    int sret = select( _data_sock_fd + 1, &rfds, NULL, NULL, &zeroTimeout );
    if( sret == -1 ) {
        int err = XPlat::NetUtils::GetLastError();
        if( EINTR != err )
            mrn_dbg( 1, mrn_printf(FLF, stderr, "select() failed\n") );
        return false;
    }

    // We only put one descriptor in the read set.  Therefore, if the return 
    // value from select() is 1, that descriptor has data available.
    if( sret == 1 ) {
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
        if( _msg_out->send( _data_sock_fd ) == -1){
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
  try {
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
            << ')' ;
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

    // handle case where child goes away before sending shutdown ack
    if( peer_node->is_child() ) {
        if( net->is_ShuttingDown() ) {
            //net->get_LocalParentNode()->proc_DeleteSubTreeAck( Packet::NullPacket );
        } else {
            if(peer_node->has_Failed()) {
                // This assumes that if the thread has broken out of the loop
                // to this check, there is no way it would have processed a 
                // DeleteSubTreeAck. If network is not currently shutting down,
                // we must set this flag because no one is waiting for
                // DeleteSubTreeAcks at this time.
                peer_node->_failed_without_ack = true;
            }
        }
    }

    peer_node->_sync.Lock();
    peer_node->recv_thread_id = 0;
    peer_node->_sync.Unlock();

    mrn_dbg( 3, mrn_printf(FLF, stderr, "I'm going away now!\n") );
    Network::free_ThreadState();

  } catch(... ) { throw; }

    return NULL;
}

void * PeerNode::send_thread_main( void* iargs )
{
  try {
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
            << ')' ;
    net->init_ThreadState( UNKNOWN_NODE, namestr.str().c_str() );

    mrn_dbg_func_begin();

    peer_node->_sync.Lock();
    peer_node->_send_thread_started = true;
    peer_node->_sync.SignalCondition( PeerNode::MRN_SEND_THREAD_STARTED );
    peer_node->_sync.Unlock();

    while(true) {
        mrn_dbg( 3, mrn_printf(FLF, stderr, "Blocking for packets to send ...\n") );
        peer_node->_msg_out->waitfor_MessagesToSend( );

        if( peer_node->has_Failed() )
            break;

        mrn_dbg( 3, mrn_printf(FLF, stderr, "Sending packets ...\n") );
        if( peer_node->_msg_out->send(peer_node->_data_sock_fd) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "msg.send() failed! Thread Exiting\n") );
            peer_node->mark_Failed();
            peer_node->signal_FlushComplete();
            break;
        }
        peer_node->signal_FlushComplete();
    }

    peer_node->_sync.Lock();
    peer_node->send_thread_id = 0;
    peer_node->_sync.Unlock();

    mrn_dbg( 3, mrn_printf(FLF, stderr, "I'm going away now!\n") );
    Network::free_ThreadState();

  } catch(... ) { throw; }

    return NULL;
}

int PeerNode::recv(std::list <PacketPtr> &packet_list) const
{
    return _msg_in->recv( _data_sock_fd, packet_list, _rank );
}

int PeerNode::waitfor_FlushCompletion(void) const
{
    int retval = 0;
    _sync.Lock();

    while( _msg_out->size_Packets() > 0 && _available ) {
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

        // wake up send thread, if that's not this thread
        XPlat::Thread::Id my_id = XPlat::XPlat_TLSKey->GetTid();
        if( my_id != get_SendThrId() ) {
            // shutdown packet send will fail, but that's expected
            PacketPtr packet( new Packet(CTL_STRM_ID, PROT_SHUTDOWN, NULL) );
            _msg_out->add_Packet( packet );
        }

        close_DataSocket();
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

bool PeerNode::has_Failed_Without_Ack(void) const
{
    bool rc;
    _sync.Lock();
    rc = _failed_without_ack;
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

XPlat::Thread::Id PeerNode::get_SendThrId(void) const
{
    XPlat::Thread::Id rc;
    _sync.Lock();
    rc = send_thread_id;
    _sync.Unlock();
    return rc;
}

XPlat::Thread::Id PeerNode::get_RecvThrId(void) const
{
    XPlat::Thread::Id rc;
    _sync.Lock();
    rc = recv_thread_id;
    _sync.Unlock();
    return rc;
}

} // namespace MRN
