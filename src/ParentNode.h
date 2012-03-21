/****************************************************************************
 *  Copyright 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
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

    int proc_ControlProtocolAck( int ack_tag ) const; 
    bool waitfor_ControlProtocolAcks( int ack_tag, 
                                      unsigned num_acks_expected ) const;
    
    int proc_SubTreeInitDoneReport ( PacketPtr ipacket ) const;
    bool waitfor_SubTreeInitDoneReports(void) const;
    bool waitfor_SubTreeReports(void) const;

    int proc_DeleteSubTree( PacketPtr ipacket ) const;
    int proc_DeleteSubTreeAck( PacketPtr ipacket ) const;
    bool waitfor_DeleteSubTreeAcks(void) const;

    int proc_RecoveryReport( PacketPtr ipacket ) const;

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

    virtual int proc_PortUpdates( PacketPtr ipacket ) const;

    virtual int send_LaunchInfo( PeerNodePtr ) const;

 protected:
    Network * _network;

    enum { NODE_REPORTED, ALL_NODES_REPORTED };
    mutable XPlat::Monitor subtreereport_sync;
    mutable XPlat::Monitor initdonereport_sync;
    mutable unsigned int _num_children, _num_children_reported;
    mutable std::map< int, std::pair<XPlat::Monitor*, unsigned> > ctl_protocol_syncs;
    mutable XPlat::Mutex cps_sync;

    virtual int proc_PacketFromChildren( PacketPtr ipacket );

    ParentNode() {}

 private:
    XPlat_Socket listening_sock_fd;

};


} // namespace MRN

#endif /* __parentnode_h */
