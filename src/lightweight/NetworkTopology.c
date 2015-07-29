/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <math.h>
#include <time.h>

#include "utils_lightweight.h"
#include "SerialGraph.h"

#include "mrnet_lightweight/NetworkTopology.h"
#include "mrnet_lightweight/Stream.h"
#include "xplat_lightweight/NetUtils.h"
#include "xplat_lightweight/map.h"
#include "xplat_lightweight/vector.h"

#ifdef MRNET_LTWT_THREADSAFE  

void _NetworkTopology_lock(NetworkTopology_t* net_top)
{
    int retval;
    mrn_dbg(3, mrn_printf(FLF, stderr, "NetworkTopology_lock\n"));
    retval = Monitor_Lock( net_top->sync );
    if( retval != 0 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "Network_lock failed to acquire sync: %s\n",
                              strerror(retval)));
    }
}

void _NetworkTopology_unlock(NetworkTopology_t* net_top)
{
    int retval;
    mrn_dbg(3, mrn_printf(FLF, stderr, "NetworkTopology_unlock\n"));
    retval = Monitor_Unlock( net_top->sync );
    if( retval != 0 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "Network_unlock failed to release sync: %s\n",
                              strerror(retval)));
    }
}

# define NetworkTopology_lock(nt) _NetworkTopology_lock(nt)
# define NetworkTopology_unlock(nt) _NetworkTopology_unlock(nt)

#else // !def MRNET_LTWT_THREADSAFE  
# define NetworkTopology_lock(nt) do{}while(0)
# define NetworkTopology_unlock(nt) do{}while(0)
#endif

Node_t* new_Node_t(char* ihostname, 
                   Port iport, 
                   Rank irank, 
                   int iis_backend)
{
    static int first_time = true;
  
    Node_t* node = (Node_t*) calloc( (size_t)1, sizeof(Node_t) );
    assert( node != NULL );
    node->hostname = strdup( ihostname );
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

  
    if( first_time ) {
        first_time = false;
        srand48( time(NULL) );
    }
    node->rand_key = drand48();

    return node;
}

void delete_Node_t( Node_t* node )
{
    if( node != NULL ) {

        if( node->hostname != NULL )
            free( node->hostname );

        delete_vector_t( node->children );
        delete_vector_t( node->ascendants );        

        free( node );
    }
}

NetworkTopology_t* new_NetworkTopology_t(Network_t* inetwork, 
                                         char* ihostname, 
                                         Port iport, 
                                         Rank irank, 
                                         int is_backend)
{ 
    NetworkTopology_t* net_top = (NetworkTopology_t*) calloc( (size_t)1, 
                                                              sizeof(NetworkTopology_t) );
    assert( net_top != NULL );
    net_top->net = inetwork;
    net_top->backend_nodes = new_empty_vector_t();
    net_top->orphans = new_empty_vector_t();
    net_top->parent_nodes = new_empty_vector_t();
    net_top->serial_graph = new_SerialGraph_t(NULL);
    net_top->nodes = new_map_t();

#ifdef MRNET_LTWT_THREADSAFE
    net_top->sync = Monitor_construct();
#else
    net_top->sync = NULL;
#endif

    mrn_dbg(5, mrn_printf(FLF,stderr, 
                          "About to insert node into net_top->nodes, with rank=%d\n", irank));
    net_top->root = NetworkTopology_new_Node(net_top,
                                             ihostname, iport, irank, 
                                             is_backend);
    return net_top;
}

