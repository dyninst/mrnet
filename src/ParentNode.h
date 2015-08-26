/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__parentnode_h)
#define __parentnode_h 1

#include <map>
#include <list>
#include <string>

#include "mrnet/CommunicationNode.h"
#include "mrnet/Error.h"
#include "mrnet/Packet.h"
#include "mrnet/Stream.h"
#include "xplat/Monitor.h"
#include "xplat/Mutex.h"
#include "Protocol.h"
#include "PeerNode.h"

namespace MRN
{

class Network;
class Stream;

class ParentNode: public virtual Error, 
                  public virtual CommunicationNode
{
    friend class PeerNode;

 public:
    ParentNode( Network* inetwork, 
                std::string const& myhost, 
                Rank myrank,
                int listeningSocket = -1,
                Port listeningPort = UnknownPort );
    virtual ~ParentNode(void);

    int recv_PacketsFromChildren( std::list< PacketPtr > & ipackets,
                                  bool blocking = true ) const;
    int flush_PacketsToChildren(void) const;
    int proc_PacketsFromChildren( std::list< PacketPtr > & ipackets );

    virtual int proc_DataFromChildren( PacketPtr ipacket ) const = 0;

    int proc_ControlProtocolAck( PacketPtr ipacket ); 
    bool waitfor_ControlProtocolAcks( int ack_tag, 
                                      unsigned num_acks_expected ) const;
    
    int proc_SubTreeInitDoneReport ( PacketPtr ipacket ) const;
    bool waitfor_SubTreeInitDoneReports(void) const;

    int proc_RecoveryReport( PacketPtr ipacket ) const;
    int proc_FilterLoadEvent( PacketPtr ipacket ) const;
    int proc_Event( PacketPtr ipacket ) const;
    int send_Event( PacketPtr ipacket ) const;

    Stream * proc_newStream( PacketPtr ipacket ) const;
    int proc_deleteStream( PacketPtr ipacket ) const;

    int proc_newFilter( PacketPtr ipacket ) const;
    int proc_FilterParams( FilterType, PacketPtr &ipacket ) const;

    int get_ListeningSocket(void) const { return listening_sock_fd; }
    int getConnections( int **conns, unsigned int *nConns ) const;

    int proc_NewChildDataConnection( PacketPtr ipacket, int sock );
    PeerNodePtr find_ChildNodeByRank( int irank );

    void init_numChildrenExpected( SerialGraph& sg );
    unsigned int get_numChildrenExpected(void) const  { return _num_children; }

    // Added by Taylor:
    void set_numChildrenExpected( const int num_children );

    virtual int proc_PortUpdates( PacketPtr ipacket ) const;

    virtual int send_LaunchInfo( PeerNodePtr ) const;

    int abort_ActiveControlProtocols();

    void notify_OfChildFailure();

 protected:
    Network * _network;
    struct ControlProtocol {
        mutable XPlat::Monitor *sync;
        unsigned num_acked;
        bool aborted;
    };

    enum { NODE_REPORTED, ALL_NODES_REPORTED };
    mutable XPlat::Monitor subtreereport_sync;
    mutable XPlat::Monitor initdonereport_sync;
    mutable unsigned int _num_children, _num_children_to_del;
    mutable unsigned int _num_children_reported_init;
    mutable unsigned int _num_children_reported_del;
    mutable XPlat::Mutex failed_sync;
    mutable unsigned int _num_failed_children;
    mutable std::map< int, struct ControlProtocol > active_ctl_protocols;
    mutable XPlat::Mutex cps_sync;
    mutable std::vector<char *> _filter_error_host;
    mutable std::vector<char *> _filter_error_soname;
    mutable std::vector<unsigned> _filter_error_funcname;
    mutable XPlat::Mutex event_lock;
    mutable unsigned _filter_children_reported;
    mutable unsigned _filter_children_waiting;
    virtual int proc_PacketFromChildren( PacketPtr ipacket );

    ParentNode() {}

 private:
    int abort_ControlProtocol( struct ControlProtocol &cp );
    XPlat_Socket listening_sock_fd;

};


} // namespace MRN

#endif /* __parentnode_h */
