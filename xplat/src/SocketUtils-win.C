/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "xplat/xplat_utils.h"
#include "xplat/SocketUtils.h"

#include <iphlpapi.h>

const char* block_str = "blocking";
const char* nonblock_str = "non-blocking";

namespace XPlat
{

namespace SocketUtils
{

bool SetBlockingMode( XPlat_Socket sock, bool blocking )
{ 
    // 0 is blocking, !0 is non-blocking
    unsigned long mode = ( blocking ? 0 : 1 );
    if( 0 != ioctlsocket(sock, FIONBIO, &mode) ) {
        // failed to set the socket flags
        xplat_dbg( 1, xplat_printf(FLF, stderr, "failed to set %s\n",
			      (blocking ? block_str : nonblock_str)) );
        return false;
    }
    return true;
}

bool Close( XPlat_Socket sock )
{
    int rc = closesocket( sock );
    if( 0 == rc ) return true;
    return false;
}

int Shutdown(XPlat_Socket s, SDHowType how) {
    int in_how;

    if(how == XPLAT_SHUT_RD) {
        in_how = SD_RECEIVE;
    } else if(how == XPLAT_SHUT_WR) {
        in_how = SD_SEND;
    } else if(how == XPLAT_SHUT_RDWR) {
        in_how = SD_BOTH;
    } else {
        xplat_dbg(1, xplat_printf(FLF, stderr, "Invalid 'how' given\n"));
        return -1;
    }

    if(shutdown((SOCKET)s, in_how) != 0) {
        return -1;
    }

    return 0;
}

ssize_t Send( XPlat_Socket s, NCBuf* ncbufs, unsigned int nBufs )
{
    ssize_t ret = 0;
    signed int bytes_remaining = 0;
    DWORD nBytesSent = 0;

    // convert buffer specifiers
    WSABUF* wsaBufs = new WSABUF[nBufs];
    for( unsigned int i = 0; i < nBufs; i++ )
    {
        wsaBufs[i].buf = ncbufs[i].buf;
        wsaBufs[i].len = ncbufs[i].len;
        bytes_remaining += ncbufs[i].len;
    }

    // do the send
    nBytesSent = 0;
    while(1) {
        int sret = WSASend( s, wsaBufs, nBufs, &nBytesSent, 0, NULL, NULL );
        if( sret == SOCKET_ERROR || nBytesSent == 0 )
        {
            ret = nBytesSent;
            break;
        }

        bytes_remaining -= nBytesSent;
        ret += nBytesSent;
        if (bytes_remaining <= 0)
            break;
        if (bytes_remaining > 0) {
            for (unsigned i=0; i<nBufs; i++) {
                if (!wsaBufs[i].len) 
                    continue;
                if (!nBytesSent)
                    break;
                if (wsaBufs[i].len <= nBytesSent) {
                    nBytesSent -= wsaBufs[i].len;
                    wsaBufs[i].len = 0;
                    continue;
                }
                else {
                    wsaBufs[i].len -= nBytesSent;
                    wsaBufs[i].buf = ((char *) wsaBufs[i].buf) + nBytesSent;
                    nBytesSent = 0;
                }
            }
        }
    }

    delete wsaBufs;
    return ret;
}


ssize_t
Recv( XPlat_Socket s, NCBuf* ncbufs, unsigned int nBufs )
{
    ssize_t ret = 0;
    signed int bytes_remaining = 0;
    // convert buffer specifiers
    WSABUF* wsaBufs = new WSABUF[nBufs];
    for( unsigned int i = 0; i < nBufs; i++ )
    {
        wsaBufs[i].buf = ncbufs[i].buf;
        wsaBufs[i].len = ncbufs[i].len;
		bytes_remaining += ncbufs[i].len;
    }

    // do the receive
    while (1) {
        DWORD nBytesReceived = 0;
        DWORD dwFlags = 0;
        int rret = WSARecv( s, wsaBufs, nBufs, &nBytesReceived, &dwFlags, NULL, NULL );
        if( rret == SOCKET_ERROR || nBytesReceived == 0)
            return -1;

        bytes_remaining -= nBytesReceived;
        ret += nBytesReceived;
        if (bytes_remaining <= 0)
            break;
        if (bytes_remaining > 0) {
            for (unsigned i=0; i<nBufs; i++) {
                if (!wsaBufs[i].len) 
                    continue;
                if (!nBytesReceived)
                    break;
                if (wsaBufs[i].len <= nBytesReceived) {
                    nBytesReceived -= wsaBufs[i].len;
                    wsaBufs[i].len = 0;
                    continue;
                }
                else {
                    wsaBufs[i].len -= nBytesReceived;
                    wsaBufs[i].buf = ((char *) wsaBufs[i].buf) + nBytesReceived;
                    nBytesReceived = 0;
                }
            }
        }
    }

    delete wsaBufs;
    return ret;
}

ssize_t send( XPlat_Socket s, const void *buf, size_t count )
{
    if( count == 0 )
        return 0;

    NCBuf ncbuf;
    ncbuf.buf = (char *)buf;
    ncbuf.len = count;

    return Send( s, &ncbuf, 1 );
}

ssize_t recv( XPlat_Socket s, void *buf, size_t count )
{
    if( count == 0 )
        return 0;

    NCBuf ncbuf;
    ncbuf.buf = (char *)buf;
    ncbuf.len = count;

    return Recv( s, &ncbuf, 1 );
}

} // namespace SocketUtils

} // namespace XPlat
