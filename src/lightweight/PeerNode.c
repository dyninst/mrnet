/****************************************************************************
 *  Copyright 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <assert.h>
#include "mrnet_lightweight/Error.h"
#include "mrnet_lightweight/Network.h"
#include "PeerNode.h"
#include "utils_lightweight.h"
#include "vector.h"

PeerNode_t* new_PeerNode_t(Network_t* inetwork, 
                            char* ihostname,
                            Port iport,
                            Rank irank,
                            int iis_parent,
                            int iis_internal_node)
{

  struct PeerNode_t* peer_node = (PeerNode_t*)malloc(sizeof(struct PeerNode_t));
  assert(peer_node);
  peer_node->net = inetwork;
  peer_node->hostname = ihostname;
  peer_node->rank = irank;
  peer_node->port = iport;
  peer_node->data_sock_fd = 0;
  peer_node->event_sock_fd = 0;
  peer_node->is_internal_node = iis_internal_node;
  peer_node->is_parent = iis_parent;
  peer_node->available = true;

  return peer_node;
}

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

// don't use this one--intended for non-blocking send
int PeerNode_send(PeerNode_t* peer, /*const*/ Packet_t* ipacket)
{
    mrn_dbg(1, mrn_printf(FLF, stderr, 
                          "PeerNode_send is not intended for blocking send\n"));
    return -1;
}

int PeerNode_sendDirectly (PeerNode_t* peer, /*const*/ Packet_t* ipacket) 
{
    int retval = 0;

    mrn_dbg_func_begin();

    peer->msg_out.packet = ipacket;

    mrn_dbg(3, mrn_printf(FLF, stderr, "node[%d].msg(%p).add_packet()\n", 
                          peer->rank, &(peer->msg_out)));
  
    if( Message_send(&(peer->msg_out), peer->data_sock_fd) == -1 ) { 
        mrn_dbg(1, mrn_printf(FLF, stderr, "Message_send() failed\n"));
        retval = -1;
    }
    mrn_dbg_func_end();

    return retval;
}

int PeerNode_has_data(PeerNode_t* node)
{
    struct timeval zeroTimeout;
    fd_set rfds;
    int sret;

    zeroTimeout.tv_sec = 0;
    zeroTimeout.tv_usec = 0;
  
    // set up file descriptor set for the poll
    FD_ZERO(&rfds);
    FD_SET(node->data_sock_fd, &rfds);

    // check if data is available
    sret = select(node->data_sock_fd + 1, &rfds, NULL, NULL, &zeroTimeout);
    if (sret == -1) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "select() failed\n"));
        return false;
    }

    // We only put one descriptor in the read set. Therefore, if the read
    // value from select() is 1, that descriptor has data available.
    if (sret == 1) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "select(): data to be read.\n"));
        return true;
    }

    mrn_dbg(3, mrn_printf(FLF, stderr, "Leaving PeerNode_has_data(). No data available\n"));
    return false;
}

int PeerNode_recv(PeerNode_t* node, /* Packet_t* packet */ vector_t* packet_list)
{
    return Message_recv(node->data_sock_fd, packet_list, node->rank);
}

int PeerNode_flush(PeerNode_t* peer)
{
    int retval = 0;
    return retval;
}

void PeerNode_mark_Failed(PeerNode_t* node)
{
    node->available = false;
}
