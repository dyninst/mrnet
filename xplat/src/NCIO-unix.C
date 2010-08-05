/****************************************************************************
 * Copyright © 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: NCIO-unix.C,v 1.6 2008/10/09 19:53:58 mjbrim Exp $
#include <cassert>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include "xplat/NCIO.h"

#ifdef compiler_sun
#include <stdio.h>
#include <limits.h>
#endif

namespace XPlat
{

const int NCBlockingRecvFlag = MSG_WAITALL;

int
NCSend( XPSOCKET s, NCBuf* ncbufs, unsigned int nBufs )
{
    int ret = 0;

    unsigned int nBufsLeftToSend = nBufs;
    NCBuf* currBuf = ncbufs;
    while( nBufsLeftToSend > 0 )
    {
        // determine how many bufs we will try to send
        unsigned int nBufsToSend = 
            ((nBufsLeftToSend > IOV_MAX ) ? IOV_MAX : nBufsLeftToSend);
        
        // convert our buffer spec to writev's buffer spec
        struct iovec* currIov = new iovec[nBufsToSend];
        for( unsigned int i = 0; i < nBufsToSend; i++ )
        {
            currIov[i].iov_base = currBuf[i].buf;
            currIov[i].iov_len = currBuf[i].len;
        }

        // do the send
        int sret = writev( s, currIov, nBufsToSend );
        delete[] currIov;
        if( sret < 0 )
        {
            ret = sret;
            break;
        }
        else
        {
            ret += sret;
        }

        // advance through buffers
        nBufsLeftToSend -= nBufsToSend;
        currBuf += nBufsToSend;
    }

    return ret;
}


int
NCRecv( XPSOCKET s, NCBuf* ncbufs, unsigned int nBufs )
{
    int ret = 0;

    unsigned int nBufsLeftToRecv = nBufs;
    NCBuf* currBuf = ncbufs;
    while( nBufsLeftToRecv > 0 )
    {
        // determine how many bufs we will try to receive
        unsigned int nBufsToRecv = 
            ((nBufsLeftToRecv > IOV_MAX ) ? IOV_MAX : nBufsLeftToRecv);
        
        // convert our buffer spec to recvmsg/readv's buffer spec
        msghdr msg;

        msg.msg_name = NULL;
        msg.msg_namelen = 0;
        msg.msg_iov = new iovec[nBufsToRecv];
        msg.msg_iovlen = nBufsToRecv;
#if defined(os_solaris) && defined(compiler_sun)
        msg.msg_accrights=NULL;
        msg.msg_accrightslen=0;
#else
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
        msg.msg_flags = 0;
#endif

        for( unsigned int i = 0; i < nBufsToRecv; i++ ) {
            msg.msg_iov[i].iov_base = currBuf[i].buf;
            msg.msg_iov[i].iov_len = currBuf[i].len;
        }

        // do the receive
        int sret = recvmsg( s, &msg, NCBlockingRecvFlag );
        if( sret < 0 ) {
            perror( "recvmsg()");
            ret = sret;
#if ! (defined(os_solaris) && defined(compiler_sun))
            int err = msg.msg_flags;
            fprintf(stderr, "NCRecv error msg_flags=%x\n", err);
#endif
            break;
        } else {
            ret += sret;
        }
        delete[] msg.msg_iov;

        // advance through buffers
        nBufsLeftToRecv -= nBufsToRecv;
        currBuf += nBufsToRecv;
    }

    return ret;
}

} // namespace XPlat