void delete_NetworkTopology_t( NetworkTopology_t* net_top )
{
    size_t i;
    if( net_top != NULL ) {
        NetworkTopology_lock(net_top);

        // delete all Node_t in nodes map, then the map
        for( i = 0; i < net_top->nodes->size; i++ ) {
            Node_t* tmp_node = (Node_t*) get_val( net_top->nodes, net_top->nodes->keys[i] );
            delete_Node_t( tmp_node );
        }
        delete_map_t(net_top->nodes);
        net_top->nodes = NULL;

        // delete the vectors
        delete_vector_t(net_top->orphans);
        net_top->orphans = NULL;
        delete_vector_t(net_top->backend_nodes);
        net_top->backend_nodes = NULL;
        delete_vector_t(net_top->parent_nodes);
        net_top->parent_nodes = NULL;

        // delete the serial graph
	    if( net_top->serial_graph != NULL ) {
            delete_SerialGraph_t( net_top->serial_graph );
            net_top->serial_graph = NULL;
        }

        NetworkTopology_unlock(net_top);

#ifdef MRNET_LTWT_THREADSAFE
        Monitor_destruct(net_top->sync);
        free(net_top->sync);
#endif

        free( net_top );
    }
}

unsigned int NetworkTopology_get_NumNodes(NetworkTopology_t* net_top)
{
    unsigned int retval;
    NetworkTopology_lock(net_top);
    retval = (unsigned int)net_top->nodes->size;
    NetworkTopology_unlock(net_top);
    return retval;
}

Node_t* NetworkTopology_get_Root(NetworkTopology_t* net_top)
{
    Node_t* ret_node;
    NetworkTopology_lock(net_top);
    ret_node = net_top->root;
    NetworkTopology_unlock(net_top);
    return ret_node; 
}

void NetworkTopology_get_BackEndNodes(NetworkTopology_t* UNUSED(net_top), 
                                      vector_t* UNUSED(nodes))
{
}

void NetworkTopology_get_ParentNodes(NetworkTopology_t* UNUSED(net_top), 
                                     vector_t* UNUSED(nodes))
{
}

void NetworkTopology_get_OrphanNodes(NetworkTopology_t* UNUSED(net_top), 
                                     vector_t* UNUSED( nodes))
{
}

int NetworkTopology_isInTopology(NetworkTopology_t* net_top,
                                 char * ihostname, 
                                 Port iport,
                                 Rank irank)
{
    int found = false;
    Node_t* tmp = NetworkTopology_find_Node(net_top, irank);
    if ( NULL != tmp ) {
        if ( 0 == strcmp(ihostname, tmp->hostname) ) { 
            if ( (iport == UnknownPort) ||
                 (tmp->port == UnknownPort) ||
                 (iport == tmp->port) )
                found = true;
        }
    } 
    return found;
}

char * NetworkTopology_get_TopologyStringPtr(NetworkTopology_t * net_top)
{
    char * retval;

    NetworkTopology_lock(net_top);

    delete_SerialGraph_t( net_top->serial_graph );
    net_top->serial_graph = new_SerialGraph_t(NULL);

    NetworkTopology_serialize(net_top, net_top->root);

    retval = strdup( SerialGraph_get_ByteArray(net_top->serial_graph) );

    NetworkTopology_unlock(net_top);

    return retval;
}

char* NetworkTopology_get_LocalSubTreeStringPtr(NetworkTopology_t* net_top)
{ 
    char* retval;
    char* localhost;
    SerialGraph_t* my_subgraph;
    
    mrn_dbg_func_begin();
    NetworkTopology_lock(net_top);

    delete_SerialGraph_t( net_top->serial_graph );
    net_top->serial_graph = new_SerialGraph_t(NULL);
    
    NetworkTopology_serialize (net_top, net_top->root);

    localhost = Network_get_LocalHostName(net_top->net);
    my_subgraph =  SerialGraph_get_MySubTree(net_top->serial_graph,
                                             localhost,
                                             Network_get_LocalPort(net_top->net),
                                             Network_get_LocalRank(net_top->net));
  
    retval = strdup(my_subgraph->byte_array);
    delete_SerialGraph_t( my_subgraph );

    mrn_dbg(5, mrn_printf(FLF, stderr, "returning '%s'\n", retval));
    NetworkTopology_unlock(net_top);
    mrn_dbg_func_end();

    return retval;
}

