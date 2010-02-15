/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "mrnet/NetworkTopology.h"
#include "Router.h"
#include "mrnet/Types.h"
#include "utils.h"
#include "map.h"
#include "vector.h"
#include "SerialGraph.h"

#include "xplat/NetUtils.h"

Node_t* new_Node_t(char* ihostname, 
                  Port iport, 
                  Rank irank, 
                  int iis_backend)
{

  static int first_time = true;
  
  Node_t* node = (Node_t*)malloc(sizeof(Node_t));
  assert(node);
  node->hostname = ihostname;
  node->port = iport;
  node->rank = irank;
  node->failed = false;
  node->children = new_empty_vector_t();
  node->ascendants = new_empty_vector_t();
  node->parent = NULL;
  node->is_backend = iis_backend;
  node->depth = 0;
  node->subtree_height = 0;
  node->adoption_score = 0;

  
  if (first_time) {
    first_time = false;
    srand48(time(NULL));
  }
  node->rand_key = drand48();

  return node;
}

NetworkTopology_t* new_NetworkTopology_t(Network_t* inetwork, 
                                          char* ihostname, 
                                          Port iport, 
                                          Rank irank, 
                                          int iis_backend)
{ 
  NetworkTopology_t* net_top = (NetworkTopology_t*)malloc(sizeof(NetworkTopology_t));
  assert(net_top);
  net_top->net = inetwork;
  net_top->root = new_Node_t(ihostname, iport, irank, iis_backend);
  net_top->router = new_Router_t(inetwork);
  net_top->orphans = new_empty_vector_t();
  net_top->backend_nodes = new_empty_vector_t();
  net_top->parent_nodes = new_empty_vector_t();
  net_top->serial_graph = new_SerialGraph_t(NULL);

  mrn_dbg(5, mrn_printf(FLF,stderr, "About to insert node into net_top->nodes, with rank=%d\n", irank));
  net_top->nodes = new_map_t();
  insert(net_top->nodes, irank ,net_top->root);

  return net_top;
}


unsigned int NetworkTopology_get_NumNodes(NetworkTopology_t* net_top) {
    return net_top->nodes->size;
}

Node_t* NetworkTopology_get_Root(NetworkTopology_t* net_top) {
    return net_top->root; 
}

void NetworkTopology_get_BackEndNodes(NetworkTopology_t* net_top, 
                                    vector_t* nodes)
{
    nodes = net_top->backend_nodes;
}

void NetworkTopology_get_ParentNodes(NetworkTopology_t* net_top, 
                                    vector_t* nodes)
{
    nodes = net_top->parent_nodes;
}

void NetworkTopology_get_OrphanNodes(NetworkTopology_t* net_top, 
                                    vector_t* nodes)
{
    nodes = net_top->orphans;
}

char* NetworkTopology_get_LocalSubTreeStringPtr(NetworkTopology_t* net_top)
{ 
  char* localhost;
  SerialGraph_t* my_subgraph;
  char* retval;
    
  mrn_dbg_func_begin();

  if (net_top->serial_graph != NULL)
    free(net_top->serial_graph);
  net_top->serial_graph = new_SerialGraph_t(NULL);
  
  NetworkTopology_serialize (net_top, net_top->root);

  localhost = Network_get_LocalHostName(net_top->net);
  my_subgraph = 
    SerialGraph_get_MySubTree(net_top->serial_graph,
                              localhost,
                              Network_get_LocalPort(net_top->net),
                              Network_get_LocalRank(net_top->net));
  
  retval = strdup(my_subgraph->byte_array);

  mrn_dbg(5, mrn_printf(FLF, stderr, "returning '%s'\n", retval));

  mrn_dbg_func_end();

  return retval;
}

void NetworkTopology_serialize(NetworkTopology_t* net_top, Node_t* inode)
{
  int iter;
  
  if (inode->is_backend) {
    // leaf node, just add my name to serial representation and return
    SerialGraph_add_Leaf(net_top->serial_graph, inode->hostname, inode->port, inode->rank);
    return;
  }
  else {
    // starting new sub-tree component in graph serialization
    SerialGraph_add_SubTreeRoot(net_top->serial_graph, inode->hostname, inode->port, inode->rank);
  }

  for (iter = 0; iter < inode->children->size; iter++) {
    NetworkTopology_serialize(net_top, (Node_t*)(inode->children->vec[iter]));
  }

  // ending sub-tree component in graph serialization:
  SerialGraph_end_SubTree(net_top->serial_graph);

}

