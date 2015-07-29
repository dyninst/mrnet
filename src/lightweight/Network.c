/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "utils_lightweight.h"
#include "BackEndNode.h"
#include "ChildNode.h"
#include "Protocol.h"
#include "PeerNode.h"
#include "SerialGraph.h"

#include "mrnet_lightweight/NetworkTopology.h"
#include "mrnet_lightweight/Stream.h"
#include "xplat_lightweight/NetUtils.h"
#include "xplat_lightweight/SocketUtils.h"
#include "xplat_lightweight/map.h"
#include "xplat_lightweight/vector.h"

const Rank UnknownRank = (Rank)-1;

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


#ifdef MRNET_LTWT_THREADSAFE

void _Network_lock(Network_t * net, enum sync_index mon)
{
    int retval;
    Monitor_t *tmp_mon;
    if(mon == NET_SYNC) {
        tmp_mon = net->net_sync;
    } else if(mon == RECV_SYNC) {
        tmp_mon = net->recv_sync;
    } else if(mon == PARENT_SYNC) {
        tmp_mon = net->parent_sync;
    } else {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "Incorrect enum passed to Network_lock\n"));
        return;
    }
    retval = Monitor_Lock( tmp_mon );
    if( retval != 0 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "Network_lock failed to acquire monitor lock: %s\n",
                              strerror(retval)));

    }
}

void _Network_unlock(Network_t *net, enum sync_index mon)
{
    int retval;
    Monitor_t *tmp_mon;
    if(mon == NET_SYNC) {
        tmp_mon = net->net_sync;
    } else if(mon == RECV_SYNC) {
        tmp_mon = net->recv_sync;
    } else if(mon == PARENT_SYNC) {
        tmp_mon = net->parent_sync;
    } else {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "Incorrect enum passed to Network_unlock\n"));
        return;
    }
    retval = Monitor_Unlock( tmp_mon );
    if( retval != 0 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "Network_unlock failed to release monitor: %s\n",
                              strerror(retval)));
    }
}

void _Network_monitor_wait(Network_t *net, 
                           enum sync_index mon, 
                           enum cond_vars cv)
{
    int retval;
    Monitor_t *tmp_mon;
    if(mon == NET_SYNC) {
        tmp_mon = net->net_sync;
    } else if(mon == RECV_SYNC) {
        tmp_mon = net->recv_sync;
    } else if(mon == PARENT_SYNC) {
        tmp_mon = net->parent_sync;
    } else {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "Incorrect enum passed to Network_monitor_wait\n"));
        return;
    }
    retval = Monitor_WaitOnCondition(tmp_mon, (int)cv);
    if( retval != 0 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
            "Network_monitor_wait failed to wait: %s\n",
            strerror(retval)));

    }
}

void _Network_monitor_broadcast(Network_t *net, 
                                enum sync_index mon, 
                                enum cond_vars cv)
{
    int retval;
    Monitor_t *tmp_mon;
    if(mon == NET_SYNC) {
        tmp_mon = net->net_sync;
    } else if(mon == RECV_SYNC) {
        tmp_mon = net->recv_sync;
    } else if(mon == PARENT_SYNC) {
        tmp_mon = net->parent_sync;
    } else {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "Incorrect enum passed to Network_monitor_broadcast\n"));
        return;
    }
    retval = Monitor_BroadcastCondition(tmp_mon, (int)cv);
    if( retval != 0 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
            "Network_monitor_broadcast failed to broadcast: %s\n",
            strerror(retval)));

    }
}

# define Network_lock(net,mon) _Network_lock(net,mon)
# define Network_unlock(net,mon) _Network_unlock(net,mon)
# define Network_monitor_wait(net,mon,cv) _Network_monitor_wait(net,mon,cv)
# define Network_monitor_broadcast(net,mon,cv) _Network_monitor_broadcast(net,mon,cv)

#else // !def MRNET_LTWT_THREADSAFE  

# define Network_lock(net,mon) do{}while(0)
# define Network_unlock(net,mon) do{}while(0)
# define Network_monitor_wait(net,mon,cv) do{}while(0)
# define Network_monitor_broadcast(net,mon,cv) do{}while(0)

#endif


