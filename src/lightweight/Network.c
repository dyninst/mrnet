/****************************************************************************
 *  Copyright 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "mrnet_lightweight/MRNet.h"

#include "BackEndNode.h"
#include "mrnet_lightweight/NetworkTopology.h"
#include "ChildNode.h"
#include "Protocol.h"
#include "PeerNode.h"
#include "SerialGraph.h"
#include "map.h"
#include "utils_lightweight.h"
#include "vector.h"

// from xplat
#include "xplat_lightweight/NetUtils.h"

const int MIN_OUTPUT_LEVEL=0;
const int MAX_OUTPUT_LEVEL=5;
int CUR_OUTPUT_LEVEL=1;
char* MRN_DEBUG_LOG_DIRECTORY = NULL;

void init_local(void)
{
#if !defined(os_windows)
    int fd;
    while ((fd = socket(AF_INET, SOCK_STREAM, 0)) <= 2) {
        if (fd == -1) break;
    }
    if (fd > 2) close(fd);

#else
    // init Winsock
    WORD version = MAKEWORD(2,2); /* socket version 2.2 supported by all moddern Windows */
    WSADATA data;
    if (WSAStartup(version, &data) != 0)
        fprintf(stderr, "WSAStartup failed!\n");
#endif

}

void cleanup_local(void)
{
#if defined(os_windows)
    // cleanup Winsock
    WSACleanup();
#endif
}

Network_t* new_Network_t()
{
    Network_t* net = (Network_t*)malloc(sizeof(Network_t));
    assert(net);
    net->local_port = UnknownPort;
    net->local_rank = UnknownRank;
    net->children = new_empty_vector_t();
    net->streams = new_map_t();
    net->stream_iter = 0;
    net->recover_from_failures = true;
    net->_was_shutdown = 0;

    init_local();

    return net;
}

void delete_Network_t(Network_t * net)
{
    int i;
    Stream_t * cur_stream;

    cleanup_local();

    delete_vector_t(net->children);

    for (i = 0; i < net->streams->size; i++) {
        cur_stream = (Stream_t*)get_val(net->streams, net->streams->keys[i]);
        delete_Stream_t(cur_stream);
    }

    delete_map_t(net->streams);
    free(net);

    if( MRN_DEBUG_LOG_DIRECTORY != NULL )
        free( MRN_DEBUG_LOG_DIRECTORY );      
}

char* Network_get_LocalHostName(Network_t* net) {
    return net->local_hostname;
}

Port Network_get_LocalPort(Network_t* net) {
    return net->local_port;
}

Rank Network_get_LocalRank(Network_t* net) {
    return net->local_rank;
}

BackEndNode_t* Network_get_LocalChildNode(Network_t* net)
{
    return net->local_back_end_node;
}


struct NetworkTopology_t* Network_get_NetworkTopology(Network_t* net) {
    return net->network_topology;
}

Network_t*
Network_CreateNetworkBE ( int argc, char* argv[] )
{ 
    char* phostname;
    Port pport;
    Rank prank;
    char* myhostname;
    Rank myrank;
    Network_t* net;

    //endianTest();
    
    // Get parent/local info from end of BE args
    if ( argc >= 6) {
        phostname = argv[argc-5];
        pport = (Port)strtoul( argv[argc-4], NULL, 10 );
        prank = (Rank)strtoul( argv[argc-3], NULL, 10 );
        myhostname = argv[argc-2];
        myrank = (Rank)strtoul( argv[argc-1], NULL, 10);

        net = Network_init_BackEnd(phostname, pport, prank,
                                   myhostname, myrank);

        mrn_dbg(5, mrn_printf(FLF, stderr, "Returning from Network_CreateNetworkBE\n"));
        return net;
    }

    else {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "Not enough args, please provide parent/local info\n"));
        return NULL;
    } 
} 



/* back-end constructor method that is used to attach to an 
 * instantiated MRNet process tree */
Network_t* Network_init_BackEnd(/*const*/ char* iphostname, Port ipport, 
                                Rank iprank, 
                                /*const*/ char* imyhostname, Rank imyrank)
{
 
    Network_t* net = new_Network_t();
    char* pretty_host;
    char* myhostname;
    BackEndNode_t* be;
  
    setrank(imyrank);  
    
    pretty_host = (char*) malloc(sizeof(char)*256);
    assert(pretty_host);
    myhostname = (char*) malloc(sizeof(char)*256);
    assert(myhostname);
    NetUtils_FindNetworkName( imyhostname, myhostname );
    NetUtils_GetHostName( myhostname, pretty_host );

    net->local_rank = imyrank;

    // create the new node
    be = CreateBackEndNode( net, myhostname, imyrank,
                            iphostname, ipport, iprank );
  
    assert ( be != NULL);
 
    // update the network struct
    mrn_dbg(5, mrn_printf(FLF, stderr, "Setting network's local BE\n"));
    net->local_back_end_node = be;

    free(pretty_host);

    // create the BE-specific stream
    Network_new_Stream(net, imyrank, NULL, 0, 0, 0, 0);

    return net;

}