void NetworkTopology_serialize(NetworkTopology_t* net_top, Node_t* inode)
{
    size_t iter;
    NetworkTopology_lock(net_top);

    if (inode->is_backend) {
        // leaf node, just add my name to serial representation and return
        SerialGraph_add_Leaf(net_top->serial_graph, inode->hostname, inode->port, inode->rank);
        NetworkTopology_unlock(net_top);
        return;
    }
    else {
        // starting new sub-tree component in graph serialization
        SerialGraph_add_SubTreeRoot(net_top->serial_graph, inode->hostname, inode->port, inode->rank);
    }

    for (iter = 0; iter < inode->children->size; iter++) {
        assert( inode->children->vec[iter] != inode );
        NetworkTopology_serialize(net_top, (Node_t*)(inode->children->vec[iter]));
    }

    // ending sub-tree component in graph serialization:
    SerialGraph_end_SubTree(net_top->serial_graph);
    NetworkTopology_unlock(net_top);

}

int NetworkTopology_reset(NetworkTopology_t* net_top, SerialGraph_t* isg)
{
    SerialGraph_t* cur_sg;
    char *host, *sg_str;
    size_t i;
    Rank rank;
    Port port;
    NetworkTopology_lock(net_top);

    sg_str = SerialGraph_get_ByteArray(isg);
    mrn_dbg(5, mrn_printf(FLF, stderr, "Reseting topology to '%s'\n", sg_str)); 
    
    if( net_top->serial_graph != NULL )
        delete_SerialGraph_t( net_top->serial_graph );
    net_top->serial_graph = isg;

    // delete all Node_t in nodes map, then clear the map
    for( i = 0; i < net_top->nodes->size; i++ ) {
        Node_t* tmp_node = (Node_t*) get_val( net_top->nodes, net_top->nodes->keys[i] );
        delete_Node_t( tmp_node );
    }
    clear_map_t(&(net_top->nodes));
    clear(net_top->orphans);
    clear(net_top->backend_nodes);

    net_top->root = NULL;

    if( 0 == strcmp(sg_str, NULL_STRING) ) {
        NetworkTopology_unlock(net_top);
        return true;
    }

    SerialGraph_set_ToFirstChild(net_top->serial_graph); 
 
    host = SerialGraph_get_RootHostName(isg);
    port = SerialGraph_get_RootPort(isg);
    rank = SerialGraph_get_RootRank(isg);

    mrn_dbg(5, mrn_printf(FLF, stderr, "Root: %s:%hu:%u\n",
                          host, port, rank));
           
    net_top->root = NetworkTopology_new_Node(net_top, host, port, rank, 
                                             SerialGraph_is_RootBackEnd(isg));
    if( host != NULL )
        free(host);

    cur_sg = SerialGraph_get_NextChild(isg);
    for( ; cur_sg != NULL; cur_sg = SerialGraph_get_NextChild(isg) ) {

        if( ! NetworkTopology_add_SubGraph(net_top, net_top->root, 
                                           cur_sg, false) ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "add_SubGraph() failed\n"));
            NetworkTopology_unlock(net_top);
            return false;
        }
        delete_SerialGraph_t( cur_sg );
    }
    
    NetworkTopology_unlock(net_top);
    return true;
}

int NetworkTopology_add_SubGraph(NetworkTopology_t* net_top, Node_t* inode, 
                                 SerialGraph_t* isg, int iupdate) 
{
    Node_t *node;
    SerialGraph_t* cur_sg;
    char* host;
    
    mrn_dbg_func_begin();
    NetworkTopology_lock(net_top);
    mrn_dbg(5, mrn_printf(FLF, stderr, "Node[%d] adding subgraph '%s'\n",
                          inode->rank, isg->byte_array));

    if( ! findElement(net_top->parent_nodes, inode) )
        pushBackElement(net_top->parent_nodes, inode);

    // search for root of subgraph
    node = NetworkTopology_find_Node(net_top, SerialGraph_get_RootRank(isg));
    if (node == NULL) {
        // Not found! allocate
        mrn_dbg(5, mrn_printf(FLF, stderr, "node==NULL, about to allocate\n"));
        host = SerialGraph_get_RootHostName(isg);
        node = NetworkTopology_new_Node(net_top,
                                        host,
                                        SerialGraph_get_RootPort(isg),
                                        SerialGraph_get_RootRank(isg),
                                        SerialGraph_is_RootBackEnd(isg));
        if( host != NULL )
            free(host);
    }

    mrn_dbg(5, mrn_printf(FLF, stderr, "Adding node[%u] as child of node[%u]\n", 
                          node->rank, inode->rank));
    // add root of new subgraph as child of input node
    NetworkTopology_set_Parent(net_top, node->rank, inode->rank, iupdate);

    SerialGraph_set_ToFirstChild(isg);
    for( cur_sg = SerialGraph_get_NextChild(isg);
         cur_sg != NULL;
         cur_sg = SerialGraph_get_NextChild(isg) ) {

        NetworkTopology_add_SubGraph( net_top, node, cur_sg, iupdate );
        delete_SerialGraph_t( cur_sg );
    }

    NetworkTopology_unlock(net_top);
    mrn_dbg_func_end();
    return 1;
}