int NetworkTopology_reset(NetworkTopology_t* net_top, char* itopology_str)
{
  SerialGraph_t* cur_sg;
    
  mrn_dbg(5, mrn_printf(FLF, stderr, "Reseting topology to \"%s\"\n", itopology_str));

  if (net_top->serial_graph != NULL) {
    free(net_top->serial_graph);
  }
  mrn_dbg(5, mrn_printf(FLF, stderr, "About to call new_SerialGraph_t\n"));
  net_top->serial_graph = new_SerialGraph_t(itopology_str);
  net_top->root = NULL;

  clear_map_t(net_top->nodes);
  clear(net_top->orphans);
  clear(net_top->backend_nodes);

  if (!strcmp(itopology_str, "")) {
    return true;
  }

  SerialGraph_set_ToFirstChild(net_top->serial_graph); 
 
  mrn_dbg(5, mrn_printf(FLF, stderr, "Root: %s:%d:%d\n",
                        SerialGraph_get_RootHostName(net_top->serial_graph),
                        SerialGraph_get_RootRank(net_top->serial_graph),
                        SerialGraph_get_RootPort(net_top->serial_graph)));
  net_top->root = 
      new_Node_t(SerialGraph_get_RootHostName(net_top->serial_graph),
                 SerialGraph_get_RootPort(net_top->serial_graph),
                 SerialGraph_get_RootRank(net_top->serial_graph),
                 SerialGraph_is_RootBackEnd(net_top->serial_graph));
;
  insert(net_top->nodes, net_top->root->rank, net_top->root);

  cur_sg = (SerialGraph_t*)malloc(sizeof(SerialGraph_t));
  assert(cur_sg);
  for (cur_sg = SerialGraph_get_NextChild(net_top->serial_graph);
        cur_sg;
        cur_sg = SerialGraph_get_NextChild(net_top->serial_graph)) {
      mrn_dbg(5, mrn_printf(FLF, stderr, "adding subgraph\n"));
      if (!NetworkTopology_add_SubGraph(net_top, net_top->root, cur_sg, false)) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "add_SubTreeRoot() failed\n"));
        return false;
    }
  }

  free(cur_sg);
  return true;

}

int NetworkTopology_add_SubGraph(NetworkTopology_t* net_top, Node_t* inode, SerialGraph_t* isg, int iupdate) 
{
    Node_t* node;
    Node_t* new_node;
    char* name;
    Rank r;
    SerialGraph_t* cur_sg;
    
    mrn_dbg_func_begin();
    mrn_dbg(5, mrn_printf(FLF, stderr, "Node[%d] adding subgraph \"%s\"\n",
                        inode->rank, isg->byte_array));

    if (!findElement(net_top->parent_nodes, inode))
        pushBackElement(net_top->parent_nodes, inode);

    // search of root of subgraph
    node = (Node_t*)malloc(sizeof(Node_t));
    assert(node);
    node = NetworkTopology_find_Node(net_top, SerialGraph_get_RootRank(isg));
    if (node == NULL) {
        // Not found! allocate
        mrn_dbg(5, mrn_printf(FLF, stderr, "node==NULL, about to allocate\n"));
        node = new_Node_t(SerialGraph_get_RootHostName(isg),
                            SerialGraph_get_RootPort(isg),
                            SerialGraph_get_RootRank(isg),
                            SerialGraph_is_RootBackEnd(isg));
        new_node = (Node_t*)malloc(sizeof(Node_t));
        assert(node);
        *new_node = *node;
        insert(net_top->nodes, node->rank, new_node);
    } else {
        // Found! Remove old subgraph
        mrn_dbg(5, mrn_printf(FLF, stderr, "node!=null, calling remove_SubGraph\n"));
        NetworkTopology_remove_SubGraph(net_top, node);
    }

    if (NetworkTopology_Node_is_BackEnd(node)) {
        name = node->hostname;
        r = node->rank;
        if (!findElement(net_top->backend_nodes, node)) {
            mrn_dbg(5, mrn_printf(FLF, stderr, "Adding node[%d] as backend\n", r));
            pushBackElement(net_top->backend_nodes, node);
        }
    }

    mrn_dbg(5, mrn_printf(FLF, stderr, "Adding node[%d] as child of node[%d]\n", node->rank, inode->rank));
    // add root of new subgraph as child of input node
    NetworkTopology_set_Parent(net_top, node->rank, inode->rank, iupdate);

    cur_sg = (SerialGraph_t*)malloc(sizeof(SerialGraph_t));
    assert(cur_sg);
    SerialGraph_set_ToFirstChild(isg);
    for (cur_sg = SerialGraph_get_NextChild(isg);
        cur_sg;
        cur_sg = SerialGraph_get_NextChild(isg)) {
        NetworkTopology_add_SubGraph(net_top, node, cur_sg, iupdate);
    }

    mrn_dbg_func_end();
    return 1;
}


