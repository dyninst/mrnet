/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <sstream>

#include "config.h"
#include "utils.h"
#include "ChildNode.h"
#include "ParentNode.h"
#include "PeerNode.h"

#include "mrnet/MRNet.h"
#include "xplat/Process.h"
#include "xplat/SocketUtils.h"
#include "xplat/Error.h"
#include "xplat/NetUtils.h"

using namespace std;

namespace MRN
{

/*====================================================*/
/*  PeerNode CLASS METHOD DEFINITIONS            */
/*====================================================*/

PeerNodePtr PeerNode::NullPeerNode;
XPlat::Mutex PeerNode::poll_timeout_mutex;
int PeerNode::poll_timeout=0;

PeerNode::PeerNode( Network * inetwork, std::string const& ihostname, Port iport,
                    Rank irank, bool iis_parent, bool iis_internal )
    : CommunicationNode(ihostname, iport, irank ), _network(inetwork),
     _data_sock_fd(0), _event_sock_fd(0),
     _is_internal_node(iis_internal), _is_parent(iis_parent), _available(true)
{
    _flush_sync.RegisterCondition( MRN_FLUSH_COMPLETE );
}


int PeerNode::connect_DataSocket( void )
{
    mrn_dbg(3, mrn_printf(FLF, stderr, "Creating data connection to (%s:%d) ...\n",
                          _hostname.c_str(), _port));

    if( connectHost(&_data_sock_fd, _hostname.c_str(), _port) == -1) {
        error( ERR_SYSTEM, _rank, "connectHost() failed" );
        mrn_dbg(1, mrn_printf(FLF, stderr, "connectHost() failed\n"));
        return -1;
    }
    
    mrn_dbg(3, mrn_printf(FLF, stderr,
                          "new data socket %d\n", _data_sock_fd));
    return 0;
}

int PeerNode::connect_EventSocket( void )
{
    mrn_dbg(3, mrn_printf(FLF, stderr, "Creating event connection to (%s:%d) ...\n",
                          _hostname.c_str(), _port));

    if( connectHost(&_event_sock_fd, _hostname.c_str(), _port) == -1 ) {
        error( ERR_SYSTEM, _rank, "connectHost() failed" );
        mrn_dbg(1, mrn_printf(FLF, stderr, "connectHost() failed\n"));
        return -1;
    }
    
    mrn_dbg(3, mrn_printf(FLF, stderr,
                          "new event socket %d\n", _event_sock_fd));
    return 0;
}

int PeerNode::start_CommunicationThreads( void )
{
    int retval;

    mrn_dbg(3, mrn_printf(FLF, stderr, "Creating recv_thread ...\n"));
    retval = XPlat::Thread::Create( recv_thread_main, &_rank, &recv_thread_id  );
    mrn_dbg(3, mrn_printf(FLF, stderr, "id: 0x%x\n", recv_thread_id));
    if(retval != 0){
        error( ERR_SYSTEM, _rank, "XPlat::Thread::Create() failed: %s\n",
               strerror(errno) );
        mrn_dbg(1, mrn_printf(FLF, stderr, "recv_thread creation failed...\n"));
    }

    mrn_dbg(3, mrn_printf(FLF, stderr, "Creating send_thread ...\n"));
    retval = XPlat::Thread::Create( send_thread_main, &_rank, &send_thread_id );
    mrn_dbg(3, mrn_printf(FLF, stderr, "id: 0x%x\n", send_thread_id));
    if(retval != 0){
        error( ERR_SYSTEM, _rank, "XPlat::Thread::Create() failed: %s\n",
               strerror(errno) );
        mrn_dbg(1, mrn_printf(FLF, stderr, "send_thread creation failed...\n"));
    }

    return retval;
}

int PeerNode::send(const PacketPtr ipacket) const
{
    mrn_dbg(3, mrn_printf(FLF, stderr,
                          "node[%d].msg(%p).add_packet()\n", _rank, &_msg_out ));

    _msg_out.add_Packet(ipacket);

    mrn_dbg(3, mrn_printf(FLF, stderr, "Leaving PeerNode.send()\n"));
    return 0;
}

int PeerNode::sendDirectly( const PacketPtr ipacket ) const
{
    mrn_dbg_func_begin();
    int retval=0;

    _msg_out.add_Packet(ipacket);

    mrn_dbg(3, mrn_printf(FLF, stderr,
                          "node[%d].msg(%p).add_packet()\n", _rank, &_msg_out ));
    if( _msg_out.send( _data_sock_fd ) == -1){
        mrn_dbg(1, mrn_printf(FLF, stderr, "msg.send() failed\n"));
        retval = -1;
    }
    mrn_dbg_func_end();
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
        mrn_dbg(1, mrn_printf(FLF, stderr, "select() failed\n"));
        return false;
    }