int NetworkTopology_remove_Node_2(NetworkTopology_t* net_top, Node_t* inode)
{
    Node_t* node_to_delete;
    size_t i;
    Node_t* cur_node;

    NetworkTopology_lock(net_top);
    mrn_dbg_func_begin();

    if (net_top->root == inode)
        net_top->root = NULL;

    // remove from ophans, back-ends, parents list
    net_top->nodes = erase(net_top->nodes, (int)inode->rank);
    net_top->orphans = eraseElement(net_top->orphans, inode);
    
    mrn_dbg(5, mrn_printf(FLF, stderr,
            "Removed rank %d from nodes and orphans list.\n",
            (int)inode->rank));

    if (inode->is_backend) {
        net_top->backend_nodes = eraseElement(net_top->backend_nodes, inode);
    } else {
        // find appropriate node to delete (necessary b/c of how they are stored)
        for (i = 0; i < net_top->parent_nodes->size; i++){
            cur_node = (Node_t*)net_top->parent_nodes->vec[i];
            if (cur_node->rank == inode->rank) {
                node_to_delete = cur_node;
                net_top->parent_nodes = eraseElement(net_top->parent_nodes,
                                                     node_to_delete);
                mrn_dbg(5, mrn_printf(FLF, stderr,
                            "Removed rank %d from parent_nodes list.\n",
                            (int)inode->rank));
                break;
            }
        }
    }

    // remove me as my children's parent, and set children as oprhans
    for (i = 0; i < inode->children->size; i++) {
        cur_node = (Node_t*)(inode->children->vec[i]);
        if (cur_node->parent == inode) {
            NetworkTopology_Node_set_Parent(cur_node, NULL);
            pushBackElement(net_top->orphans, cur_node);
        }
    }

    mrn_dbg(5, mrn_printf(FLF, stderr,
                "Removed from children's parent list.\n"));
  
    free(inode);

    mrn_dbg_func_end();
    NetworkTopology_unlock(net_top);
    return true;
}

int NetworkTopology_remove_Node(NetworkTopology_t* net_top, Rank irank)
{
    Node_t* node_to_remove;
    int retval;
    NetworkTopology_lock(net_top);
    
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
    NetworkTopology_unlock(net_top);
    return retval;
}

TopologyLocalInfo_t* new_TopologyLocalInfo_t(NetworkTopology_t* topol, Node_t* node)
{
    TopologyLocalInfo_t* new_top;
    NetworkTopology_lock(topol);
    new_top = (TopologyLocalInfo_t*) calloc( (size_t)1, sizeof(TopologyLocalInfo_t) );
    assert(new_top);
    new_top->topol = topol;
    new_top->local_node = node;

    NetworkTopology_unlock(topol);
    return new_top;
}

Network_t * TopologyLocalInfo_get_Network(TopologyLocalInfo_t * tli)
{
    return tli->topol->net;
}

Node_t* NetworkTopology_find_Node(NetworkTopology_t* net_top, Rank irank)
{
    Node_t* ret;
    
    NetworkTopology_lock(net_top);
    mrn_dbg_func_begin();
    
    ret = (Node_t*)(get_val(net_top->nodes, (int)irank));

    NetworkTopology_unlock(net_top);
    mrn_dbg_func_end();

    return ret;
}