Network_t* new_Network_t()
{
    Network_t* net = (Network_t*) calloc( (size_t)1, sizeof(Network_t) );
    assert(net != NULL);
    net->local_port = UnknownPort;
    net->local_rank = UnknownRank;
    net->parent = NULL;
    net->local_back_end_node = NULL;
    net->network_topology = NULL;
    net->children = new_empty_vector_t();
    net->streams = new_map_t();
    net->stream_iter = 0;
    net->recover_from_failures = true;
    net->_was_shutdown = 0;

#ifdef MRNET_LTWT_THREADSAFE
    net->net_sync = Monitor_construct();
    net->recv_sync = Monitor_construct();
    net->parent_sync = Monitor_construct();
    if( Monitor_RegisterCondition(net->parent_sync, PARENT_NODE_AVAILABLE) ) {
        assert(0);
    }
    if( Monitor_RegisterCondition(net->recv_sync, THREAD_LISTENING) ) {
        assert(0);
    }
#else
    net->net_sync = NULL;
    net->recv_sync = NULL;
    net->parent_sync = NULL;
#endif    
    net->thread_listening = 0;
    net->thread_recovering = 0;

    init_local();

    return net;
}

void delete_Network_t(Network_t * net)
{
    Stream_t * cur_stream;

    if( net != NULL ) {

        Network_lock(net, NET_SYNC);

        if( ! net->_was_shutdown ) {
            // detach before shutdown
            BackEndNode_proc_DeleteSubTree( net->local_back_end_node, 
                                            NULL /* Packet_t* - not used */ );
        }

        if( net->local_hostname != NULL )
            free( net->local_hostname );

        if( net->parent != NULL )
            free( net->parent );

        delete_BackEndNode_t( net->local_back_end_node );

        delete_NetworkTopology_t( net->network_topology );

        delete_vector_t(net->children);

        // delete all Stream_t in streams map, then the map
        while( net->streams->size > 0 ) {
            /* remove streams in reverse order to avoid lots of key movement
               caused by the use of map erase() */
            cur_stream = (Stream_t*)get_val(net->streams, 
                                            net->streams->keys[net->streams->size - 1]);
            if( cur_stream != NULL )
                delete_Stream_t(cur_stream);
        }
        delete_map_t(net->streams);

        Network_unlock(net, NET_SYNC);

#ifdef MRNET_LTWT_THREADSAFE 
        if(net->net_sync != NULL) {
            Monitor_destruct(net->net_sync);
            free(net->net_sync);
        }
        if(net->recv_sync != NULL) {
            Monitor_destruct(net->recv_sync);
            free(net->recv_sync);
        }
        if(net->parent_sync != NULL) {
            Monitor_destruct(net->parent_sync);
            free(net->parent_sync);
        }
#endif        
        
        free(net);
    }

    if( MRN_DEBUG_LOG_DIRECTORY != NULL )
        free( MRN_DEBUG_LOG_DIRECTORY );      

    cleanup_local();
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
Network_t* Network_init_BackEnd(char* iphostname, Port ipport, 
                                Rank iprank, 
                                char* imyhostname, Rank imyrank)
{
 
    BackEndNode_t* be;
    char* myhostname = NULL;
    Network_t* net = new_Network_t();
    assert( net != NULL );

    setrank(imyrank);  

    myhostname = strdup(imyhostname);

    net->local_rank = imyrank;

    // create the BE-specific stream
    Network_new_Stream(net, imyrank, NULL, 0, 0, 0, 0);

    // create the new node
    be = CreateBackEndNode( net, myhostname, imyrank,
                            iphostname, ipport, iprank );
  
    assert( be != NULL );
 
    // update the network struct
    mrn_dbg(5, mrn_printf(FLF, stderr, "Setting network's local BE\n"));
    net->local_back_end_node = be;

    return net;
}

int Network_recv_internal(Network_t* net, Stream_t* stream, bool_t blocking)
{
    vector_t* packet_list;
    Packet_t* ready_packet = NULL;
    PeerNode_t* orig_parent;
    PeerNode_t* recov_parent;
    int ret_val;
    int listened = 0;
    int no_lock = 0;

    mrn_dbg_func_begin();
    Network_lock(net, RECV_SYNC);

    packet_list = new_empty_vector_t();

    /* Once we obtain lock, check if data is available */
    if(stream != NULL) {
        ready_packet = Stream_get_IncomingPacket(stream);
    } else {
        ready_packet = Network_recv_stream_check(net);
    }

    orig_parent = Network_get_ParentNode(net);
    while(ready_packet == NULL) {
        /* No packet came for us while we were waiting for Network_recv_lock */
        if(!net->thread_listening) {
            mrn_dbg(5, mrn_printf(FLF, stderr, "Calling recv_packets()\n"));
            net->thread_listening++;
            listened = 1;
            assert(net->thread_listening == 1);
            Network_unlock(net, RECV_SYNC);
            ret_val = Network_recv_PacketsFromParent(net,packet_list,blocking);
            if( ret_val == -1 ) {
                mrn_dbg(3, mrn_printf(FLF, stderr, "recv() failed\n"));
                /* We've noticed a failure on the socket. Check if another  *
                 * thread hasn't already recovered.                         */

                // Check if the parent has sent an event packet
                ret_val = Network_recv_EventFromParent(net,packet_list,0);
                if (ret_val == -1) {
                   if (Network_recover_FromFailures(net)) {
                        recov_parent = Network_get_ParentNode(net);
                        /* We are first ones here, recover */
                        if(recov_parent == orig_parent) {
                            Network_recover_FromParentFailure(net);
                        }
                        /* Try receiving again */
                        if (Network_recv_PacketsFromParent(net, packet_list,
                                    blocking) == -1) {
                            mrn_dbg(3, mrn_printf(FLF, stderr, 
                                        "recv() failed twice, return -1\n"));
                            no_lock = 1;
                            goto recv_clean_up;
                        }
                    }
                    else {
                        ret_val = -1;
                        no_lock = 1;
                        goto recv_clean_up;
                    }
               }
            }
            Network_lock(net, RECV_SYNC);
            break;
        } else {
            if(!blocking) {
                mrn_dbg(5, mrn_printf(FLF, stderr, "No packets read!\n"));
                ret_val = 0;
                goto recv_clean_up;
            }
            // While thread listening down here too?
            mrn_dbg(5, mrn_printf(FLF, stderr,
                    "Waiting on condition THREAD_LISTENING.\n"));
            Network_monitor_wait(net, RECV_SYNC, THREAD_LISTENING);

            mrn_dbg(5, mrn_printf(FLF, stderr,
                    "Woke up from waiting on THREAD_LISTENING.\n"));

            /* I've awoken with the lock. Check if I have a packet waiting */
            if(stream != NULL) {
                ready_packet = Stream_get_IncomingPacket(stream);
            } else {
                ready_packet = Network_recv_stream_check(net);
            }

        }

    }
    ret_val = 1;

    if(ready_packet != NULL) {
        /* Data was ready on thread wake up */
        pushBackElement(packet_list, ready_packet);
    }

    if( packet_list->size == 0 ) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "No packets read!\n"));
        ret_val = 0;
        goto recv_clean_up;
    }
    else {
        mrn_dbg(5, mrn_printf(FLF, stderr, "Calling proc_packets()\n"));

        if( ChildNode_proc_PacketsFromParent(net->local_back_end_node, packet_list) ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "proc_packets() failed\n"));
            ret_val = -1;
            goto recv_clean_up;
        }
    }

    // if we get here, we have found data to return
    mrn_dbg(5, mrn_printf(FLF, stderr, 
                          "Found/processed packets! Returning\n"));

