/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__backendnode_h)
#define __backendnode_h 1

#include "utils_lightweight.h"
#include "PeerNode.h"
#include "mrnet_lightweight/Network.h"

/*  definition */
struct BackEndNode_t {
    Network_t* network;
    char* myhostname;
    Rank myrank;
    Port myport;
    char* phostname;
    Port pport;
    Rank prank;
    uint16_t incarnation;
};

typedef struct BackEndNode_t BackEndNode_t;

/* functions */

BackEndNode_t* new_BackEndNode_t( Network_t* inetwork,
                                  char* imyhostname,
                                  Rank imyrank,
                                  char* iphostname,
                                  Port ipport,
                                  Rank iprank);

BackEndNode_t* CreateBackEndNode (  Network_t* inetwork,
                                char* imy_hostname,
                                Rank imy_rank,
                                char* iphostname,
                                Port ipport,
                                Rank iprank);

void delete_BackEndNode_t( BackEndNode_t* ben );

int BackEndNode_proc_DeleteSubTree( BackEndNode_t* be,  Packet_t* packet);

int BackEndNode_proc_newStream( BackEndNode_t* be,  Packet_t* packet);

int BackEndNode_proc_UpstreamFilterParams( BackEndNode_t* be,  Packet_t* ipacket);

int BackEndNode_proc_DownstreamFilterParams( BackEndNode_t* be,  Packet_t* ipacket);

int BackEndNode_proc_newFilter( BackEndNode_t* be,  Packet_t* ipacket);

int BackEndNode_proc_DataFromParent( BackEndNode_t* be,  Packet_t* ipacket);

int BackEndNode_proc_deleteStream(BackEndNode_t * be, Packet_t * ipacket);

#endif /* __backendnode_h */
