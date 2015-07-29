/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/


#if !defined(__network_h)
#define __network_h 1

#include "mrnet_lightweight/Types.h"
#include "mrnet_lightweight/Packet.h"
#include "xplat_lightweight/Monitor.h"
#include "xplat_lightweight/SocketUtils.h"

#define UnknownPort InvalidPort
extern const Rank UnknownRank;

struct BackEndNode_t; 
struct NetworkTopology_t;
struct PeerNode_t;
struct SerialGraph_t;
struct Stream_t;
struct vector_t;
struct mrn_map_t;

struct Network_t {
    char* local_hostname;
    Port local_port;
    Rank local_rank;
    struct PeerNode_t* parent;
    struct vector_t* children;
    struct BackEndNode_t* local_back_end_node;
    struct NetworkTopology_t* network_topology;
    struct mrn_map_t* streams;
    unsigned int stream_iter;
    int recover_from_failures;
    char _was_shutdown;
    char thread_listening;
    char thread_recovering;
    Monitor_t* net_sync;
    Monitor_t* recv_sync;
    Monitor_t* parent_sync;
};
typedef struct Network_t Network_t;

/* Representation for monitors */
enum sync_index {
    NET_SYNC,
    RECV_SYNC,
    PARENT_SYNC
};

/* Condition for recv_sync */
enum cond_vars {
    THREAD_LISTENING,
    PARENT_NODE_AVAILABLE
};


/* BEGIN PUBLIC API */

Network_t* Network_CreateNetworkBE( int argc, char* argv[] );

void delete_Network_t( Network_t * net);

char* Network_get_LocalHostName( Network_t* net );
Port Network_get_LocalPort( Network_t* net );
Rank Network_get_LocalRank( Network_t* net );

struct NetworkTopology_t* Network_get_NetworkTopology( Network_t* net );

char Network_is_ShutDown( Network_t* net );
void Network_waitfor_ShutDown( Network_t* net );

int Network_recv( Network_t* net, int *otag,  
                  Packet_t* opacket, struct Stream_t** ostream );
int Network_recv_nonblock( Network_t* net, int *otag,  
                           Packet_t* opacket, struct Stream_t** ostream );

struct Stream_t* Network_get_Stream( Network_t* net, unsigned int iid );
struct Stream_t* Network_new_Stream( Network_t* net,
                                     unsigned int iid,
                                     Rank* ibackends,
                                     unsigned int inum_backends,
                                     int ius_filter_id,
                                     int isync_filter_id,
                                     int ids_filter_id );
void Network_delete_Stream(Network_t* net, unsigned int iid);

void Network_set_OutputLevel( int l );
void Network_set_DebugLogDir( char* value );

/* END PUBLIC API */

Network_t* new_Network_t();

Network_t* Network_init_BackEnd( char *iphostname, Port ipport,
                                 Rank iprank,
                                 char *imyhostname, Rank imyrank );
int Network_is_LocalNodeBackEnd( Network_t* net );

struct PeerNode_t* Network_get_ParentNode( Network_t* net );

int Network_has_PacketsFromParent( Network_t* net );
int Network_recv_internal( Network_t* net, struct Stream_t* stream, bool_t blocking );
Packet_t* Network_recv_stream_check(Network_t* net);

int Network_recv_EventFromParent(Network_t * net, struct vector_t* opackets, bool_t blocking);

int Network_recv_PacketsFromParent( Network_t* net, struct vector_t* opacket, bool_t blocking );
int Network_send_PacketToParent( Network_t* net,  Packet_t* ipacket );

void Network_shutdown_Network( Network_t* net );

int Network_reset_Topology( Network_t* net, char* itopology );
int Network_remove_Node( Network_t* net, Rank ifailed_rank, int iupdate );
int Network_change_Parent( Network_t* net, Rank ichild_rank, 
                           Rank inew_parent_rank );
int Network_add_SubGraph( Network_t * net, Rank iroot_rank, 
                          struct SerialGraph_t * sg, int iupdate );

struct PeerNode_t* Network_new_PeerNode( Network_t* network,
                                         char* ihostname,
                                         Port iport,
                                         Rank irank,
                                         int iis_parent,
                                         int iss_internal );
int Network_delete_PeerNode( Network_t* net, Rank irank );
void Network_set_ParentNode( Network_t* net, struct PeerNode_t* ip );

int Network_have_Streams( Network_t* net );
int Network_is_UserStreamId( unsigned int id );

int Network_recover_FromFailures( Network_t* net );
void Network_enable_FailureRecovery( Network_t* net );
void Network_disable_FailureRecovery( Network_t* net );
int Network_has_ParentFailure( Network_t* net );
int Network_recover_FromParentFailure( Network_t* net );
char* Network_get_LocalSubTreeStringPtr( Network_t* net );

#endif /* __network_h */
