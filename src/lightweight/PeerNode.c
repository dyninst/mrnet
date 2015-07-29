/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "PeerNode.h"
#include "mrnet_lightweight/Error.h"
#include "mrnet_lightweight/Network.h"
#include "xplat_lightweight/vector.h"

PeerNode_t* new_PeerNode_t(Network_t* inetwork, 
                            char* ihostname,
                            Port iport,
                            Rank irank,
                            int is_parent,
                            int is_internal_node)
{

  struct PeerNode_t* peer_node = (PeerNode_t*)malloc(sizeof(struct PeerNode_t));
  assert( peer_node != NULL );

  peer_node->net = inetwork;
  peer_node->hostname = ihostname;
  peer_node->rank = irank;
  peer_node->port = iport;
  peer_node->data_sock_fd = InvalidSocket;
  peer_node->event_sock_fd = InvalidSocket;
  peer_node->is_internal_node = is_internal_node;
  peer_node->is_parent = is_parent;
  peer_node->available = true;

#ifdef MRNET_LTWT_THREADSAFE  
    peer_node->send_mutex = Mutex_construct();
    peer_node->recv_monitor = Monitor_construct();
#endif

  return peer_node;
}

#ifdef MRNET_LTWT_THREADSAFE  

void _PeerNode_send_lock(PeerNode_t* peer)
{
    int retval;
    mrn_dbg(3, mrn_printf(FLF, stderr, "PeerNode_send_lock %d\n", peer->rank));
    retval = Mutex_Lock( peer->send_mutex );
    if( retval != 0 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "PeerNode_send_lock failed to acquire mutex: %s\n",
                              strerror(retval)));
    }
}

void _PeerNode_send_unlock(PeerNode_t* peer)
{
    int retval;
    mrn_dbg(3, mrn_printf(FLF, stderr, "PeerNode_send_unlock %d\n", peer->rank));
    retval = Mutex_Unlock( peer->send_mutex );
    if( retval != 0 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "PeerNode_send_unlock failed to release mutex: %s\n",
                              strerror(retval)));
    }
}

int _PeerNode_recv_lock(PeerNode_t* peer, int blocking)
{
    int retval;
    int dbg_level;
    mrn_dbg(3, mrn_printf(FLF, stderr, "PeerNode_recv_lock %d\n", peer->rank));
    if(blocking) {
        dbg_level = 1;
        retval = Monitor_Lock( peer->recv_monitor );
    } else {
        dbg_level = 3;
        retval = Monitor_Trylock( peer->recv_monitor );
    }
    if( retval != 0 ) {
        mrn_dbg(dbg_level, mrn_printf(FLF, stderr, 
                              "PeerNode_recv_lock failed to acquire mutex(%d): %s\n",
                              retval, strerror(retval)));
    }
    return retval;
}

void _PeerNode_recv_unlock(PeerNode_t* peer)
{
    int retval;
    mrn_dbg(3, mrn_printf(FLF, stderr, "PeerNode_recv_unlock %d\n",
                          peer->rank));
    retval = Monitor_Unlock( peer->recv_monitor );
    if( retval != 0 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "PeerNode_recv_unlock failed to release mutex"
                              "(%d): %s\n",
                              retval, strerror(retval)));
    }
}

# define PeerNode_send_lock(p) _PeerNode_send_lock(p)
# define PeerNode_send_unlock(p) _PeerNode_send_unlock(p)
# define PeerNode_recv_lock(p,b) _PeerNode_recv_lock(p,b)
# define PeerNode_recv_unlock(p) _PeerNode_recv_unlock(p)

#else // !def MRNET_LTWT_THREADSAFE  

# define PeerNode_send_lock(p) do{}while(0)
# define PeerNode_send_unlock(p) do{}while(0)
# define PeerNode_recv_lock(p,b) 0
# define PeerNode_recv_unlock(p) do{}while(0)

#endif

Rank PeerNode_get_Rank(PeerNode_t* node)
{
    return node->rank;
}

int PeerNode_connect_DataSocket(PeerNode_t* parent, int num_retry) 
{
    mrn_dbg(3, mrn_printf(FLF, stderr, 
                          "Creating data conection to (%s:%d) ...\n", 
                          parent->hostname, parent->port));

    if( connectHost(&(parent->data_sock_fd), parent->hostname, 
                    parent->port, num_retry) == -1 ) { 
        error (ERR_SYSTEM, parent->net->local_rank, "connectHost() failed" );
        mrn_dbg(1, mrn_printf(FLF, stderr, "connectHost() failed\n"));
        return -1;
    }

    mrn_dbg(3, mrn_printf(FLF, stderr, "new data socket %d\n", 
                          parent->data_sock_fd));
    return 0;
}

