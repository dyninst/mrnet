/****************************************************************************
 * Copyright © 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <winsock2.h>
#include <assert.h>

#include "xplat_lightweight/NCIO.h"

// the Winsock version of recv doesn't take an argument to do a blocking
// receive
const int XPlat_NCBlockingRecvFlag = 0;

ssize_t XPlat_NCSend(XPSOCKET s, NCBuf_t* ncbufs, unsigned int nBufs)
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

ssize_t XPlat_NCRecv(XPSOCKET s, NCBuf_t* ncbufs, unsigned int nBufs)
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

ssize_t XPlat_NCsend(XPSOCKET s, const void *buf, size_t count) {
    NCBuf_t ncbuf[1];

    ncbuf[0].buf = (char *)buf;
    ncbuf[0].len = count;

    return XPlat_NCSend(s, ncbuf, 1);
}

ssize_t XPlat_NCrecv(XPSOCKET s, void *buf, size_t count) {
    NCBuf_t ncbuf[1];

    ncbuf[0].buf = buf;
    ncbuf[0].len = count;

    return XPlat_NCRecv(s, ncbuf, 1);
}

