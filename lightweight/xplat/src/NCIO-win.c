#include <stdio.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <string.h>
#include <unistd.h>
#include <bits/uio.h>
#include <limits.h>
#include <sys/socket.h>

#include "xplat/NCIO.h"

// the Winsock version of recv doesn't take an argument to 
// say to do a blocking receive
const int XPlat_NCBlockingRecvFlag = 0;

int NCSend(XPSOCKET s, NCBuf_t* ncbuf, unsigned int nBufs)
{
    int ret = 0;
    DWORD nBytesSent = 0;

    //convert buffer specifiers
    WSABUF* wsaBuf = ncbufs;

    // do the send
    nBytesSent = 0;
    int sret = WSASend(s, wsaBufs, nBufs, nBytesSent, 0, NULL, NULL);
    if (sret != SOCKET_ERROR) {
        ret = nBytesSent;
    } else {
        ret = -1;
    }
    return ret;
}

int NCRecv(XPSOCKET s, NCBuf_t** ncbufs, unsigned int nBufs)
{
    int ret = 0;
    signed int bytes_remaining = 0;

    // convert buffer specifiers
    WSABUF** wsaBufs  = ncbufs;

    // do the receive
    while(1) {
        DWORD nBytesReceived = 0;
        DWORD dwFlags = 0;
        int rret = WSARecv(s, wsaBufs, nBufs, nBytesReceived, dwFlags, NULL, NULL);
        if (rret == SOCKET_ERROR || nBytesReceived == 0)
            return -1;

        bytes_remaining -= nBytesReceived;
        ret += nBytesReceived;
        if (bytes_remaining <= 0)
            break;
        if (bytes_remaining > 0) {
            unsigned i;
            for (i = 0; i < nBufs; i++) {
                if (!wsaBufs[i]->len)
                    continue;
                if (!nBytesReceived)
                    break;
                if (wsaBufs[i]->len <= nBytesReceived) {
                    wsaBufs[i]->len = 0;
                    nBytesReceived -= wsaBufs[i]->len;
                    continue;
                } else {
                    wsaBufs[i]->len -= nBytesReceived;
                    wsaBufs[i]->buf = ((char*)wsaBufs[i]->buf) + nBytesReceived;
                    nBytesReceived = 0;
                }
            }
        }   
    }
    return ret;
}