int NetworkTopology_remove_Node_2(NetworkTopology_t* net_top, Node_t* inode)
{
  Node_t* node_to_delete;
  int i;
  Node_t* cur_node;

  mrn_dbg_func_begin();

  if (net_top->root == inode)
    net_top->root = NULL;

  // remove from ophans, back-ends, parents list
  net_top->nodes = erase(net_top->nodes, inode->rank);
  net_top->orphans = eraseElement(net_top->orphans, inode);

  if (inode->is_backend) {
    net_top->backend_nodes = eraseElement(net_top->backend_nodes, inode);
  } else {
    // find appropriate node to delete (necessary b/c of how they are stored)
    node_to_delete = (Node_t*)malloc(sizeof(Node_t));
    assert(node_to_delete);
    for (i = 0; i < net_top->parent_nodes->size; i++){
        cur_node = (Node_t*)net_top->parent_nodes->vec[i];
        if (cur_node->rank == inode->rank) {
            node_to_delete = cur_node;
            break;
        }
    }
    net_top->parent_nodes = eraseElement(net_top->parent_nodes, node_to_delete);
  }

  // remove me as my children's parent, and set children as oprhans
  for (i = 0; i < inode->children->size; i++) {
    cur_node = (Node_t*)(inode->children->vec[i]);
    if (cur_node->parent == inode) {
        NetworkTopology_Node_set_Parent(cur_node, NULL);
        pushBackElement(net_top->orphans, cur_node);
    }
  }
  
  free(inode);
  
  mrn_dbg_func_end();
  return true;
}

int NetworkTopology_remove_Node(NetworkTopology_t* net_top, Rank irank)
{
    Node_t* node_to_remove;
    int retval;
    
    mrn_dbg_func_begin();
    
    node_to_remove = NetworkTopology_find_Node(net_top, irank);

    if (node_to_remove == NULL) {
        return false;
    }
    
    node_to_remove->failed = true;

    // remove node as parent's child
    if (node_to_remove->parent) {
        NetworkTopology_Node_remove_Child(node_to_remove->parent, node_to_remove);
    }

    retval = NetworkTopology_remove_Node_2(net_top, node_to_remove);

    mrn_dbg_func_end();

    return retval;
}

TopologyLocalInfo_t* new_TopologyLocalInfo_t(NetworkTopology_t* topol, Node_t* node)
{
  TopologyLocalInfo_t* new_top = (TopologyLocalInfo_t*)malloc(sizeof(TopologyLocalInfo_t));
  assert(new_top);
  new_top->topol = topol;
  new_top->local_node = node;

  return new_top;
}

Node_t* NetworkTopology_find_Node(NetworkTopology_t* net_top, Rank irank)
{
    Node_t* ret;
    
    mrn_dbg_func_begin();
    
    ret = (Node_t*)(get_val(net_top->nodes, irank));

    mrn_dbg_func_end();

    return ret;
}

int NetworkTopology_Node_is_BackEnd(Node_t* node)
{
    return node->is_backend;
}

void NetworkTopology_remove_SubGraph(NetworkTopology_t* net_top, Node_t* inode)
{
    int iter;
    
    mrn_dbg_func_begin();

    for (iter = 0; iter < inode->children->size; iter++) {
        Node_t* cur_node = (Node_t*)inode->children->vec[iter];
        NetworkTopology_remove_SubGraph(net_top, cur_node);
        NetworkTopology_remove_Node_2(net_top, cur_node);
    }

    clear(inode->children);

    mrn_dbg_func_end(); 
}

