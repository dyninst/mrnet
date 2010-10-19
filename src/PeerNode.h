/****************************************************************************
 *  Copyright 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__PeerNode_h) 
#define __PeerNode_h 1

#include <list>
#include <map>
#include <string>
#include <boost/shared_ptr.hpp>

#include "Message.h"
#include "Protocol.h"
#include "mrnet/CommunicationNode.h"
#include "mrnet/Error.h"
#include "mrnet/Types.h"
#include "mrnet/Network.h"
#include "xplat/Thread.h"
#include "xplat/Monitor.h"
#include "xplat/Mutex.h"

namespace MRN
{

class ChildNode;
class ParentNode;
class Network;
class Packet;

class PeerNode: public CommunicationNode, public Error {
    friend class ChildNode;
    friend class Network;
 public:

    static PeerNodePtr NullPeerNode;

    static void * recv_thread_main(void * arg);
    static void * send_thread_main(void * arg);

    int connect_DataSocket(void);
    int connect_EventSocket(void);

    int new_InternalNode( int listening_sock_fd,
                          std::string parent_hostname, Port parent_port,
                          std::string commnode ) const;
    int new_Application( int listening_sock_fd,
                         std::string parent_hostname, Port parent_port,
                         Rank be_rank,
                         std::string &cmd, std::vector <std::string> &args ) const;


    void send( const PacketPtr ) const;
    int sendDirectly( const PacketPtr ipacket ) const;
    int flush( bool ignore_threads=false ) const;
    int recv( std::list <PacketPtr> & ) const; //blocking recv

    bool has_data(void) const;
    bool is_backend(void) const;
    bool is_internal(void) const;
    bool is_parent(void) const;
    bool is_child(void) const;

    void set_DataSocketFd( int isock ) { _data_sock_fd = isock; }
    int get_DataSocketFd(void) const { return _data_sock_fd; }
    int get_EventSocketFd(void) const { return _event_sock_fd; }
    int start_CommunicationThreads(void);

    int waitfor_FlushCompletion(void) const;
    void signal_FlushComplete(void) const;
    void mark_Failed(void);
    bool has_Failed(void) const;

 private:
    PeerNode( Network *, std::string const& ihostname, Port iport, Rank irank,
              bool iis_parent, bool iis_internal );

    //Static data members
    Network * _network;
    int _data_sock_fd;
    int _event_sock_fd;
    bool _is_internal_node;
    bool _is_parent;
    XPlat::Thread::Id recv_thread_id, send_thread_id;

    //Dynamic data members
    mutable Message _msg_out;
    mutable Message _msg_in;
    bool _available;
    mutable XPlat::Monitor _sync;
    enum{ MRN_FLUSH_COMPLETE };
};

} // namespace MRN

#endif /* __PeerNode_h  */