recv_clean_up:
    delete_vector_t( packet_list );
    if(listened) {
        net->thread_listening--;
        assert(net->thread_listening == 0);
        mrn_dbg(5, mrn_printf(FLF, stderr,
                "Broadcasting condition THREAD_LISTENING.\n"));
        Network_monitor_broadcast(net, RECV_SYNC, THREAD_LISTENING);
    }

    if(!no_lock) {
        Network_unlock(net, RECV_SYNC);
    }
    mrn_dbg_func_end();

    return ret_val;
}

Packet_t* Network_recv_stream_check(Network_t* net)
{
    int packet_found;  
    unsigned int start_iter;
    Stream_t* cur_stream;
    Packet_t* cur_packet = NULL;

    mrn_dbg_func_begin();

    /* mrn_dbg(5, mrn_printf(FLF, stderr, "calling Network_have_Streams()\n")); */
    if( Network_have_Streams(net) ) {
    
        packet_found = false;

        start_iter = net->stream_iter;
    
        Network_lock(net, NET_SYNC);

        do {
            // get the Stream associated with the current stream_iter,
            // which is an index into the keys array
            cur_stream = (Stream_t*)get_val(net->streams, 
                                            net->streams->keys[net->stream_iter]);
            if( cur_stream != NULL ) {
                mrn_dbg(5, mrn_printf(FLF, stderr,
                                      "Checking for packets on stream[%d] id=%u ...\n",
                                      net->stream_iter, cur_stream->id));
                cur_packet = Stream_get_IncomingPacket(cur_stream);
                if (cur_packet != NULL) 
                    packet_found = true;
                mrn_dbg(5, mrn_printf(FLF, stderr, "... %s!\n", 
                                      (packet_found ? "found" : "not found" )));
            }
            net->stream_iter++;
            if (net->stream_iter >= net->streams->size) {
                // wrap around to the beginning of the map
                net->stream_iter = 0;
                mrn_dbg(5, mrn_printf(FLF, stderr,
                                      "wraparound, setting stream_iter to zero\n"));
            }
        } while((start_iter != net->stream_iter) && !packet_found);

        Network_unlock(net, NET_SYNC);
    }
    
    return cur_packet;
}