    // We only put one descriptor in the read set.  Therefore, if the return 
    // value from select() is 1, that descriptor has data available.
    if( sret == 1 ){
        mrn_dbg(5, mrn_printf(FLF, stderr, "select(): data to be read.\n"));
        return true;
    }

    mrn_dbg(3, mrn_printf(FLF, stderr, "Leaving PeerNode.has_data(). No data available\n"));
    return false;
}


int PeerNode::flush( bool ignore_threads /*=false*/ ) const
{
    mrn_dbg_func_begin();
    int retval=0;

    if( ignore_threads ) {
        mrn_dbg(3, mrn_printf(FLF, stderr, "Calling msg.send()\n"));
        if( _msg_out.send( _data_sock_fd ) == -1){
            mrn_dbg(1, mrn_printf(FLF, stderr, "msg.send() failed\n"));
            retval = -1;
        }
    }
    else{
        if( _network->is_LocalNodeThreaded() && waitfor_FlushCompletion() == -1) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "Flush() failed\n"));
            retval = -1;
        }
    }

    mrn_dbg_func_end();
    return retval;
}

void * PeerNode::recv_thread_main(void * args)
{
    std::list< PacketPtr > packet_list;

    Rank rank = *((Rank*)args);
    PeerNodePtr peer_node = _global_network->get_PeerNode( rank );
    assert( peer_node != PeerNode::NullPeerNode );

    //TLS: setup thread local storage for recv thread
    std::ostringstream namestr;

    if( peer_node->is_parent() )
        namestr << "FROMPARENT(";
    else
        namestr << "FROMCHILD(";

    namestr << peer_node->_network->get_LocalHostName()
            << ':'
            << peer_node->_network->get_LocalRank()
            << "<-"
            << peer_node->get_HostName()
            << ':'
            << peer_node->get_Rank()
            << ')'
            << std::ends;

    int status;
    tsd_t * local_data = new tsd_t;
    local_data->thread_id = XPlat::Thread::GetId();
    local_data->thread_name = strdup( namestr.str().c_str() );
    if( (status = tsd_key.Set(local_data)) != 0 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "XPlat::TLSKey::Set(): %s\n",
                   strerror(status)));
        XPlat::Thread::Exit(args);
    }

    mrn_dbg_func_begin();

    while(true) {
        // block for data
        mrn_dbg(3, mrn_printf(FLF, stderr, "Calling blocking recv() for data\n"));
        int rret = peer_node->recv( packet_list );
        if( (rret == -1) || ((rret == 0) && (packet_list.size() == 0)) ) {
            if( rret == -1 ) {
                mrn_dbg(1, mrn_printf(FLF, stderr, 
                           "PeerNode recv failed - thread terminating\n"));
            }
            XPlat::Thread::Exit(args);
        }

        if( peer_node->is_parent() ) {
            if( peer_node->_network->get_LocalChildNode()->proc_PacketsFromParent(packet_list) == -1 )
                mrn_dbg(1, mrn_printf(FLF, stderr, "proc_PacketsFromParent() failed\n"));
        }
        else {
            if( peer_node->_network->get_LocalParentNode()->proc_PacketsFromChildren(packet_list) == -1 )
                mrn_dbg(1, mrn_printf(FLF, stderr, "proc_PacketsFromChildren() failed\n"));
        }
    }

    return NULL;
}