int NetworkTopology_set_Parent(NetworkTopology_t* net_top, Rank ichild_rank, Rank inew_parent_rank, int iupdate)
{
    Node_t* child_node;
    Node_t* new_parent_node;
    int j;
    
    mrn_dbg_func_begin();

    child_node = NetworkTopology_find_Node(net_top, ichild_rank);

    if (child_node == NULL) {
        mrn_dbg_func_end();
        return 0;
    }

    new_parent_node = NetworkTopology_find_Node(net_top, inew_parent_rank);

    if (new_parent_node == NULL) {
        mrn_dbg_func_end();
        return false;
    }

    if (child_node->parent != NULL) {
        NetworkTopology_Node_remove_Child(child_node->parent, child_node);
    }

    NetworkTopology_Node_set_Parent(child_node, new_parent_node);
    NetworkTopology_Node_add_Child(new_parent_node, child_node);
    NetworkTopology_remove_Orphan(net_top, child_node->rank); 
    
    //child_node->ascendants = new_parent_node->ascendants;
    for (j = 0; j < new_parent_node->ascendants->size; j++)
        pushBackElement(child_node->ascendants, new_parent_node->ascendants->vec[j]);
    if (!findElement(child_node->ascendants, new_parent_node))
        pushBackElement(child_node->ascendants, new_parent_node);
    

    mrn_dbg_func_end();
    return 1;
}

int NetworkTopology_remove_Orphan(NetworkTopology_t* net_top, Rank r) 
{
    // find the node associated with r
    Node_t* node = (Node_t*)(get_val(net_top->nodes, r));

    if (!node)
        return false;

    net_top->orphans = eraseElement(net_top->orphans, node);

    return true;

}

void NetworkTopology_print(NetworkTopology_t* net_top, FILE * f) 
{
    char* cur_parent_str;
    char* cur_child_str;
    char rank_str[128];
    int node_iter;
    Node_t* cur_node;
    int set_iter;
    
    cur_parent_str = (char*)malloc(sizeof(char)*256);
    assert(cur_parent_str);
    cur_child_str = (char*)malloc(sizeof(char)*256);
    assert(cur_child_str);

    mrn_dbg(5, mrn_printf(0,0,0, f, "\n***NetworkTopology***\n"));

    for (node_iter = 0; node_iter < net_top->nodes->size; node_iter++) {
        cur_node = (Node_t*)(get_val(net_top->nodes, net_top->nodes->keys[node_iter]));

        // Only print on lhs if root or have children
        if (cur_node == net_top->root || cur_node->children->size) {
            sprintf(rank_str, "%u", cur_node->rank);
            strncpy(cur_parent_str, cur_node->hostname, strlen(cur_node->hostname)+1);
            strcat(cur_parent_str, ":");
            strcat(cur_parent_str, rank_str); 
            mrn_dbg(5, mrn_printf(0,0,0, f, "%s =>\n", cur_parent_str));
            
            for (set_iter = 0; set_iter < cur_node->children->size; set_iter++){
                Node_t* cur_child = (Node_t*)(cur_node->children->vec[set_iter]);
                sprintf(rank_str, "%u", cur_child->rank);
                strncpy(cur_child_str, cur_child->hostname, strlen(cur_child->hostname)+1);
                strcat(cur_child_str, ":");
                strcat(cur_child_str, rank_str); 
                mrn_dbg(5, mrn_printf(0,0,0, f, "\t%s\n", cur_child_str));
            }// end iteration over children
            mrn_dbg(5, mrn_printf(0,0,0, f, "\n"));
        }
    }// end iterate over network topology nodes
}

