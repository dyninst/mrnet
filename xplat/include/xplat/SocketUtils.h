/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: SocketUtils.h,v 1.2 2007/01/24 19:33:51 darnold Exp $
#ifndef XPLAT_SOCKETUTILS_H
#define XPLAT_SOCKETUTILS_H

#include "xplat/Types.h"

#include <string>
#include <vector>

#ifndef MSG_NOSIGNAL
# define MSG_NOSIGNAL 0
#endif

enum SDHowType {XPLAT_SHUT_RD, XPLAT_SHUT_WR, XPLAT_SHUT_RDWR};

namespace XPlat
{

namespace SocketUtils
{
    extern const XPlat_Socket InvalidSocket;
    extern const XPlat_Port InvalidPort;


    bool Connect( const std::string &hostname,
                  const XPlat_Port port,
                  XPlat_Socket& sock, 
                  const unsigned num_retry=0 );
    
    bool CreateListening( XPlat_Socket& sock, 
                          XPlat_Port& port, 
                          bool nonblock=false );
 
    bool AcceptConnection( const XPlat_Socket listen_sock, 
                           XPlat_Socket& connected_sock,
                           int timeout_sec=0,
                           bool nonblock=false );

    bool GetPort( const XPlat_Socket sock, XPlat_Port& port );

    bool SetOption( XPlat_Socket sock, int level,
                    int option, void* optval, socklen_t optsz );
    
    bool SetBlockingMode( XPlat_Socket sock, bool blocking );

    int Shutdown(XPlat_Socket s, SDHowType how);
    
    bool Close( const XPlat_Socket sock );
                              
    struct NCBuf
    {
        size_t len;
        char* buf;
    };

    ssize_t Send( XPlat_Socket s, 
                  NCBuf* bufs, 
                  unsigned int nBufs );

    ssize_t Recv( XPlat_Socket s, 
                  NCBuf* bufs, 
                  unsigned int nBufs );

    ssize_t send( XPlat_Socket s, 
                  const void *buf, 
                  size_t count );

    ssize_t recv( XPlat_Socket s, 
                  void *buf, 
                  size_t count );

} // namespace SocketUtils

} // namespace XPlat

#endif // XPLAT_SOCKETUTILS_H
