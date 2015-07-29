/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__childnode_h)
#define __childnode_h 1

#include <string>

#include "Message.h"
#include "PeerNode.h"

#include "xplat/Mutex.h"

namespace MRN
{

class Network;

class ChildNode: public virtual Error, 
                 public virtual CommunicationNode
{
  public:
    ChildNode( Network *inetwork,
               std::string const& ihostname, Rank irank,
               std::string const& iphostname, Port ipport, Rank iprank );
    virtual ~ChildNode(void) {}

    int proc_PacketsFromParent( std::list<PacketPtr> & );
    virtual int proc_DataFromParent( PacketPtr ipacket ) const=0;

    bool ack_ControlProtocol( int ack_tag, bool success ) const;

    int proc_RecoveryReport( PacketPtr ipacket ) const;

    int send_SubTreeInitDoneReport(void) const;

    int proc_PortUpdate(PacketPtr ipacket ) const;

    int proc_EnablePerfData( PacketPtr ipacket ) const;
    int proc_DisablePerfData( PacketPtr ipacket ) const;
    int proc_CollectPerfData( PacketPtr ipacket ) const;
    int proc_PrintPerfData( PacketPtr ipacket ) const;
    
    // Network Settings (topology and environment)
    int proc_NetworkSettings( PacketPtr ipacket ) const;

    /* Failure Recovery */
    int proc_EnableFailReco( PacketPtr ipacket ) const;
    int proc_DisableFailReco( PacketPtr ipacket ) const;

    int recv_PacketsFromParent( std::list<PacketPtr> &packet_list ) const;
    int send_EventsToParent( ) const;
    bool has_PacketsFromParent( ) const;

    void error( ErrorCode, Rank, const char *, ... ) const;

    int init_newChildDataConnection( PeerNodePtr iparent,
                                     Rank ifailed_rank = UnknownRank );

 protected:
    Network * _network;
    virtual int proc_PacketFromParent( PacketPtr cur_packet );

    ChildNode(void) {}

 private:
    uint16_t _incarnation; //incremented each time child connects to new parent

    mutable XPlat::Mutex _sync;
};

} // namespace MRN

#endif /* __childnode_h */