Node_t* NetworkTopology_find_NewParent(NetworkTopology_t* net_top, 
                                        Rank ichild_rank,
                                        unsigned int inum_attempts,
                                        ALGORITHM_T ialgorithm)
{
    vector_t* potential_adopters = new_empty_vector_t(); // vec of Node_t*
    Node_t* adopter;
    Node_t* orphan;
    int i, j;
    Node_t* cur;
    Node_t* cur_node;

    adopter = (Node_t*)malloc(sizeof(Node_t*));
    assert(adopter);

    mrn_dbg_func_begin();

    mrn_dbg(5, mrn_printf(FLF, stderr, "called with ichild_rank=%d\n", ichild_rank));

    if (inum_attempts > 0) {
        // previously computed list, but failed to contact new parent
        if (inum_attempts >= potential_adopters->size) {
            return NULL;
        }      

        adopter = (Node_t*)(potential_adopters->vec[inum_attempts]);

        mrn_dbg(5, mrn_printf(FLF, stderr, "Returning: node[%d]:%s:%s\n",
                            NetworkTopology_Node_get_Rank(adopter),
                            NetworkTopology_Node_get_HostName(adopter),
                            NetworkTopology_Node_get_Port(adopter)));

        return adopter;
    }

    orphan = NetworkTopology_find_Node(net_top, ichild_rank);
    assert(orphan);

    // computer list of potential adopters
    clear(potential_adopters);  
    NetworkTopology_find_PotentialAdopters(net_top, orphan, net_top->root, potential_adopters);
    if (potential_adopters->size == 0){
        mrn_dbg(5, mrn_printf(FLF,stderr, "No adopters left :(\n"));
        Network_shutdown_Network(net_top->net);
        exit(-1);
    }

    if (ialgorithm == ALG_RANDOM) {
        assert(!"STUB");
    }

    else if (ialgorithm == ALG_WRS) {
        // use weighted random selection to choose a parent
        NetworkTopology_compute_AdoptionScores(net_top, potential_adopters, orphan);

        for (j = 0; j < potential_adopters->size; j++) {
            cur = (Node_t*)(potential_adopters->vec[j]);
            mrn_dbg(5, mrn_printf(FLF, stderr, "adopter[%d] is [%s:%d], adoption_wrs_key=%f\n", j, cur->hostname, cur->rank, cur->wrs_key)); 
        }   
        adopter = (Node_t*)(potential_adopters->vec[0]);
        cur_node = (Node_t*)malloc(sizeof(Node_t));
        assert(cur_node);
        for (i = 1; i < potential_adopters->size; i++) {
            cur_node = (Node_t*)(potential_adopters->vec[i]);
            if (fabs(adopter->wrs_key - cur_node->wrs_key) < .000001 ) {
                if (!(adopter->rank > cur_node->rank))
                    adopter = cur_node;
            }
            else {
                if (!(adopter->wrs_key > cur_node->wrs_key))
                    adopter = cur_node;
            }
        }
    }
    else if (ialgorithm == ALG_SORTED_RR) {
        assert(!"STUB");
    }
    
    if (adopter) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "Returning: node[%d]:%s:%d\n",
                            NetworkTopology_Node_get_Rank(adopter),
                            NetworkTopology_Node_get_HostName(adopter),
                            NetworkTopology_Node_get_Port(adopter)));
    } else {
        mrn_dbg(5, mrn_printf(FLF, stderr, "No adopter found ... \n"));
    }

    mrn_dbg_func_end();

    return adopter;
}

void NetworkTopology_find_PotentialAdopters(NetworkTopology_t* net_top,
                                            Node_t* iorphan,
                                            Node_t* ipotential_adopter,
                                            vector_t* potential_adopters)
{
    int i;
    
    mrn_dbg_func_begin();

    mrn_dbg(5, mrn_printf(FLF, stderr, 
                "Can Node[%d]:%s:%d (%s) adopt Orphan{%d]:%s :%d ...",
                NetworkTopology_Node_get_Rank(ipotential_adopter),
                NetworkTopology_Node_get_HostName(ipotential_adopter),
                NetworkTopology_Node_get_Port(ipotential_adopter),
                ( NetworkTopology_Node_is_BackEnd(ipotential_adopter) ? "be" : "int"),
                NetworkTopology_Node_get_Rank(iorphan),
                NetworkTopology_Node_get_HostName(iorphan),
                NetworkTopology_Node_get_Port(iorphan)));

    if ( (iorphan->parent == ipotential_adopter) ||
        NetworkTopology_Node_is_BackEnd(ipotential_adopter)) {
        mrn_dbg(5, mrn_printf(0,0,0, stderr, "no!\n"));
        return;
    }

    mrn_dbg(5, mrn_printf(0,0,0, stderr, "yes!\n"));

    pushBackElement(potential_adopters, ipotential_adopter);

    for (i = 0; i < ipotential_adopter->children->size; i++) {
        NetworkTopology_find_PotentialAdopters(net_top, iorphan, ipotential_adopter->children->vec[i], potential_adopters);
    }
    
    mrn_dbg_func_end();
}