int PeerNode_connect_EventSocket(PeerNode_t* parent, int num_retry) 
{
    mrn_dbg(3, mrn_printf(FLF, stderr, 
                          "Creating event connection to (%s:%d) ...\n", 
                          parent->hostname, parent->port));

    if( connectHost(&(parent->event_sock_fd), parent->hostname, 
                    parent->port, num_retry) == -1 ) { 
        error (ERR_SYSTEM, parent->net->local_rank, "connectHost() failed" );
        mrn_dbg(1, mrn_printf(FLF, stderr, "connectHost() failed\n"));
        return -1;
    }

    mrn_dbg(3, mrn_printf(FLF, stderr, "new event socket %d\n", 
                          parent->event_sock_fd));
    return 0;
}

// don't use this one--intended for non-blocking send
int PeerNode_send(PeerNode_t* UNUSED(peer), /*const*/ Packet_t* UNUSED(ipacket))
{
    mrn_dbg(1, mrn_printf(FLF, stderr, 
                          "PeerNode_send is not intended for blocking send\n"));
    return -1;
}

int PeerNode_sendDirectly (PeerNode_t* peer, /*const*/ Packet_t* ipacket) 
{
    int retval = 0;

    mrn_dbg_func_begin();
    PeerNode_send_lock(peer);

    peer->msg_out.packet = ipacket;

    mrn_dbg(3, mrn_printf(FLF, stderr, "node[%d].msg(%p).add_packet()\n", 
                          peer->rank, &(peer->msg_out)));
  
    if( Message_send(&(peer->msg_out), peer->data_sock_fd) == -1 ) { 
        mrn_dbg(1, mrn_printf(FLF, stderr, "Message_send() failed\n"));
        retval = -1;
    }

    PeerNode_send_unlock(peer);
    mrn_dbg_func_end();

    return retval;
}

int PeerNode_has_data(PeerNode_t* node)
{
    struct timeval zeroTimeout;
    fd_set rfds;
    int sret;

    mrn_dbg_func_begin();
    if(PeerNode_recv_lock(node, 0) != 0) {
       return false;
    }

    zeroTimeout.tv_sec = 0;
    zeroTimeout.tv_usec = 0;
  
    // set up file descriptor set for the poll
    FD_ZERO(&rfds);
    FD_SET(node->data_sock_fd, &rfds);

    // check if data is available
    sret = select(node->data_sock_fd + 1, &rfds, NULL, NULL, &zeroTimeout);
    if (sret == -1) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "select() failed\n"));
        PeerNode_recv_unlock(node);
        mrn_dbg_func_end();
        return false;
    }

    PeerNode_recv_unlock(node);

    // We only put one descriptor in the read set. Therefore, if the read
    // value from select() is 1, that descriptor has data available.
    if (sret == 1) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "select(): data to be read.\n"));
        mrn_dbg_func_end();
        return true;
    }

    mrn_dbg(3, mrn_printf(FLF, stderr, "Leaving PeerNode_has_data(). No data available\n"));
    mrn_dbg_func_end();
    return false;
}

int PeerNode_has_event_data(PeerNode_t * node) {
    struct timeval zeroTimeout;
    fd_set rfds;
    int sret;

    mrn_dbg_func_begin();

    zeroTimeout.tv_sec = 0;
    zeroTimeout.tv_usec = 0;
  
    // set up file descriptor set for the poll
    FD_ZERO(&rfds);
    FD_SET(node->event_sock_fd, &rfds);

    // check if data is available
    sret = select(node->event_sock_fd + 1, &rfds, NULL, NULL, &zeroTimeout);
    if (sret == -1) {
        mrn_dbg_func_end();
        return 0;
    }
    return 1;
}

int PeerNode_recv_event(PeerNode_t* node, vector_t* packet_list, bool_t blocking)
{
    int msg_ret = 0; 
    if(  PeerNode_has_event_data(node) ) {
        msg_ret = Message_recv(node->event_sock_fd, packet_list, node->rank);
    }
    blocking = 1;
    return msg_ret;
}

int PeerNode_recv(PeerNode_t* node, vector_t* packet_list, bool_t blocking)
{
    int msg_ret = 0;
    mrn_dbg_func_begin();

    if( 0 != PeerNode_recv_lock(node, blocking) ) {
        return msg_ret;
    }
    if( blocking || PeerNode_has_data(node) ) {
        msg_ret = Message_recv(node->data_sock_fd, packet_list, node->rank);
    }

    PeerNode_recv_unlock(node);

    mrn_dbg_func_end();
    return msg_ret;
}

int PeerNode_flush(PeerNode_t* UNUSED(peer))
{
    int retval = 0;
    return retval;
}

void PeerNode_mark_Failed(PeerNode_t* node)
{
    node->available = false;
}
