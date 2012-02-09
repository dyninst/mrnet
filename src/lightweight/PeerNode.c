/****************************************************************************
 *  Copyright 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <assert.h>
#include <string.h>
#include "mrnet_lightweight/Error.h"
#include "mrnet_lightweight/Network.h"
#include "PeerNode.h"
#include "utils_lightweight.h"
#include "vector.h"

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
  peer_node->data_sock_fd = 0;
  peer_node->event_sock_fd = 0;
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
inline void PeerNode_send_lock(PeerNode_t* peer)
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

inline void PeerNode_send_unlock(PeerNode_t* peer)
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

inline int PeerNode_recv_lock(PeerNode_t* peer, int blocking)
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

inline void PeerNode_recv_unlock(PeerNode_t* peer)
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
#else

inline void PeerNode_send_lock(PeerNode_t* UNUSED(peer))
{

}

inline void PeerNode_send_unlock(PeerNode_t* UNUSED(peer))
{

}

inline int PeerNode_recv_lock(PeerNode_t* UNUSED(peer), int UNUSED(blocking))
{
    return 0;
}


inline void PeerNode_recv_unlock(PeerNode_t* UNUSED(peer))
{
}
#endif

Rank PeerNode_get_Rank(PeerNode_t* node)
{
    return node->rank;
}

int PeerNode_connect_DataSocket(PeerNode_t* parent) 
{
    mrn_dbg(3, mrn_printf(FLF, stderr, 
                          "Creating data conection to (%s:%d) ...\n", 
                          parent->net->local_hostname, parent->net->local_port));

    if( connectHost(&(parent->data_sock_fd), parent->hostname, 
                    parent->port, -1) == -1 ) { 
        error (ERR_SYSTEM, parent->net->local_rank, "connectHost() failed" );
        mrn_dbg(1, mrn_printf(FLF, stderr, "connectHost() failed\n"));
        return -1;
    }

    mrn_dbg(3, mrn_printf(FLF, stderr, "new data socket %d\n", 
                          parent->data_sock_fd));

    return 0;
}

int PeerNode_connect_EventSocket(PeerNode_t* parent) 
{
    mrn_dbg(3, mrn_printf(FLF, stderr, 
                          "Creating event conection to (%s:%d) ...\n", 
                          parent->net->local_hostname, parent->net->local_port));

    if( connectHost(&(parent->event_sock_fd), parent->hostname, 
                    parent->port, -1) == -1 ) { 
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

int PeerNode_recv(PeerNode_t* node, vector_t* packet_list, bool_t blocking)
{
    int msg_ret = 0;
    mrn_dbg_func_begin();

    if((PeerNode_recv_lock(node, blocking) != 0)) {
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