int Network_recv_nonblock(Network_t* net, int *otag, 
                          Packet_t* opacket, Stream_t** ostream)
{
    int ret_val;
    Packet_t* cur_packet = NULL;
get_packet_from_stream_label:
    cur_packet = Network_recv_stream_check(net);
    if( cur_packet != NULL ) {
        *otag = cur_packet->tag;
        *ostream = Network_get_Stream(net, cur_packet->stream_id);
        assert(*ostream);
        *opacket = *cur_packet;
        mrn_dbg(5, mrn_printf(FLF, stderr, "cur_packet tag: %d, fmt: %s\n", 
                              cur_packet->tag, cur_packet->fmt_str));
        
        // cur_packet fields now owned by opacket, just free the Packet_t
        free( cur_packet );
        return 1;
    }
    
    ret_val = Network_recv_internal(net, NULL, false);
    if(ret_val == 1) {
        goto get_packet_from_stream_label;
    }

    return ret_val;
}

int Network_recv(Network_t* net, int *otag, 
                 Packet_t* opacket, Stream_t** ostream)
{
    Packet_t* cur_packet = NULL;
    int retval;

    mrn_dbg_func_begin();

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

        // cur_packet fields now owned by opacket, just free the Packet_t
        free( cur_packet );
        return 1;
    }

    // No packets are already in the stream
    // check whether there is data waiting to be read on our socket
    mrn_dbg(5, mrn_printf(FLF, stderr, 
            "No packets waiting in stream, checking for data on socket\n"));
    retval = Network_recv_internal(net, NULL, true);


    if( retval == -1 ) {
        mrn_dbg(3, mrn_printf(FLF, stderr, "Network_recv_internal() failed\n"));
        return -1;
    }
    else if( retval == 1 ) {
        // go back if we found a packet
        mrn_dbg(3, mrn_printf(FLF, stderr, "Network_recv_internal() found packet!\n"));
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
    PeerNode_t *tmp_parent = Network_get_ParentNode(net);
    int ret_val = 0;
    if(tmp_parent != NULL) {
        ret_val = PeerNode_has_data(tmp_parent);
    }
    return ret_val;
}
int Network_recv_EventFromParent(Network_t * net, vector_t* opackets, bool_t blocking) {
    Network_lock(net,PARENT_SYNC);
    PeerNode_t *tmp_parent = Network_get_ParentNode(net);
    int ret_val = -1;
    if(tmp_parent != NULL) {
        ret_val = PeerNode_recv_event(tmp_parent, opackets, blocking);
    }
    Network_unlock(net,PARENT_SYNC);
    return ret_val;
}
int Network_recv_PacketsFromParent(Network_t* net, vector_t* opackets, bool_t blocking)
{
    PeerNode_t *tmp_parent = Network_get_ParentNode(net);
    int ret_val = -1;
    if(tmp_parent != NULL) {
        ret_val = PeerNode_recv(tmp_parent, opackets, blocking);
    }
    return ret_val;
}