int Network_recv_internal(Network_t* net, bool_t blocking)
{
    vector_t* packet_list;
    
    mrn_dbg_func_begin();

    packet_list = new_empty_vector_t();

    mrn_dbg(5, mrn_printf(FLF, stderr, "Calling recv_packets()\n"));
    if( Network_recv_PacketsFromParent(net, packet_list, blocking) == -1 ) {
        mrn_dbg(3, mrn_printf(FLF, stderr, "recv_packets() failed\n"));
        if (Network_recover_FromFailures(net)) {
            Network_recover_FromParentFailure(net);
            if (Network_recv_PacketsFromParent(net, packet_list, blocking) == -1) {
                mrn_dbg(3, mrn_printf(FLF, stderr, 
                                      "recv() failed twice, return -1\n"));
                return -1;
            }
        } 
        else return -1;
    }

    if( packet_list->size == 0 ) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "No packets read!\n"));
        return 0;
    }
    else {
        mrn_dbg(5, mrn_printf(FLF, stderr, "Calling proc_packets()\n"));

        if( ChildNode_proc_PacketsFromParent(net->local_back_end_node, packet_list) ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "proc_packets() failed\n"));
            return -1;
        }
    }
  
    // if we get here, we have found data to return
    mrn_dbg(5, mrn_printf(FLF, stderr, 
                          "Found/processed packets! Returning\n"));
    
    return 1;
}

Packet_t* Network_recv_stream_check(Network_t* net)
{
    int packet_found;  
    int start_iter;
    Stream_t* cur_stream;
    Packet_t* cur_packet = NULL;

    mrn_dbg(5, mrn_printf(FLF, stderr, "calling Network_have_Streams()\n"));
    if( Network_have_Streams(net) ) {
    
        packet_found = false;

        start_iter = net->stream_iter;
    
        do {
            // get the Stream associated with the current stream_iter,
            // which is an index into the keys array
            cur_stream = (Stream_t*)get_val(net->streams, 
                                            net->streams->keys[net->stream_iter]);

            mrn_dbg(5, mrn_printf(FLF, stderr,
                                  "Checking for packets on stream[%d]...\n",
                                  cur_stream->id));
            cur_packet = Stream_get_IncomingPacket(cur_stream);
            if (cur_packet != NULL) 
                packet_found = true;
            mrn_dbg(5, mrn_printf(FLF, stderr, "... %s!\n", 
                                  (packet_found ? "found" : "not found" )));
            net->stream_iter++;
            if (net->stream_iter == net->streams->size) {
                // wrap around to the beginning of the map
                net->stream_iter = 0;
            }
        } while((start_iter != net->stream_iter) && !packet_found);
    }
    
    return cur_packet;
}


int Network_recv_nonblock(Network_t* net, int *otag, 
                          Packet_t* opacket, Stream_t** ostream)
{
    Packet_t* cur_packet = NULL;
    cur_packet = Network_recv_stream_check(net);
    if( cur_packet != NULL ) {
        *otag = cur_packet->tag;
        *ostream = Network_get_Stream(net, cur_packet->stream_id);
        assert(*ostream);
        *opacket = *cur_packet;
        mrn_dbg(5, mrn_printf(FLF, stderr, "cur_packet tag: %d, fmt: %s\n", 
                              cur_packet->tag, cur_packet->fmt_str));
        return 1;
    }
    
    if( Network_has_PacketsFromParent(net) )
        return Network_recv(net, otag, opacket, ostream);

    return 0;
}

