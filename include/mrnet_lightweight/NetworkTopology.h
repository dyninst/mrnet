/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/


#if !defined(__network_topology_h)
#define __network_topology_h 1

#include "mrnet_lightweight/Network.h"

struct mrn_map_t;
struct node_t;
struct SerialGraph_t;

struct Node_t {
    char* hostname;
    Port port;
    Rank rank;
    int failed;
    struct vector_t* children; 
    struct vector_t* ascendants; 
    struct Node_t* parent;
    int is_backend;
    unsigned int depth;
    unsigned int subtree_height;
    double adoption_score;
    double rand_key;
    double wrs_key;
};

typedef struct Node_t Node_t;

struct NetworkTopology_t {
    Network_t* net;
    struct Node_t* root;
    unsigned int min_fanout;
    unsigned int max_fanout;
    unsigned int depth;
    double avg_fanout;
    double stddev_fanout;
    double var_fanout;
    struct mrn_map_t* nodes;
    struct vector_t* orphans;
    struct vector_t* backend_nodes;
    struct vector_t* parent_nodes;
    struct SerialGraph_t* serial_graph;
    struct Monitor_t* sync;
}; 

typedef struct NetworkTopology_t NetworkTopology_t;

typedef struct  {
    Node_t* local_node;
    NetworkTopology_t* topol;
} TopologyLocalInfo_t;

typedef enum {ALG_RANDOM=0,
              ALG_WRS,
              ALG_SORTED_RR
} ALGORITHM_T;

Node_t* new_Node_t(char* ihostname,
                   Port iport,
                   Rank irank,
                   int iis_backend);

NetworkTopology_t* new_NetworkTopology_t(Network_t* inetwork,
                                         char* ihostname,
                                         Port iport,
                                         Rank irank,
                                         int iis_backend);

void delete_NetworkTopology_t( NetworkTopology_t* net_top );

/* BEGIN PUBLIC API */

/* NetworkTopology */
unsigned int NetworkTopology_get_NumNodes(NetworkTopology_t* net_top);

Node_t* NetworkTopology_get_Root(NetworkTopology_t* net_top);

Node_t* NetworkTopology_find_Node(NetworkTopology_t* net_top, Rank irank);

/* NetworkTopology_Node */
char* NetworkTopology_Node_get_HostName(Node_t* node);

Port NetworkTopology_Node_get_Port(Node_t* node);

Rank NetworkTopology_Node_get_Rank(Node_t* node);

Rank NetworkTopology_Node_get_Parent(Node_t* node);

unsigned int NetworkTopology_Node_find_SubTreeHeight(Node_t* node);

/* END PUBLIC API */

char* NetworkTopology_get_TopologyStringPtr(NetworkTopology_t * net_top);

char* NetworkTopology_get_LocalSubTreeStringPtr( NetworkTopology_t* net_top); 

void NetworkTopology_serialize(NetworkTopology_t* net_top, Node_t* inode);

int NetworkTopology_reset(NetworkTopology_t* net_top, struct SerialGraph_t* isg);

int NetworkTopology_remove_Node_2(NetworkTopology_t* net_top,  Node_t* inode);

int NetworkTopology_remove_Node(NetworkTopology_t* net_top, Rank irank);

int NetworkTopology_set_Parent(NetworkTopology_t* net_top, Rank ichild_rank, 
                               Rank inew_Parent_rank, int iupdate);

int NetworkTopology_remove_Orphan(NetworkTopology_t* net_top, Rank r);

int NetworkTopology_add_SubGraph(NetworkTopology_t* net_top, Node_t* inode, 
                                 struct SerialGraph_t* isg, int iupdate);

void NetworkTopology_remove_SubGraph(NetworkTopology_t* net_top, Node_t* inode);

void NetworkTopology_print(NetworkTopology_t* net_top, FILE * f);

Node_t* NetworkTopology_find_NewParent(NetworkTopology_t* net_top,
                                       Rank ichild_rank,
                                       unsigned int inum_attempts,
                                       ALGORITHM_T ialgorithm);

void NetworkTopology_compute_AdoptionScores(NetworkTopology_t* net_top,
                                            struct vector_t* iadopters,
                                            Node_t* orphan);

void NetworkTopology_compute_TreeStatistics(NetworkTopology_t* net_top);

void NetworkTopology_find_PotentialAdopters(NetworkTopology_t* net_top,
                                            Node_t* iorphan,
                                            Node_t* ipotential_adopter,
                                            struct vector_t* potential_adopters);

int NetworkTopology_isInTopology(NetworkTopology_t * net_top, 
                                 char * hostname, 
                                 Port _port, 
                                 Rank _rank);

void NetworkTopology_add_BackEnd(NetworkTopology_t * net_top, 
                                 Rank rprank, Rank rcrank, 
                                 char * rchost, Port rcport);

void NetworkTopology_add_InternalNode(NetworkTopology_t * net_top, 
                                      Rank rprank, Rank rcrank, 
                                      char * rchost, Port rcport);

void NetworkTopology_change_Port(NetworkTopology_t * net_top,
                                 Rank rcrank, Port rcport);

Node_t* NetworkTopology_new_Node(NetworkTopology_t * net_top, char * host, 
                                 Port port, Rank rank, int iis_backend);

void NetworkTopology_update_TopoStreamPeers(NetworkTopology_t * net_top, 
                                            struct vector_t * new_nodes);

/* NetworkTopology_Node */
int NetworkTopology_Node_is_BackEnd(Node_t* node);

void NetworkTopology_Node_set_Parent(Node_t* node, Node_t* parent);

unsigned int NetworkTopology_Node_get_Proximity(Node_t* adopter, Node_t* iorphan);

unsigned int NetworkTopology_Node_get_DepthIncrease(Node_t* adopter, Node_t* iorphan);

void NetworkTopology_Node_compute_AdoptionScore(NetworkTopology_t* net_top,
                                                Node_t* adopter,
                                                Node_t* orphan,
                                                unsigned int imin_fanout,
                                                unsigned int imax_fanout,
                                                unsigned int idepth);

void NetworkTopology_Node_add_Child(Node_t* parent, Node_t* child);

int NetworkTopology_Node_remove_Child(Node_t* parent, Node_t* child);

void NetworkTopology_Node_set_Port(Node_t * node, Port port);


/* TopologyLocalInfo */
TopologyLocalInfo_t* new_TopologyLocalInfo_t(NetworkTopology_t* topol, Node_t* node);

Network_t * TopologyLocalInfo_get_Network(TopologyLocalInfo_t* tli);

#endif /* __network_topology_h */