void Network_shutdown_Network(Network_t* net)
{
    unsigned int i;
    Stream_t * cur_stream;
    
    mrn_dbg_func_begin();
    Network_lock(net, NET_SYNC);

    if( ! net->_was_shutdown ) { 
        net->_was_shutdown = 1;

        Network_reset_Topology(net, NULL);

        // close streams
        for (i = 0; i < net->streams->size; i++) {
            cur_stream = (Stream_t*)get_val(net->streams, net->streams->keys[i]);
            if( cur_stream != NULL )
                cur_stream->_was_closed = 1;
        }
    }

    Network_unlock(net, NET_SYNC);
    mrn_dbg_func_end();
}

int Network_reset_Topology(Network_t* net, char* itopology)
{
    SerialGraph_t* sg = NULL;
 
    mrn_dbg_func_begin();

    sg = new_SerialGraph_t( itopology );
    if( itopology != NULL )
    	free( itopology );

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
    Network_lock(net, NET_SYNC);

    stream = new_Stream_t(net, iid, ibackends, inum_backends,
                          (unsigned)ius_filter_id, 
                          (unsigned)isync_filter_id, 
                          (unsigned)ids_filter_id);
    insert(net->streams, (int)iid, stream);

    Network_unlock(net, NET_SYNC);
    mrn_dbg_func_end();
    return stream;
}

int Network_remove_Node(Network_t* net, Rank ifailed_rank, int UNUSED(iupdate))
{
    mrn_dbg_func_begin();
    Network_lock(net, NET_SYNC);

    mrn_dbg(3, mrn_printf(FLF, stderr, "Deleting PeerNode: node[%u] ...\n", ifailed_rank));
    Network_delete_PeerNode(net, ifailed_rank);

    mrn_dbg(3, mrn_printf(FLF, stderr, "Removing from Topology: node[%u] ...\n", 
                          ifailed_rank));
    NetworkTopology_remove_Node(net->network_topology, ifailed_rank);

    Network_unlock(net, NET_SYNC);
    mrn_dbg_func_end();
    return true;
}

int Network_delete_PeerNode(Network_t* net, Rank irank)
{
    unsigned int iter;
    Node_t* cur_child;
    Network_lock(net, PARENT_SYNC);
    
    if ( (net->parent != NULL) && (net->parent->rank == irank) ) {
        free( net->parent );
        net->parent = NULL;
        Network_unlock(net, PARENT_SYNC);
        return true;
    }

    for (iter = 0; iter < net->children->size; iter++) {
        cur_child = (Node_t*)(net->children->vec[iter]);
        if (NetworkTopology_Node_get_Rank(cur_child) == irank) {
            net->children = eraseElement(net->children, cur_child);
            Network_unlock(net, PARENT_SYNC);
            return true; 
        }
    } 

    Network_unlock(net, PARENT_SYNC);
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
    Stream_t* ret;
    Network_lock(net, NET_SYNC);

    ret = (Stream_t*)get_val(net->streams, (int)iid);
    if (ret == NULL)
        mrn_dbg(5, mrn_printf(FLF, stderr, "ret == NULL\n"));

    Network_unlock(net, NET_SYNC);
    return ret;
}

void Network_delete_Stream(Network_t * net, unsigned int iid)
{
    int key;
    mrn_dbg_func_begin();
    Network_lock(net, NET_SYNC);

    /* if we're deleting the iter, set to the next element */
    key = (int) iid;
    if( key == net->streams->keys[net->stream_iter] )
        net->stream_iter++;

    net->streams = erase(net->streams, key);

    // wrap around to the beginning of the map, if necessary
    if( net->stream_iter >= net->streams->size )
        net->stream_iter = 0;

    Network_unlock(net, NET_SYNC);
    mrn_dbg_func_end();
}

int Network_is_UserStreamId( unsigned int id )
{
    if( (id >= USER_STRM_BASE_ID) || (id < CTL_STRM_ID) )
        return 1;
    return 0;
}