int NetworkTopology_Node_is_BackEnd(Node_t* node)
{
    return node->is_backend;
}

void NetworkTopology_remove_SubGraph(NetworkTopology_t* net_top, Node_t* inode)
{
    size_t iter;
    
    mrn_dbg_func_begin();

    for( iter = 0; iter < inode->children->size; iter++ ) {
        Node_t* cur_node = (Node_t*)inode->children->vec[iter];
        assert( cur_node != inode );
        NetworkTopology_remove_SubGraph(net_top, cur_node);
        NetworkTopology_remove_Node_2(net_top, cur_node);
    }

    clear(inode->children);

    mrn_dbg_func_end(); 
}

int NetworkTopology_set_Parent(NetworkTopology_t* net_top, Rank ichild_rank, 
                               Rank inew_parent_rank, int UNUSED(iupdate))
{
    Node_t* child_node;
    Node_t* new_parent_node;
    size_t j;
    
    mrn_dbg(5, mrn_printf(FLF, stderr, 
                          "Setting node[%d] parent to node[%d]\n", 
                          ichild_rank, inew_parent_rank));

    child_node = NetworkTopology_find_Node(net_top, ichild_rank);
    if (child_node == NULL) {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "child node[%d] not found!\n", ichild_rank));
        return 0;
    }

    new_parent_node = NetworkTopology_find_Node(net_top, inew_parent_rank);

    if (new_parent_node == NULL) {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "parent node[%d] not found!\n", inew_parent_rank));
        return 0;
    }

    if (child_node->parent != NULL) {
        NetworkTopology_Node_remove_Child(child_node->parent, child_node);
    }

    NetworkTopology_Node_set_Parent(child_node, new_parent_node);
    NetworkTopology_Node_add_Child(new_parent_node, child_node);
    NetworkTopology_remove_Orphan(net_top, child_node->rank); 
    
    //child_node->ascendants = new_parent_node->ascendants;
    clear(child_node->ascendants);
    for (j = 0; j < new_parent_node->ascendants->size; j++) {
        pushBackElement(child_node->ascendants,
                        new_parent_node->ascendants->vec[j]);
    }
    if (!findElement(child_node->ascendants, new_parent_node)) {
        pushBackElement(child_node->ascendants, new_parent_node);
    }
    
    return 1;
}

int NetworkTopology_remove_Orphan(NetworkTopology_t* net_top, Rank r) 
{
    Node_t* node;
    NetworkTopology_lock(net_top);
    // find the node associated with r
    node = (Node_t*)(get_val(net_top->nodes, (int)r));

    if (!node) {
        NetworkTopology_unlock(net_top);
        return false;
    }

    net_top->orphans = eraseElement(net_top->orphans, node);

    NetworkTopology_unlock(net_top);
    return true;
}

void NetworkTopology_print(NetworkTopology_t* net_top, FILE * f) 
{
    char* cur_parent_str;
    char* cur_child_str;
    char rank_str[128];
    size_t node_iter, set_iter;
    Node_t* cur_node;

    NetworkTopology_lock(net_top);
    
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

    free(cur_parent_str);
    free(cur_child_str);

    NetworkTopology_unlock(net_top);
}