void NetworkTopology_compute_AdoptionScores(NetworkTopology_t* net_top,
                                            vector_t* iadopters,
                                            Node_t* orphan)
{
    unsigned int i;
    
    NetworkTopology_compute_TreeStatistics(net_top);

    for (i = 0; i < iadopters->size; i++) {
        NetworkTopology_Node_compute_AdoptionScore(net_top, (Node_t*)(iadopters->vec[i]), orphan, net_top->min_fanout, net_top->max_fanout, net_top->depth); 
    } 
}




void NetworkTopology_compute_TreeStatistics(NetworkTopology_t* net_top)
{
    int i;
    double diff = 0, sum_of_square = 0;

    net_top->max_fanout = 0;
    net_top->depth = 0;
    net_top->min_fanout = (unsigned int)-1;

    net_top->depth = NetworkTopology_Node_find_SubTreeHeight(net_top->root);

    for (i = 0; i < net_top->parent_nodes->size; i++) {
        Node_t* cur_node = (Node_t*)(net_top->parent_nodes->vec[i]);
        net_top->max_fanout = (cur_node->children->size > net_top->max_fanout ?
                cur_node->children->size : net_top->max_fanout);
        net_top->min_fanout = (cur_node->children->size < net_top->min_fanout ?
                cur_node->children->size : net_top->min_fanout);
    }

    net_top->avg_fanout = ((double)(net_top->nodes->size - 1) / ((double)net_top->parent_nodes->size));

    for (i = 0; i < net_top->parent_nodes->size; i++) {
        Node_t* cur_node = (Node_t*)(net_top->parent_nodes->vec[i]);
        diff = net_top->avg_fanout - cur_node->children->size;
        sum_of_square += (diff*diff); 
    }

    net_top->var_fanout = sum_of_square/net_top->parent_nodes->size;
    net_top->stddev_fanout = sqrt(net_top->var_fanout);

    mrn_dbg(5, mrn_printf(FLF, stderr, "min_fanout=%d, max_fanout=%d, depth=%d\n", net_top->min_fanout, net_top->max_fanout, net_top->depth));

}


char* NetworkTopology_Node_get_HostName(Node_t* node) {
    return node->hostname;
}

Port NetworkTopology_Node_get_Port(Node_t* node) {
    return node->port;
}

Rank NetworkTopology_Node_get_Rank(Node_t* node) {
    return node->rank;
}

Rank NetworkTopology_Node_get_Parent(Node_t* node) {
    return NetworkTopology_Node_get_Rank(node->parent);
}

void NetworkTopology_Node_set_Parent(Node_t* node, Node_t* parent)
{
    node->parent = parent;
}

unsigned int NetworkTopology_Node_find_SubTreeHeight(Node_t* node)
{
    unsigned int max_height = 0, cur_height;
    int i;
    
    if (node->children->size == 0)
        node->subtree_height = 0;
    
    else {
        for (i = 0; i < node->children->size; i++) {
            Node_t* cur_node = (Node_t*)(node->children->vec[i]);
            cur_height = NetworkTopology_Node_find_SubTreeHeight(cur_node);
            max_height = (cur_height > max_height ? cur_height : max_height);
        }

        node->subtree_height = max_height + 1;
    }
    return node->subtree_height;
}

