/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__message_h)
#define __message_h 1

#include <stdio.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <sys/socket.h>

#include "mrnet/Network.h"
#include "mrnet/Packet.h"
#include "pdr.h"
#include "vector.h"

typedef struct {
   Packet_t* packet;
}Message_t ;

 Message_t* new_Message_t();

int Message_recv(Network_t* net, int sock_fd,  /* Packet_t* packet_in */ vector_t* packets_in, Rank iinlet_rank);

int Message_send(Network_t* net, Message_t* msg_out, int sock_fd);

int MRN_write(Network_t* net, int ifd, /*const*/ void *ibuf, int ibuf_len);

int MRN_read(Network_t* net, int fd, void *buf, int count);

#endif /* __message_h */
