/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__childnode_h)
#define __childnode_h

#include "BackEndNode.h"
#include "PeerNode.h"
#include "mrnet_lightweight/Network.h"
#include "vector.h"

int ChildNode_proc_PortUpdate(BackEndNode_t * be,
        Packet_t* packet);

int ChildNode_ack_PortUpdate(BackEndNode_t* be);

int ChildNode_init_newChildDataConnection (  BackEndNode_t* be, 
                                             PeerNode_t* iparent,
                                             Rank ifailed_rank);

int ChildNode_send_SubTreeInitDoneReport(BackEndNode_t* be);

int ChildNode_send_NewSubTreeReport( BackEndNode_t* be);

int ChildNode_proc_PacketsFromParent(BackEndNode_t* be, vector_t* packets);

int ChildNode_proc_PacketFromParent( BackEndNode_t* be,  Packet_t* packet);

int ChildNode_ack_DeleteSubTree( BackEndNode_t* be);

int ChildNode_proc_RecoveryReport( BackEndNode_t* be,  Packet_t* ipacket);

int ChildNode_proc_EnablePerfData( BackEndNode_t* be,  Packet_t* ipacket);

int ChildNode_proc_DisablePerfData( BackEndNode_t* be,  Packet_t* ipacket);

int ChildNode_proc_CollectPerfData( BackEndNode_t* be,  Packet_t* ipacket);

int ChildNode_proc_PrintPerfData( BackEndNode_t* be,  Packet_t* ipacket);

int ChildNode_proc_TopologyReport(BackEndNode_t* be, Packet_t* ipacket);

int ChildNode_ack_TopologyReport(BackEndNode_t* be);

#endif /* __childnode_h */