int Network_send_PacketToParent(Network_t* net, Packet_t* ipacket)
{
    PeerNode_t *send_parent;
    PeerNode_t *recov_parent;
    mrn_dbg_func_begin();
    send_parent = Network_get_ParentNode(net);
    if (PeerNode_sendDirectly(send_parent, ipacket) == -1) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "upstream.send() failed\n"));
        if (Network_recover_FromFailures(net)) {
            mrn_dbg(1, mrn_printf(FLF, stderr, 
                                  "assume parent failure, try one more time\n"));
            recov_parent = Network_get_ParentNode(net);
            if(recov_parent == send_parent) {
                Network_recover_FromParentFailure(net); 
                send_parent = Network_get_ParentNode(net);
            } else {
                send_parent = recov_parent;
            }
            if (PeerNode_sendDirectly(send_parent, ipacket) == -1) {
                mrn_dbg(1, mrn_printf(FLF, stderr, "upstream.send() failed, again\n"));
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
                                 int is_parent,
                                 int is_internal)
{
    /* Assumes we possess network lock */
    PeerNode_t* node = new_PeerNode_t(network, ihostname, iport, 
                                      irank, is_parent, is_internal);

    mrn_dbg(5, mrn_printf(FLF, stderr, "new peer node: %s:%d (%p) \n", 
                          node->hostname, node->rank, node));

    pushBackElement(network->children, node); 

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
    PeerNode_t* ret_parent;
    mrn_dbg_func_begin();
    Network_lock(net, PARENT_SYNC);
    ret_parent = NULL;
    while( net->parent == NULL && (!Network_is_ShutDown(net)) ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr,
                    "Waiting on PARENT_NODE_AVAILABLE\n"));

        Network_monitor_wait(net, PARENT_SYNC, PARENT_NODE_AVAILABLE);

        if(net->parent != NULL) {
            mrn_dbg( 3, mrn_printf(FLF, stderr,
                        "Woke up from waiting on PARENT_NODE_AVAILABLE\n"));
        }
    }

    ret_parent = net->parent;

    Network_unlock(net, PARENT_SYNC);
    mrn_dbg_func_end();
    return ret_parent;
}

void Network_set_ParentNode(Network_t* net, PeerNode_t* ip) 
{
    mrn_dbg_func_begin();
    Network_lock(net, PARENT_SYNC);
    net->parent = ip;

    if (net->parent != NULL) {
        Network_monitor_broadcast(net, PARENT_SYNC, PARENT_NODE_AVAILABLE);
        mrn_dbg(3, mrn_printf(FLF, stderr, "Parent node available\n"));
    }

    Network_unlock(net, PARENT_SYNC);
    mrn_dbg_func_end();
}

int Network_has_ParentFailure(Network_t* net) 
{
    struct timeval zeroTimeout;
    fd_set rfds;
    int sret;
    int ret = 0;

    Network_lock(net, PARENT_SYNC);

    zeroTimeout.tv_sec = 0;
    zeroTimeout.tv_usec = 0;

    // set up file descriptor set for the poll
    FD_ZERO(&rfds);
    FD_SET(net->parent->event_sock_fd, &rfds);

    sret = select(net->parent->event_sock_fd + 1, &rfds, NULL, NULL, &zeroTimeout);
    if (sret == -1) {
        // select error
        perror("select()");
        ret = -1;
    } else if (sret == 1) {
        if (FD_ISSET(net->parent->event_sock_fd, &rfds)) { 
            ret = 1;
        }
    }
   
    Network_unlock(net, PARENT_SYNC);
    return ret;
}

