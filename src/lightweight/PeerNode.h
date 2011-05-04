/****************************************************************************
 *  Copyright 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__peernode_h)
#define __peernode_h 1

#include "mrnet_lightweight/Network.h"
#include "Message.h"
#include "mrnet_lightweight/Packet.h"
#include "mrnet_lightweight/Types.h"
#include "vector.h"

struct PeerNode_t { 
   Network_t* net;
  char* hostname;
  Port port;
  Rank rank;
  int data_sock_fd;
  int event_sock_fd;
  int is_internal_node;
  int is_parent;

  int available;

  Message_t msg_out;
  Message_t msg_in;

};

typedef struct PeerNode_t PeerNode_t;

 PeerNode_t* new_PeerNode_t(Network_t* inetwork,
                            char* iphostname,
                            Port ipport,
                            Rank iprank,
                            int is_parent,
                            int is_internal);

Rank PeerNode_get_Rank(PeerNode_t* node);

int PeerNode_connect_DataSocket(PeerNode_t* parent);

int PeerNode_send(PeerNode_t* peer,  Packet_t* ipacket);

int PeerNode_sendDirectly(PeerNode_t* peer,  Packet_t* ipacket);

int PeerNode_has_data(PeerNode_t* node);

int PeerNode_recv(PeerNode_t* node, vector_t* opacket, bool_t blocking);

int PeerNode_flush(PeerNode_t* peer);

void PeerNode_mark_Failed(PeerNode_t* node);

#endif /* __peernode_h */
