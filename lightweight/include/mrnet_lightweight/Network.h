/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/


#if !defined(__network_h)
#define __network_h 1

#include "mrnet_lightweight/Packet.h"
#include "mrnet_lightweight/Types.h"

static const Port UnknownPort = (Port)-1;
static const Rank UnknownRank = (Rank)-1;

struct PeerNode_t;
struct BackEndNode_t; 
struct NetworkTopology_t ;
struct Stream_t ;
struct vector_t;
struct mrn_map_t;
struct SerialGraph_t;

typedef struct {
    char* local_hostname;
    Port local_port;
    Rank local_rank;
    struct PeerNode_t* parent;
    struct vector_t* children;
    struct BackEndNode_t* local_back_end_node;
    struct NetworkTopology_t* network_topology;
    struct mrn_map_t* streams;
    int stream_iter;
    int recover_from_failures;
    char _was_shutdown;
    unsigned int next_stream_id;
} Network_t;

Network_t* new_Network_t();

void delete_Network_t( Network_t * net);

char* Network_get_LocalHostName( Network_t* net );

Port Network_get_LocalPort( Network_t* net );

Rank Network_get_LocalRank( Network_t* net );

struct PeerNode_t* Network_get_ParentNode( Network_t* net );

struct NetworkTopology_t* Network_get_NetworkTopology( Network_t* net );

Network_t* Network_CreateNetworkBE( int argc, char* argv[] );

Network_t* Network_init_BackEnd( char *iphostname, Port ipport,
                                 Rank iprank,
                                 char *imyhostname, Rank imyrank );

int Network_recv_2( Network_t* net );

int Network_recv( Network_t* net, int *otag,  
                  Packet_t* opacket, struct Stream_t** ostream );

int Network_is_LocalNodeBackEnd( Network_t* net );

int Network_has_PacketsFromParent( Network_t* net );

int Network_recv_PacketsFromParent( Network_t* net, struct vector_t* opacket );

void Network_shutdown_Network( Network_t* net );

int Network_reset_Topology( Network_t* net, char* itopology );

struct Stream_t* Network_new_Stream( Network_t* net,
                                     int iid,
                                     Rank* ibackends,
                                     unsigned int inum_backends,
                                     int ius_filter_id,
                                     int isync_filter_id,
                                     int ids_filter_id );

int Network_remove_Node( Network_t* net, Rank ifailed_rank, int iupdate );

int Network_delete_PeerNode( Network_t* net, Rank irank );

int Network_change_Parent( Network_t* net, Rank ichild_rank, Rank inew_parent_rank );

int Network_have_Streams( Network_t* net );

struct Stream_t* Network_get_Stream( Network_t* net, unsigned int iid );

int Network_send_PacketToParent( Network_t* net,  Packet_t* ipacket );

struct PeerNode_t* Network_new_PeerNode( Network_t* network,
                                         char* ihostname,
                                         Port iport,
                                         Rank irank,
                                         int iis_parent,
                                         int iss_internal );

int Network_recover_FromFailures( Network_t* net );

void Network_enable_FailureRecovery( Network_t* net );

void Network_disable_FailureRecovery( Network_t* net );

int Network_has_ParentFailure( Network_t* net );

int Network_recover_FromParentFailure( Network_t* net );

char* Network_get_LocalSubTreeStringPtr( Network_t* net );

void Network_set_OutputLeveL( int l );

void Network_set_OutputLevelFromEnvironment(void);

char Network_is_ShutDown( Network_t* net );

void Network_waitfor_ShutDown( Network_t* net );

int Network_add_SubGraph(Network_t * net, Rank iroot_rank, struct SerialGraph_t * sg, int iupdate);

struct SerialGraph_t * Network_readTopology(Network_t * net, int topoSocket);

void Network_writeTopology(Network_t * net, int topoFd, struct SerialGraph_t * topology);

void Network_delete_Stream(Network_t * net, unsigned int iid);

#endif /* __network_h */