int Network_recover_FromParentFailure(Network_t* net) 
{

    
    Timer_t* new_parent_timer = new_Timer_t();
    Timer_t* cleanup_timer = new_Timer_t();
    Timer_t* connection_timer = new_Timer_t();
    Timer_t* overall_timer = new_Timer_t();
    Rank failed_rank;
    Node_t* new_parent_node;
    char* new_parent_name;
    PeerNode_t* new_parent;
    BackEndNode_t* local_node;
    Rank new_parent_rank;

    mrn_dbg_func_begin();
    
    /* Check if someone is already recovering */
    Network_lock(net, PARENT_SYNC);
    if(!net->thread_recovering) {
        net->thread_recovering++;
        assert(net->thread_recovering == 1);
    } else {
        while(net->thread_recovering) {
            Network_monitor_wait(net, PARENT_SYNC, PARENT_NODE_AVAILABLE);
        }
        Network_unlock(net, PARENT_SYNC);
        /* We assume recovery that made us block executed correctly */
        return 0;
    }
    Network_unlock(net, PARENT_SYNC);

    /* No one else is recovering...we can recover */
    Network_lock(net, NET_SYNC);

    PeerNode_mark_Failed(Network_get_ParentNode(net));
    failed_rank = PeerNode_get_Rank(Network_get_ParentNode(net));
    Network_set_ParentNode(net, NULL);

    mrn_dbg(3, mrn_printf(FLF, stderr, "Recovering from parent[%d]'s failure\n", 
                          failed_rank));

    NetworkTopology_print(Network_get_NetworkTopology(net), NULL);

    local_node = Network_get_LocalChildNode(net);

    //Step 1: Compute new parent
    Timer_start(overall_timer);
    Timer_start(new_parent_timer);
    new_parent_node = NetworkTopology_find_NewParent(Network_get_NetworkTopology(net), 
                                                     Network_get_LocalRank(net), 
                                                     0, ALG_WRS);
    if (!new_parent_node) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Can't find new parent! Exiting...\n"));
        exit(-1);
    }

    new_parent_rank = NetworkTopology_Node_get_Rank(new_parent_node);

    mrn_dbg(1, mrn_printf(FLF, stderr, "RECOVERY: NEW PARENT: %s:%d:%d\n", 
                          NetworkTopology_Node_get_HostName(new_parent_node),
                          NetworkTopology_Node_get_Port(new_parent_node),
                          new_parent_rank));

    new_parent_name = NetworkTopology_Node_get_HostName(new_parent_node);
 
    new_parent = Network_new_PeerNode(net, 
                                      new_parent_name,
                                      NetworkTopology_Node_get_Port(new_parent_node),
                                      new_parent_rank,
                                      true, true);

    Timer_stop(new_parent_timer); 
   
    // Step 2. Establish data connection with new parent
    Timer_start(connection_timer);
    mrn_dbg(3, mrn_printf(FLF, stderr, "Establish new data connection ...\n"));

    mrn_dbg(5, mrn_printf(FLF, stderr, "local child node rank = %d\n", 
                          local_node->myrank));

    if (ChildNode_init_newChildDataConnection(local_node, new_parent,
                                              failed_rank) == -1) {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "ChildNode_init_newChildDataConnection() failed\n"));
        return -1;
    }

    // Step 3. Establish event detection connnection with new Parent
    if (ChildNode_init_newChildEventConnection(local_node, new_parent) == -1) {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "ChildNode_init_newChildEventConnection() failed\n"));
    }
        
    mrn_dbg(3, mrn_printf(FLF, stderr, "New event connection established.\n"));
    Timer_stop(connection_timer);

    // Step 4. Propagate filter state for active streams to new parent
    // No filtering is done at BEs
    
    // Step 5. Update local topology and data structures
    Timer_start(cleanup_timer);
    mrn_dbg(3, mrn_printf(FLF, stderr, "Updating local structures ..\n"));
    // remove node, but don't update datastructs since following procedure will
    Network_remove_Node(net, failed_rank, false);
    Network_change_Parent(net, Network_get_LocalRank(net), new_parent_rank);
    Timer_stop(cleanup_timer);
    Timer_stop(overall_timer);
    NetworkTopology_print(Network_get_NetworkTopology(net), NULL);
    
    /* Update recovering state and parent node */
    Network_lock(net, PARENT_SYNC);
    net->thread_recovering--;
    assert(!net->thread_recovering);
    Network_set_ParentNode(net, new_parent);
    Network_unlock(net, PARENT_SYNC);

    Network_unlock(net, NET_SYNC);

    if( NULL != new_parent_timer ) free( new_parent_timer );
    if( NULL != cleanup_timer ) free( cleanup_timer );
    if( NULL != connection_timer ) free( connection_timer );
    if( NULL != overall_timer ) free( overall_timer );

    mrn_dbg_func_end();

    return 0;
}

char* Network_get_LocalSubTreeStringPtr(Network_t* net)
{
    return NetworkTopology_get_LocalSubTreeStringPtr(net->network_topology);
}

void Network_set_DebugLogDir( char* value )
{
    if( NULL != value )
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
    set_DebugLevel(CUR_OUTPUT_LEVEL);
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

    mrn_dbg_func_begin();
    Network_lock(net, NET_SYNC);
	
    p = (Packet_t*) malloc(sizeof(Packet_t));
    assert(p);

    while( ! net->_was_shutdown ) {

        if( Network_recv(net, &tag, p, &stream) != 1 ) {
            mrn_dbg(3, mrn_printf(FLF, stderr, "Network_recv() failure\n"));
            break;
        }

    }

    free(p);

    Network_unlock(net, NET_SYNC);
    mrn_dbg_func_end();
}