void * PeerNode::send_thread_main(void * args)
{
    Rank rank = *((Rank*)args);
    PeerNodePtr peer_node = _global_network->get_PeerNode( rank );
    assert( peer_node != PeerNode::NullPeerNode );

    //TLS: setup thread local storage for send thread
    std::ostringstream namestr;

    if( peer_node->is_parent() )
        namestr << "TOPARENT(";
    else
        namestr << "TOCHILD(";

    namestr << peer_node->_network->get_LocalHostName()
            << ':'
            << peer_node->_network->get_LocalRank()
            << "<-"
            << peer_node->get_HostName()
            << ':'
            << peer_node->get_Rank()
            << ')'
            << std::ends;

    int status;
    tsd_t * local_data = new tsd_t;
    local_data->thread_id = XPlat::Thread::GetId();
    local_data->thread_name = strdup( namestr.str().c_str() );
    if( (status = tsd_key.Set(local_data)) != 0 ) {
        mrn_dbg(1, mrn_printf(0,0,0, stderr, "XPlat::TLSKey::Set(): %s\n",
                              strerror(status))); 
        XPlat::Thread::Exit(args);
    }

    mrn_dbg_func_begin();

    while(true) {
        mrn_dbg(3, mrn_printf(FLF, stderr, "Blocking for packets to send ...\n"));
        peer_node->_msg_out.waitfor_MessagesToSend( );

        mrn_dbg(3, mrn_printf(FLF, stderr, "Sending packets ...\n"));
        if( peer_node->_msg_out.send(peer_node->_data_sock_fd) == -1 ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "msg.send() failed. Thread Exiting\n"));
            peer_node->mark_Failed();
            XPlat::Thread::Exit(args);
        }
        peer_node->signal_FlushComplete();
    }

    return NULL;
}

int PeerNode::recv(std::list <PacketPtr> &packet_list) const
{
    return _msg_in.recv( _data_sock_fd, packet_list, _rank );
}

int PeerNode::waitfor_FlushCompletion( void ) const
{
    int retval = 0;
    _flush_sync.Lock();

    while( _msg_out.size_Packets() > 0 && _available ) {
        _flush_sync.WaitOnCondition( MRN_FLUSH_COMPLETE );
    }
    if( !_available ) {
        retval = -1;
    }

    _flush_sync.Unlock();

    return retval;
}

void PeerNode::signal_FlushComplete( void ) const
{
    _flush_sync.Lock();

    _flush_sync.BroadcastCondition( MRN_FLUSH_COMPLETE );

    _flush_sync.Unlock();
}

void PeerNode::mark_Failed( void )
{
    _flush_sync.Lock();

    _available = false;
    _flush_sync.BroadcastCondition( MRN_FLUSH_COMPLETE );

    _flush_sync.Unlock();
}

bool PeerNode::is_backend() const {
    return !_is_internal_node;
}

bool PeerNode::is_internal() const {
    return _is_internal_node;
}

bool PeerNode::is_parent() const {
    return _is_parent;
}

bool PeerNode::is_child() const {
    return !_is_parent;
}

void PeerNode::set_BlockingTimeOut(int itimeout)
{
    poll_timeout_mutex.Lock();
    poll_timeout = itimeout;
    poll_timeout_mutex.Unlock();
}

int PeerNode::get_BlockingTimeOut( )
{
    int ret;

    poll_timeout_mutex.Lock();
    ret = poll_timeout;
    poll_timeout_mutex.Unlock();

    return ret;
}

} // namespace MRN
