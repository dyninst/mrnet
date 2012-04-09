/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "xplat_lightweight/xplat_utils_lightweight.h"
#include "xplat_lightweight/SocketUtils.h"

#include <iphlpapi.h>

const char* block_str = "blocking";
const char* nonblock_str = "non-blocking";

bool_t XPlat_SocketUtils_SetBlockingMode( XPlat_Socket sock,
                                          bool_t blocking )
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

bool_t XPlat_SocketUtils_Close( XPlat_Socket sock )
{
    int rc = closesocket( sock );
    if( 0 == rc ) return true;
    return false;
}

ssize_t XPlat_SocketUtils_Send( XPlat_Socket s, 
                                XPlat_NCBuf_t* ncbufs, unsigned int nBufs )
{
    int ret = 0;
    int bytes_remaining = 0;
    DWORD nBytesSent = 0;
    WSABUF *wsaBufs;
    int sret;
    unsigned int i;

    //Allocate memory based on nBufs
    wsaBufs = (WSABUF*)malloc(sizeof(WSABUF) * nBufs);
    assert(wsaBufs);

    //convert buffer specifiers
    for(i = 0; i < nBufs; i++) {
        wsaBufs[i].buf = ncbufs[i].buf;
        wsaBufs[i].len = (ULONG)ncbufs[i].len;
        bytes_remaining += (int)ncbufs[i].len;
    }

    // do the send
    while(1) {
        sret = WSASend(s, wsaBufs, nBufs, &nBytesSent, 0, NULL, NULL);
        if (sret == SOCKET_ERROR || nBytesSent == 0) {
            ret = -1;
            break;
        }

        bytes_remaining -= nBytesSent;
        ret += nBytesSent;
        if (bytes_remaining <= 0)
            break;
        if (bytes_remaining > 0) {
            for (i = 0; i < nBufs; i++) {
                if (!wsaBufs[i].len)
                    continue;
                if (!nBytesSent)
                    break;
                if (wsaBufs[i].len <= nBytesSent) {
                    nBytesSent -= wsaBufs[i].len;
                    wsaBufs[i].len = 0;
                    continue;
                } else {
                    wsaBufs[i].len -= nBytesSent;
                    wsaBufs[i].buf = ((char*)wsaBufs[i].buf) + nBytesSent;
                    nBytesSent = 0;
                }
            }
        }
    }

    free(wsaBufs);

    return ret;
}

ssize_t XPlat_SocketUtils_Recv( XPlat_Socket s, 
                                XPlat_NCBuf_t* ncbufs, unsigned int nBufs )
{
    int ret = 0;
    int bytes_remaining = 0;
    WSABUF* wsaBufs = (WSABUF*)malloc(sizeof(WSABUF)*nBufs);
    unsigned int i;
    DWORD nBytesReceived = 0;
    DWORD dwFlags = 0;
    int rret;
    assert(wsaBufs);

    // convert buffer specifiers
    for (i = 0; i < nBufs; i++) {
        wsaBufs[i].buf = ncbufs[i].buf;
        wsaBufs[i].len = (ULONG)ncbufs[i].len;
        bytes_remaining += (int)ncbufs[i].len;
    }

    // do the receive
    while(1) {
        rret = WSARecv(s, wsaBufs, nBufs, &nBytesReceived, &dwFlags, NULL, NULL);
        if (rret == SOCKET_ERROR || nBytesReceived == 0) {
            ret = -1;
            break;
        }

        bytes_remaining -= nBytesReceived;
        ret += nBytesReceived;

        if (bytes_remaining <= 0)
            break;
        if (bytes_remaining > 0) {
            for (i = 0; i < nBufs; i++) {
                if (!wsaBufs[i].len)
                    continue;
                if (!nBytesReceived)
                    break;
                if (wsaBufs[i].len <= nBytesReceived) {
                    nBytesReceived -= wsaBufs[i].len;
                    wsaBufs[i].len = 0;
                    continue;
                } else {
                    wsaBufs[i].len -= nBytesReceived;
                    wsaBufs[i].buf = ((char*)wsaBufs[i].buf) + nBytesReceived;
                    nBytesReceived = 0;
                }
            }
        }   
    }

    free(wsaBufs);

    return ret;
}

ssize_t XPlat_SocketUtils_send( XPlat_Socket s, const void *buf, size_t count )
{
    XPlat_NCBuf_t ncbuf;

    ncbuf.buf = (char *)buf;
    ncbuf.len = count;

    return XPlat_SocketUtils_Send(s, &ncbuf, 1);
}

ssize_t XPlat_SocketUtils_recv( XPlat_Socket s, void *buf, size_t count )
{
    XPlat_NCBuf_t ncbuf;

    ncbuf.buf = buf;
    ncbuf.len = count;

    return XPlat_SocketUtils_Recv(s, &ncbuf, 1);
}