int Network_recv(Network_t* net, int *otag, 
                 Packet_t* opacket, Stream_t** ostream)
{
    int checked_network = false; // have we checked sockets for input?
    Packet_t* cur_packet = NULL;
    int packet_found;  
    int start_iter;
    Stream_t* cur_stream;
    int retval;

    // check streams for input:
 get_packet_from_stream_label: 

    cur_packet = Network_recv_stream_check(net);
    if( cur_packet != NULL ) {
        *otag = cur_packet->tag;
        *ostream = Network_get_Stream(net, cur_packet->stream_id);
        assert(*ostream);
        *opacket = *cur_packet;
        mrn_dbg(5, mrn_printf(FLF, stderr, "cur_packet tag: %d, fmt: %s\n", 
                              cur_packet->tag, cur_packet->fmt_str));
        return 1;
    }

    // No packets are already in the stream
    // check whether there is data waiting to be read on our socket
    mrn_dbg(5, mrn_printf(FLF, stderr, 
            "No packets waiting in stream, checking for data on socket\n"));
    retval = Network_recv_internal(net, true);

    checked_network = true;

    if( retval == -1 ) {
        mrn_dbg(3, mrn_printf(FLF, stderr, "Network_recv() failed\n"));
        return -1;
    }
    else if( retval == 1 ) {
        // go back if we found a packet
        mrn_dbg(3, mrn_printf(FLF, stderr, "Network_recv() found packet!\n"));
        goto get_packet_from_stream_label;
    } 
    mrn_dbg(3, mrn_printf(FLF, stderr, "Network_recv() No packets found\n"));

    return 0;
}

int Network_is_LocalNodeBackEnd(Network_t* net) {
    return (net->local_back_end_node != NULL);
}


int Network_has_PacketsFromParent(Network_t* net)
{
    return PeerNode_has_data(net->parent);
}

int Network_recv_PacketsFromParent(Network_t* net, vector_t* opackets, bool_t blocking)
{
    return PeerNode_recv(net->parent, opackets, blocking);
}

void Network_shutdown_Network(Network_t* net)
{
    int i;
    Stream_t * cur_stream;
    
    mrn_dbg_func_begin();

    net->_was_shutdown = 1;

    Network_reset_Topology(net, NULL);

    // close streams
    for (i = 0; i < net->streams->size; i++) {
        cur_stream = (Stream_t*)get_val(net->streams, net->streams->keys[i]);
        cur_stream->_was_closed = 1;
    }

    mrn_dbg_func_end();
}

int Network_reset_Topology(Network_t* net, char* itopology)
{
    SerialGraph_t* sg = NULL;
 
    mrn_dbg_func_begin();

    sg = new_SerialGraph_t( itopology );
    if( ! NetworkTopology_reset(net->network_topology, sg) ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Topology->reset() failed\n"));
        return false;
    }
    return true;
}

int Network_add_SubGraph(Network_t * net, Rank iroot_rank, SerialGraph_t * sg, int iupdate)
{
    Node_t * node = NetworkTopology_find_Node(net->network_topology, iroot_rank);
    if( ! node )
        return false;

    if( NetworkTopology_add_SubGraph(net->network_topology, node,
                                     sg, iupdate) ) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "add_SubGraph() failed\n"));
        return false;
    }
    return true;
}

Stream_t* Network_new_Stream(Network_t* net,
                             unsigned int iid,
                             Rank* ibackends,
                             unsigned int inum_backends,
                             int ius_filter_id,
                             int isync_filter_id,
                             int ids_filter_id)
{
    Stream_t* stream;
    
    mrn_dbg_func_begin();

    stream = new_Stream_t(net, iid, ibackends, inum_backends,
                          (unsigned)ius_filter_id, 
                          (unsigned)isync_filter_id, 
                          (unsigned)ids_filter_id);
    insert(net->streams, (int)iid, stream);

    mrn_dbg_func_end();
    return stream;
}

int Network_remove_Node(Network_t* net, Rank ifailed_rank, int iupdate)
{
    int i;
    int retval;
    
    mrn_dbg_func_begin();

    mrn_dbg(3, mrn_printf(FLF, stderr, "Deleting PeerNode: node[%u] ...\n", ifailed_rank));
    Network_delete_PeerNode(net, ifailed_rank);

    mrn_dbg(3, mrn_printf(FLF, stderr, "Removing from Topology: node[%u] ...\n", 
                          ifailed_rank));
    NetworkTopology_remove_Node(net->network_topology, ifailed_rank);

    mrn_dbg(3, mrn_printf(FLF, stderr, "Removing from Streams: node[%u] ...\n" , 
                          ifailed_rank));
  
    for (i = 0; i < net->streams->size; i++) {
        Stream_remove_Node((Stream_t*)get_val(net->streams, 
                                              net->streams->keys[i]), 
                           ifailed_rank);
    }

    retval = true;

    mrn_dbg_func_end();
    return retval;
}