Node_t* NetworkTopology_find_NewParent(NetworkTopology_t* net_top, 
                                       Rank ichild_rank,
                                       unsigned int inum_attempts,
                                       ALGORITHM_T ialgorithm)
{
    vector_t* potential_adopters = new_empty_vector_t(); // vec of Node_t*
    Node_t* adopter = NULL;
    Node_t* orphan = NULL;
    size_t i, j;
    Node_t* cur = NULL;
    Node_t* cur_node = NULL;
    NetworkTopology_lock(net_top);

    mrn_dbg_func_begin();

    mrn_dbg(5, mrn_printf(FLF, stderr, "rank=%u\n", ichild_rank));

    if (inum_attempts > 0) {
        // previously computed list, but failed to contact new parent
        if (inum_attempts >= potential_adopters->size) {
            NetworkTopology_unlock(net_top);
            return NULL;
        }      

        adopter = (Node_t*)(potential_adopters->vec[inum_attempts]);

        mrn_dbg(5, mrn_printf(FLF, stderr, "Returning: node[%d]:%s:%s\n",
                            NetworkTopology_Node_get_Rank(adopter),
                            NetworkTopology_Node_get_HostName(adopter),
                            NetworkTopology_Node_get_Port(adopter)));

        NetworkTopology_unlock(net_top);
        return adopter;
    }

    orphan = NetworkTopology_find_Node(net_top, ichild_rank);
    assert(orphan);

    // computer list of potential adopters
    clear(potential_adopters);  
    NetworkTopology_find_PotentialAdopters(net_top, orphan, net_top->root, potential_adopters);
    if (potential_adopters->size == 0){
        mrn_dbg(5, mrn_printf(FLF,stderr, "No adopters left :(\n"));
        NetworkTopology_unlock(net_top);
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
            mrn_dbg(5, mrn_printf(FLF, stderr, "adopter[%d] is [%s:%u], adoption_wrs_key=%f\n", j, cur->hostname, cur->rank, cur->wrs_key)); 
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
        mrn_dbg(5, mrn_printf(FLF, stderr, "Returning: node[%u]:%s:%d\n",
                            NetworkTopology_Node_get_Rank(adopter),
                            NetworkTopology_Node_get_HostName(adopter),
                            NetworkTopology_Node_get_Port(adopter)));
    } else {
        mrn_dbg(5, mrn_printf(FLF, stderr, "No adopter found ... \n"));
    }

    NetworkTopology_unlock(net_top);
    mrn_dbg_func_end();

    return adopter;
}

void NetworkTopology_find_PotentialAdopters(NetworkTopology_t* net_top,
                                            Node_t* iorphan,
                                            Node_t* ipotential_adopter,
                                            vector_t* potential_adopters)
{
    size_t i;
    
    mrn_dbg_func_begin();

    mrn_dbg(5, mrn_printf(FLF, stderr, 
                "Can Node[%u]:%s:%hu (%s) adopt Orphan[%u]:%s :%hu ...",
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
        NetworkTopology_find_PotentialAdopters(net_top, iorphan,
                                               ipotential_adopter->children->vec[i], potential_adopters);
    }
    
    mrn_dbg_func_end();
}

void NetworkTopology_compute_AdoptionScores(NetworkTopology_t* net_top,
                                            vector_t* iadopters,
                                            Node_t* orphan)
{
    size_t i;
    NetworkTopology_lock(net_top);
    
    NetworkTopology_compute_TreeStatistics(net_top);

    for (i = 0; i < iadopters->size; i++) {
        NetworkTopology_Node_compute_AdoptionScore(net_top, 
                                                  (Node_t*)(iadopters->vec[i]), 
                                                   orphan,
                                                   net_top->min_fanout,
                                                   net_top->max_fanout, 
                                                   net_top->depth); 
    } 
    
    NetworkTopology_unlock(net_top);
}




void NetworkTopology_compute_TreeStatistics(NetworkTopology_t* net_top)
{
    size_t i;
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

void NetworkTopology_Node_set_Port(Node_t* node, Port port) {
    node->port = port;
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
    size_t i;
    
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
    // Find closest common ascendant to both potential adopter and orphan
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
    } while( (common_ascendant == NULL) && (cur_ascendant != NULL) );

    if( common_ascendant == NULL ) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "no common ascendant found\n"));
        return (unsigned int)-1;
    }
    
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
void NetworkTopology_Node_compute_AdoptionScore(NetworkTopology_t* 
                                                    UNUSED(net_top),
                                                Node_t* adopter,
                                                Node_t* orphan,
                                                unsigned int imin_fanout,
                                                unsigned int imax_fanout,
                                                unsigned int idepth)
{
    unsigned int depth_increase;
    unsigned int proximity;
    unsigned int fanout;
    double fanout_score;
    double depth_increase_score;
    double proximity_score;

    depth_increase = NetworkTopology_Node_get_DepthIncrease(adopter, orphan); 
    proximity = NetworkTopology_Node_get_Proximity(adopter, orphan); 
    fanout = adopter->children->size;

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
    parent->is_backend=false;
    pushBackElement(parent->children, (void*)child);
}

