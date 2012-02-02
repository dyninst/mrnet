/****************************************************************************
 * Copyright © 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: NCIO-win.C,v 1.5 2008/10/09 19:54:02 mjbrim Exp $

#include <winsock2.h>
#include <cstdio>
#include "xplat/NCIO.h"

namespace XPlat
{

// the Winsock version of recv doesn't take an argument to say to do 
// a blocking receive.
const int NCBlockingRecvFlag = 0;

ssize_t
NCSend( XPSOCKET s, NCBuf* ncbufs, unsigned int nBufs )
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
NCRecv( XPSOCKET s, NCBuf* ncbufs, unsigned int nBufs )
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

ssize_t NCsend( XPSOCKET s, const void *buf, size_t count )
{
    if( count == 0 )
        return 0;

    NCBuf ncbuf;
    ncbuf.buf = (char *)buf;
    ncbuf.len = count;

    return NCSend( s, &ncbuf, 1 );
}

ssize_t NCrecv( XPSOCKET s, void *buf, size_t count )
{
    if( count == 0 )
        return 0;

    NCBuf ncbuf;
    ncbuf.buf = (char *)buf;
    ncbuf.len = count;

    return NCRecv( s, &ncbuf, 1 );
}


} // namespace XPlat