int Network_delete_PeerNode(Network_t* net, Rank irank)
{
    struct PeerNode_t* peerNode;
    int iter;
    Node_t* cur_child;
    
    if (net->parent->rank == irank) {
        peerNode = NULL;
        net->parent = peerNode;
        return true;
    }

    for (iter = 0; iter < net->children->size; iter++) {
        cur_child = (Node_t*)(net->children->vec[iter]);
        if (NetworkTopology_Node_get_Rank(cur_child) == irank) {
            net->children = eraseElement(net->children, cur_child);
            return true; 
        }
    } 

    return false;
}

int Network_change_Parent(Network_t* net, Rank ichild_rank, Rank inew_parent_rank)
{
    // update topology
    if( ! NetworkTopology_set_Parent(net->network_topology, 
                                     ichild_rank, inew_parent_rank, false) ) {
        return false;
    }
    return true;
}

int Network_have_Streams(Network_t* net)
{
    return (net->streams->root != NULL);  
}

Stream_t* Network_get_Stream(Network_t* net, unsigned int iid)
{
    Stream_t* ret = (Stream_t*)get_val(net->streams, (int)iid);
    if (ret == NULL)
        mrn_dbg(5, mrn_printf(FLF, stderr, "ret == NULL\n"));
    return ret;
}

void Network_delete_Stream(Network_t * net, unsigned int iid)
{
    int key = (int) iid;
    Stream_t* ret = (Stream_t*)get_val(net->streams, key);

    /* if we're deleting the iter, set to the next element */
    if (ret == (Stream_t*)get_val(net->streams, 
                                  net->streams->keys[net->stream_iter])) {
        net->stream_iter++;
        // wrap around to the beginning of the map, if necessary
        if (net->stream_iter == net->streams->size)
            net->stream_iter = 0;
    }   

    net->streams = erase(net->streams, key);
}

int Network_send_PacketToParent(Network_t* net, Packet_t* ipacket)
{
    mrn_dbg_func_begin();
    if (PeerNode_sendDirectly(net->parent, ipacket) == -1) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "upstream.send() failed\n"));
        if (Network_recover_FromFailures(net)) {
            mrn_dbg(1, mrn_printf(FLF, stderr, 
                                  "assume parent failure, try one more time\n"));
            Network_recover_FromParentFailure(net); 
            if (PeerNode_sendDirectly(net->parent, ipacket) == -1) {
                mrn_dbg(1, mrn_printf(FLF, stderr, "upstram.send() failed, again\n"));
                return -1;
            }
        } 
        else return -1;
    }

    mrn_dbg_func_end();
    return 0;
}

PeerNode_t* Network_new_PeerNode(Network_t* network,
                                 char* ihostname,
                                 Port iport,
                                 Rank irank,
                                 int iis_parent,
                                 int iis_internal)
{
    PeerNode_t* node = new_PeerNode_t(network, ihostname, iport, 
                                      irank, iis_parent, iis_internal);

    mrn_dbg(5, mrn_printf(FLF, stderr, "new peer node: %s:%d (%p) \n", 
                          node->hostname, node->rank, node));

    if (iis_parent) {
        network->parent = node;  
    } else {
        pushBackElement(network->children, node); 
    }
    
    return node;

}

int Network_recover_FromFailures(Network_t* net)
{
    return net->recover_from_failures;
}

void Network_enable_FailureRecovery(Network_t* net)
{
    net->recover_from_failures = true;
}

void Network_disable_FailureRecovery(Network_t* net) 
{
    net->recover_from_failures = false;
}

PeerNode_t* Network_get_ParentNode(Network_t* net)
{
    return net->parent;
}

void Network_set_ParentNode(Network_t* net, PeerNode_t* ip) 
{
    net->parent = ip;

    if (net->parent != NULL) {
        mrn_dbg(3, mrn_printf(FLF, stderr, "Parent node available\n"));
    } 
}

int Network_has_ParentFailure(Network_t* net) 
{
    struct timeval zeroTimeout;
    fd_set rfds;
    int sret;

    zeroTimeout.tv_sec = 0;
    zeroTimeout.tv_usec = 0;

    // set up file descriptor set for the poll
    FD_ZERO(&rfds);
    FD_SET(net->parent->event_sock_fd, &rfds);

    sret = select(net->parent->event_sock_fd + 1, &rfds, NULL, NULL, &zeroTimeout);
   
    if (sret != 1) {
        // select error
        perror("select()");
        return -1;
    } else {
        if (FD_ISSET(net->parent->event_sock_fd, &rfds)) { 
            return 1;
        }
    }
   
    return 0;
}

