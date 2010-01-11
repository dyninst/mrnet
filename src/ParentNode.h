/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
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
    friend class Aggregator;
    friend class Synchronizer;
    friend class PeerNode;
 public:
    ParentNode( Network* inetwork, 
                std::string const& myhost, 
                Rank myrank,
                int listeningSocket = -1,
                Port listeningPort = UnknownPort );
    virtual ~ ParentNode( void );

    int recv_PacketsFromChildren( std::list< PacketPtr > & ipackets,
                                  bool blocking = true ) const;
    int flush_PacketsToChildren( void ) const;
    int proc_PacketsFromChildren( std::list< PacketPtr > & ipackets );

    virtual int proc_DataFromChildren( PacketPtr ipacket ) const = 0;

    virtual int proc_NewParentReportFromParent( PacketPtr ipacket ) const=0;

    int proc_newSubTreeReport( PacketPtr ipacket ) const;
    int proc_DeleteSubTree( PacketPtr ipacket ) const;
    int proc_DeleteSubTreeAck( PacketPtr ipacket ) const;
    int proc_TopologyReport( PacketPtr ipacket ) const;
    int proc_TopologyReportAck( PacketPtr ipacket ) const;

    int proc_FailureReport( PacketPtr ipacket ) const;
    int proc_RecoveryReport( PacketPtr ipacket ) const;

    int proc_Event( PacketPtr ipacket ) const;
    int send_Event( PacketPtr ipacket ) const;

    Stream * proc_newStream( PacketPtr ipacket ) const;
    int proc_deleteStream( PacketPtr ipacket ) const;
    int proc_closeStream( PacketPtr ipacket ) const;

    int proc_newFilter( PacketPtr ipacket ) const;
    int proc_FilterParams( FilterType, PacketPtr &ipacket ) const;

    int get_ListeningSocket( void ) const { return listening_sock_fd; }
    int getConnections( int **conns, unsigned int *nConns ) const;

    int proc_NewChildDataConnection( PacketPtr ipacket, int sock );
    PeerNodePtr find_ChildNodeByRank( int irank );

    bool waitfor_SubTreeReports( void ) const;
    bool waitfor_DeleteSubTreeAcks( void ) const ;
    bool waitfor_TopologyReportAcks( void ) const ;

    void init_numChildrenExpected( SerialGraph& sg );
    unsigned int get_numChildrenExpected( void ) const  { return _num_children; }

 protected:
    Network * _network;

    enum { ALLNODESREPORTED };
    mutable XPlat::Monitor subtreereport_sync;
    mutable unsigned int _num_children, _num_children_reported;

    virtual int proc_PacketFromChildren( PacketPtr ipacket );

 private:
    int listening_sock_fd;
};

//bool lt_PeerNodePtr( PeerNodePtr p1, PeerNodePtr p2 );
//bool equal_PeerNodePtr( PeerNodePtr p1, PeerNodePtr p2 );

}                               // namespace MRN

#endif                          /* __parentnode_h */
