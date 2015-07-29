/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__message_h)
#define __message_h 1

#include <stdio.h>
#include <stdlib.h>

#include "mrnet_lightweight/Network.h"
#include "mrnet_lightweight/Packet.h"
#include "pdr.h"
#include "xplat_lightweight/vector.h"

#ifndef os_windows
#include <sys/uio.h>
#include <sys/socket.h>
#endif

typedef struct {
   Packet_t* packet;
} Message_t ;

Message_t* new_Message_t();

int Message_recv(XPlat_Socket sock_fd, vector_t* packets_in, Rank iinlet_rank);
int Message_send(Message_t* msg_out, XPlat_Socket sock_fd);

ssize_t MRN_send(XPlat_Socket sock_fd, void *buf, size_t buf_len);
ssize_t MRN_recv(XPlat_Socket sock_fd, void *buf, size_t count);

#endif /* __message_h */