int NetworkTopology_Node_remove_Child(Node_t* parent, Node_t* child)
{
    mrn_dbg_func_begin();
    parent->children = eraseElement(parent->children, (void*)child);
    mrn_dbg_func_end();

    return true;
}

Node_t* NetworkTopology_new_Node(NetworkTopology_t* net_top, char* host_name, 
                                 Port port, Rank rank, int is_backend)
{
    Node_t * node = NULL;
    NetworkTopology_lock(net_top);

    mrn_dbg(5, mrn_printf(FLF, stderr, "Creating node[%u] %s:%hu\n",
                          rank, host_name, port));

    // Note: new node owns host_name
    node = new_Node_t(host_name, port, rank, is_backend);
    insert(net_top->nodes, (int)rank, node);
   
    if( is_backend ) {
        if( ! findElement(net_top->backend_nodes, node) ) {
            mrn_dbg(5, mrn_printf(FLF, stderr, "Adding node[%u] as backend\n", rank));
            pushBackElement(net_top->backend_nodes, node);
        }
    }

    NetworkTopology_unlock(net_top);
    return node;
}

// Topology Update Methods
void NetworkTopology_add_BackEnd(NetworkTopology_t * net_top, 
                                 uint32_t rprank, uint32_t rcrank, 
                                 char * rchost, uint16_t rcport)
{
    Node_t * n = NetworkTopology_find_Node(net_top, rcrank);

    mrn_dbg(5, mrn_printf(FLF, stderr, 
                          "Adding backend node[%u] as child of node[%u]\n",
                          rcrank, rprank));

    if (n == NULL) {
        n = NetworkTopology_new_Node(net_top, rchost, rcport, rcrank, true);

        if ( !(NetworkTopology_set_Parent(net_top, rcrank, rprank, false))) {
            mrn_dbg(1, mrn_printf(FLF, stderr, 
                                  "Set parent for %s failed\n", rchost));
        }
        mrn_dbg(5, mrn_printf(FLF, stderr, "topology is after add %s\n", 
                              NetworkTopology_get_TopologyStringPtr(net_top)));
    } else {
        mrn_dbg(5, mrn_printf(FLF, stderr, "node already present\n"));
    }
}

void NetworkTopology_add_InternalNode(NetworkTopology_t * net_top, 
                                      uint32_t rprank, uint32_t rcrank, 
                                      char * rchost, uint16_t rcport)
{
    Node_t * n = NetworkTopology_find_Node(net_top, rcrank);

    mrn_dbg(5, mrn_printf(FLF, stderr, 
                          "Adding internal node[%u] as child of node[%u]\n",
                          rcrank, rprank));

    if (n == NULL) {
        NetworkTopology_new_Node(net_top, rchost, rcport, rcrank, false);

        if ( !(NetworkTopology_set_Parent(net_top, rcrank, rprank, false))) {
            mrn_dbg(1, mrn_printf(FLF, stderr, 
                                  "Set parent for %s failed\n", rchost));
        }
        mrn_dbg(5, mrn_printf(FLF, stderr, "topology after add %s\n", 
                              NetworkTopology_get_TopologyStringPtr(net_top)));
    } else {
        mrn_dbg(5, mrn_printf(FLF, stderr, "node already present\n")); 
    }
}

void NetworkTopology_change_Port(NetworkTopology_t * net_top,
                                 uint32_t rcrank, uint16_t rcport)
{
    Node_t * update_node;
    if( rcport == UnknownPort )
        return;

    update_node = NetworkTopology_find_Node(net_top, rcrank);

    mrn_dbg(5, mrn_printf(FLF, stderr, 
                          "Changing port of node[%u] from %hu to %hu\n",
                          rcrank, update_node->port, rcport));

    // Actual port update on the local network topology
    NetworkTopology_Node_set_Port(update_node, rcport);
}
