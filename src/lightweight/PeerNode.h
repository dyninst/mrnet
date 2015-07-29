/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__peernode_h)
#define __peernode_h 1

#include "utils_lightweight.h"
#include "Message.h"

#include "mrnet_lightweight/Network.h"
#include "xplat_lightweight/Mutex.h"
#include "xplat_lightweight/vector.h"

struct PeerNode_t { 
  Network_t* net;
  char* hostname;
  Port port;
  Rank rank;
  XPlat_Socket data_sock_fd;
  XPlat_Socket event_sock_fd;
  int is_internal_node;
  int is_parent;

  int available;

  Message_t msg_out;
  Message_t msg_in;
#ifdef MRNET_LTWT_THREADSAFE
  Mutex_t* send_mutex;
  Monitor_t* recv_monitor;
#endif
};

typedef struct PeerNode_t PeerNode_t;

 PeerNode_t* new_PeerNode_t(Network_t* inetwork,
                            char* iphostname,
                            Port ipport,
                            Rank iprank,
                            int is_parent,
                            int is_internal);

Rank PeerNode_get_Rank(PeerNode_t* node);

int PeerNode_connect_DataSocket(PeerNode_t* parent, int num_retry);
int PeerNode_connect_EventSocket(PeerNode_t* parent, int num_retry);

int PeerNode_send(PeerNode_t* peer,  Packet_t* ipacket);

int PeerNode_sendDirectly(PeerNode_t* peer,  Packet_t* ipacket);

int PeerNode_has_data(PeerNode_t* node);

int PeerNode_has_event_data(PeerNode_t * node);
int PeerNode_recv_event(PeerNode_t* node, vector_t* packet_list, bool_t blocking);

int PeerNode_recv(PeerNode_t* node, vector_t* opacket, bool_t blocking);

int PeerNode_flush(PeerNode_t* peer);

void PeerNode_mark_Failed(PeerNode_t* node);

#endif /* __peernode_h */
