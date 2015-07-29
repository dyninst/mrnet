/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__PeerNode_h) 
#define __PeerNode_h 1

#include <list>
#include <map>
#include <string>
#include <boost/shared_ptr.hpp>

#include "utils.h"
#include "Message.h"
#include "Protocol.h"

#include "mrnet/CommunicationNode.h"
#include "mrnet/Error.h"
#include "mrnet/Network.h"
#include "xplat/Monitor.h"
#include "xplat/Mutex.h"
#include "xplat/Thread.h"
#include "xplat/TLSKey.h"

namespace MRN
{

class ChildNode;
class ParentNode;
class Network;
class Packet;

class PeerNode: public CommunicationNode, public Error {
    friend class ChildNode;
    friend class Network;
    friend class ParentNode;
 public:

    static PeerNodePtr NullPeerNode;

    static void * recv_thread_main(void * arg);
    static void * send_thread_main(void * arg);

    ~PeerNode();

    int connect_DataSocket(int num_retry=0);
    int connect_EventSocket(int num_retry=0);
    XPlat_Socket get_DataSocketFd(void) const { return _data_sock_fd; }
    XPlat_Socket get_EventSocketFd(void) const { return _event_sock_fd; }
    void set_DataSocketFd( XPlat_Socket isock );
    void set_EventSocketFd( XPlat_Socket isock );
    void close_Sockets(void);
    void close_DataSocket(void);
    void close_EventSocket(void);

    void send( PacketPtr ) const;
    int sendDirectly( PacketPtr ipacket ) const;
    int flush( bool ignore_threads=false ) const;
    int recv( std::list <PacketPtr> & ) const; //blocking recv

    bool has_data(void) const;
    bool is_backend(void) const;
    bool is_internal(void) const;
    bool is_parent(void) const;
    bool is_child(void) const;

    int start_CommunicationThreads(void);    
    void waitfor_CommunicationThreads(void) const;
    bool stop_CommunicationThreads(void);
    
    int waitfor_FlushCompletion(void) const;
    void signal_FlushComplete(void) const;
    void mark_Failed(void);
    bool has_Failed(void) const;
    bool has_Failed_Without_Ack(void) const;

    XPlat::Thread::Id get_SendThrId(void) const;
    XPlat::Thread::Id get_RecvThrId(void) const;

 private:
    PeerNode( Network *, std::string const& ihostname, Port iport, Rank irank,
              bool iis_parent, bool iis_internal );


    //Static data members
    Network * _network;
    XPlat_Socket _data_sock_fd;
    XPlat_Socket _event_sock_fd;
    bool _is_internal_node;
    bool _is_parent;
    bool _recv_thread_started, _send_thread_started;
    XPlat::Thread::Id recv_thread_id, send_thread_id;

    //Dynamic data members

    bool _available;
    mutable Message * _msg_out;
    mutable Message * _msg_in;
    bool _failed_without_ack;
    mutable XPlat::Monitor _sync;
    enum{ MRN_FLUSH_COMPLETE, MRN_RECV_THREAD_STARTED, MRN_SEND_THREAD_STARTED };
};

} // namespace MRN

#endif /* __PeerNode_h  */
