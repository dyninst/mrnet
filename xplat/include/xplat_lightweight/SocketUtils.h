/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: SocketUtils.h,v 3.1 2012/01/20 17:44:17 samanas Exp $
#ifndef XPLAT_SOCKETUTILS_H
#define XPLAT_SOCKETUTILS_H

#include "xplat_lightweight/Types.h"

#ifndef MSG_NOSIGNAL
# define MSG_NOSIGNAL 0
#endif

extern const XPlat_Socket InvalidSocket;
extern const XPlat_Port InvalidPort;

bool_t XPlat_SocketUtils_Connect( const char* hostname,
                                  const XPlat_Port port,
                                  XPlat_Socket* sock, 
                                  unsigned num_retry );

bool_t XPlat_SocketUtils_CreateListening( XPlat_Socket* sock, 
                                          XPlat_Port* port, 
                                          bool_t nonblock );

bool_t XPlat_SocketUtils_AcceptConnection( XPlat_Socket listen_sock, 
                                           XPlat_Socket* connected_sock,
                                           int timeout_sec,
                                           bool_t nonblock );

bool_t XPlat_SocketUtils_GetPort( const XPlat_Socket sock, 
                                  XPlat_Port* port );

bool_t XPlat_SocketUtils_SetBlockingMode( XPlat_Socket sock,
                                          bool_t blocking );

bool_t XPlat_SocketUtils_SetOption( XPlat_Socket sock,
                                    int level,
                                    int option,
                                    void* optval,
                                    socklen_t optsz );

bool_t XPlat_SocketUtils_Close( XPlat_Socket sock );

typedef struct
{
  size_t len;
  char* buf;
} XPlat_NCBuf_t;

XPlat_NCBuf_t* XPlat_new_NCBuf();

ssize_t XPlat_SocketUtils_Send( XPlat_Socket s, 
                                XPlat_NCBuf_t* bufs, 
                                unsigned int nBufs );

ssize_t XPlat_SocketUtils_Recv( XPlat_Socket s, 
                                XPlat_NCBuf_t* bufs, 
                                unsigned int nBufs );

ssize_t XPlat_SocketUtils_send( XPlat_Socket s, 
                                const void *buf, 
                                size_t count );

ssize_t XPlat_SocketUtils_recv( XPlat_Socket s, 
                                void *buf, 
                                size_t count );

#endif // XPLAT_SOCKETUTILS_H