int Network_recover_FromParentFailure(Network_t* net) 
{

    Timer_t new_parent_timer = new_Timer_t();
    Timer_t cleanup_timer = new_Timer_t();
    Timer_t connection_timer = new_Timer_t();
    Timer_t overall_timer = new_Timer_t();
    Rank failed_rank;
    Node_t* new_parent_node;
    char* new_parent_name;
    PeerNode_t* new_parent;

    mrn_dbg_func_begin();

    PeerNode_mark_Failed(Network_get_ParentNode(net));
    failed_rank = PeerNode_get_Rank(Network_get_ParentNode(net));
    Network_set_ParentNode(net, NULL);

    mrn_dbg(3, mrn_printf(FLF, stderr, "Recovering from parent[%d]'s failure\n", 
                          failed_rank));

    NetworkTopology_print(Network_get_NetworkTopology(net), NULL);

    //Step 1: Compute new parent
    Timer_start(overall_timer);
    Timer_start(new_parent_timer);
    new_parent_node = NetworkTopology_find_NewParent(Network_get_NetworkTopology(net), Network_get_LocalRank(net), 0, ALG_WRS);
    if (!new_parent_node) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Can't find new parent! Exiting...\n"));
        exit(-1);
    }

    mrn_dbg(1, mrn_printf(FLF, stderr, "RECOVERY: NEW PARENT: %s:%d:%d\n", 
                          NetworkTopology_Node_get_HostName(new_parent_node),
                          NetworkTopology_Node_get_Port(new_parent_node),
                          NetworkTopology_Node_get_Rank(new_parent_node)));

    new_parent_name = NetworkTopology_Node_get_HostName(new_parent_node);
 
    new_parent = Network_new_PeerNode(net, 
                                      new_parent_name,
                                      NetworkTopology_Node_get_Port(new_parent_node),
                                      NetworkTopology_Node_get_Rank(new_parent_node),
                                      true, true);

    Timer_stop(new_parent_timer); 
   
    // Step 2. Establish data connection with new parent
    Timer_start(connection_timer);
    mrn_dbg(3, mrn_printf(FLF, stderr, "Establish new data connection ...\n"));

    mrn_dbg(5, mrn_printf(FLF, stderr, "local child node rank = %d\n", Network_get_LocalChildNode(net)->myrank));

    if (ChildNode_init_newChildDataConnection(Network_get_LocalChildNode(net), new_parent, failed_rank) == -1) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "ChildNode_init_newChildDataConnection() failed\n"));
        return -1;
    }

    // Step 3. Establish event detection connnection with new Parent
    // This step gets skipped
        
    mrn_dbg(3, mrn_printf(FLF, stderr, "New event connection established.\n"));
    Timer_stop(connection_timer);

    // Step 4. Propagate filter state for active streams to new parent
    // No filtering is done at BEs
    
    // Step 5. Update local topology and data structures
    Timer_start(cleanup_timer);
    mrn_dbg(3, mrn_printf(FLF, stderr, "Updating local structures ..\n"));
    // remove node, but don't update datastructs since following procedure will
    Network_remove_Node(net, failed_rank, false);
    Network_change_Parent(net, Network_get_LocalRank(net),
                          NetworkTopology_Node_get_Rank(new_parent_node));
    Timer_stop(cleanup_timer);
    Timer_stop(overall_timer);

    mrn_dbg_func_end();

    return 0;
}

char* Network_get_LocalSubTreeStringPtr(Network_t* net)
{
    return NetworkTopology_get_LocalSubTreeStringPtr(net->network_topology);
}

void Network_set_DebugLogDir( char* value )
{
    if( value != NULL )
        MRN_DEBUG_LOG_DIRECTORY = strdup( value );
}

void Network_set_OutputLevel(int l) 
{
    if (l <= MIN_OUTPUT_LEVEL) {
        CUR_OUTPUT_LEVEL = MIN_OUTPUT_LEVEL;
    } else if (l >= MAX_OUTPUT_LEVEL) {
        CUR_OUTPUT_LEVEL = MAX_OUTPUT_LEVEL;
    } else {
        CUR_OUTPUT_LEVEL = l;
    }
}

char Network_is_ShutDown(Network_t* net)
{
    return net->_was_shutdown;
}

void Network_waitfor_ShutDown(Network_t* net)
{
    Stream_t* stream;
    Packet_t* p;
    int tag = 0;
	
    p = (Packet_t*) malloc(sizeof(Packet_t));
    assert(p);

    while( ! net->_was_shutdown ) {

        if( Network_recv(net, &tag, p, &stream) != 1 ) {
            mrn_dbg(3, mrn_printf(FLF, stderr, "Network_recv() failure\n"));
            break;
        }

    }

    free(p);
}