unsigned int NetworkTopology_Node_get_Proximity(Node_t* adopter,
                                                Node_t* iorphan)
{
    // Find closest common ascendant to both current node and orphan
    Node_t* cur_ascendant = adopter;
    Node_t* common_ascendant = NULL;
    unsigned int node_ascendant_distance = 0;
    unsigned int orphan_ascendant_distance = 0;
    
    mrn_dbg(5, mrn_printf(FLF, stderr,
                    "Computing proximity: [%s:%d] <-> [%s:%d] ...\n",
                    iorphan->hostname, iorphan->rank,
                    adopter->hostname, adopter->rank));


    do {
        mrn_dbg(5, mrn_printf(FLF, stderr, 
                            "Is \"%s:%d\" a common ascendant? ",
                            cur_ascendant->hostname, 
                            cur_ascendant->rank));
        if (findElement(iorphan->ascendants, cur_ascendant)) {
            common_ascendant = cur_ascendant;
            mrn_dbg(5, mrn_printf(0,0,0, stderr, "yes!\n"));
            break;
        }
        mrn_dbg(5, mrn_printf(0,0,0, stderr, "no.\n"));
        cur_ascendant = cur_ascendant->parent;
        node_ascendant_distance++;
    } while(common_ascendant == NULL);;
    
    // Find distance between orphan and common ascendant
    cur_ascendant = iorphan;
    mrn_dbg(5, mrn_printf(FLF, stderr, "Ascend from orphan to common ascendant [%s:%d]:\n"
                "\t[%s:%d]",
                common_ascendant->hostname,
                common_ascendant->rank,
                cur_ascendant->hostname,
                cur_ascendant->rank));

    while( cur_ascendant != common_ascendant) {
        cur_ascendant = cur_ascendant->parent;
        mrn_dbg(5, mrn_printf(0,0,0, stderr, " <- [%s:%d]",
                    cur_ascendant->hostname,
                    cur_ascendant->rank));
        orphan_ascendant_distance++;
    }

    mrn_dbg(5, mrn_printf(0,0,0, stderr, "\n\n"));
    mrn_dbg(5, mrn_printf(FLF,stderr, 
                "\t[%s:%d]<->[%s:%d]: %d\n\t[%s:%d]<->[%s:%d]: %d\n", 
                common_ascendant->hostname,
                common_ascendant->rank,
                adopter->hostname,
                adopter->rank,
                node_ascendant_distance,
                common_ascendant->hostname, 
                common_ascendant->rank,
                iorphan->hostname, 
                iorphan->rank,
                orphan_ascendant_distance));

    return node_ascendant_distance + orphan_ascendant_distance;

}

unsigned int NetworkTopology_Node_get_DepthIncrease(Node_t* adopter, 
                                                    Node_t* iorphan)
{
    unsigned int depth_increase = 0;

    if (iorphan->subtree_height + 1 > adopter->subtree_height) {
        depth_increase = iorphan->subtree_height + 1 - adopter->subtree_height;
    }

    mrn_dbg(5, mrn_printf(FLF, stderr, 
                "\n\tOrphan[%s:%d] sub tree height: %d\n"
                "\tNode:[%s:%d] sub tree height: %d.\n"
                "\tDepth Increase: %u\n",
                iorphan->hostname, iorphan->rank,
                iorphan->subtree_height,
                adopter->hostname, adopter->rank, adopter->subtree_height,
                depth_increase));

    return depth_increase;
}

const float WFANOUT=1.0;
const float WDEPTH=1.0;
const float WPROXIMITY=0.5;
void NetworkTopology_Node_compute_AdoptionScore(NetworkTopology_t* net_top,
                                            Node_t* adopter,
                                                Node_t* orphan,
                                                unsigned int imin_fanout,
                                                unsigned int imax_fanout,
                                                unsigned int idepth)
{
    unsigned int depth_increase = NetworkTopology_Node_get_DepthIncrease(adopter, orphan); 
    unsigned int proximity = NetworkTopology_Node_get_Proximity(adopter, orphan); 
    unsigned int fanout = adopter->children->size;

    double fanout_score;
    double depth_increase_score;
    double proximity_score;

    mrn_dbg(5, mrn_printf(FLF, stderr,
                            "Computing [%s:%d]'s score for adopting [%s:%d] ... \n", 
                            adopter->hostname,
                            adopter->rank,
                            orphan->hostname,
                            orphan->rank));


    if (imax_fanout == imin_fanout)
        fanout_score = 1;
    else 
        fanout_score = (double)(imax_fanout - fanout) /
                (double)(imax_fanout - imin_fanout);


    depth_increase_score = (double)((idepth-1) - depth_increase )/
        (double)(idepth-1);


    proximity_score = (double)(2 * idepth - 1 - proximity ) /
        (double)(2 * idepth - 1 - 2);

    
    adopter->adoption_score = (WFANOUT * fanout_score) + 
        (WDEPTH * depth_increase_score) + 
        (WPROXIMITY * proximity_score);

    adopter->wrs_key = pow(drand48(), 1/adopter->adoption_score);

}

void NetworkTopology_Node_add_Child(Node_t* parent, Node_t* child)
{
    pushBackElement(parent->children, (void*)child);
}

int NetworkTopology_Node_remove_Child(Node_t* parent, Node_t* child)
{
    mrn_dbg_func_begin();
    parent->children = eraseElement(parent->children, (void*)child);
    mrn_dbg_func_end();

    return true;
}
