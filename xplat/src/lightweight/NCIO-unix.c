/****************************************************************************
 * Copyright © 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/socket.h>

#include "xplat_lightweight/NCIO.h"

// uncomment following if large data transfers hang
//#define XPLAT_NCRECV_NO_BLOCK

#ifndef XPLAT_NCRECV_NO_BLOCK
const int XPlat_NCBlockingRecvFlag = MSG_WAITALL;
#else
const int XPlat_NCBlockingRecvFlag = 0;
#endif

ssize_t XPlat_NCSend(XPSOCKET s, NCBuf_t* ncbufs, unsigned int nBufs)
{
    ssize_t sret, ret = 0;
    unsigned int i;
    
    // The simple way: one send per buffer 

    for( i = 0; i < nBufs; i++ ) {
        sret = XPlat_NCsend(s, ncbufs[i].buf, ncbufs[i].len);
        if( sret >= 0 )
            ret += sret;
    }
    return ret;
}

ssize_t XPlat_NCRecv(XPSOCKET s, NCBuf_t* ncbufs, unsigned int nBufs)
{
    ssize_t rret, ret = 0;
    unsigned int i;

    // The simple way: one recv per buffer 

    for( i = 0; i < nBufs; i++ ) {
        rret = XPlat_NCrecv(s, ncbufs[i].buf, ncbufs[i].len);
        if( rret >= 0 )
            ret += rret;
    }
    return ret;
}

ssize_t XPlat_NCsend(XPSOCKET s, const void *buf, size_t count)
{
    if( count == 0 )
        return 0;

    //fprintf(stderr, "XPlat DEBUG: NCsend - writing %zd bytes to fd=%d)\n", count, s);

    // don't generate SIGPIPE
    int flags = MSG_NOSIGNAL;

    ssize_t bytes_written = 0;

    while( bytes_written != count ) {

        ssize_t ret = send( s, ((const char*)buf) + bytes_written,
                            count - bytes_written,
                            flags );

        int err = errno;

        if( ret == -1 ) {
            if( (err == EINTR) || (err == EAGAIN) || (err == EWOULDBLOCK) ) {
                continue;
            }
            else {
                /* fprintf(stderr, */
/*                         "Warning: XPlat::NCsend - premature return from send(). " */
/*                         "Wrote %zd of %zd bytes ('%s')\n",  */
/*                         bytes_written, count, strerror(err)); */
                return bytes_written;
            }
        }
        else {
            bytes_written += ret;
            if( bytes_written < count ) {
                continue;
            }
            else {
                //fprintf(stderr, "XPlat DEBUG: NCsend - returning %zd\n", bytes_written);
                return bytes_written;
            }
        }
    }
    assert(!"XPlat::NCsend - invalid code path");
    return -1;
}

ssize_t XPlat_NCrecv(XPSOCKET s, void *buf, size_t count)
{
    if( count == 0 )
        return 0;

    ssize_t bytes_recvd = 0;

    while( bytes_recvd != count ) {

        ssize_t ret = recv( s, ((char*)buf) + bytes_recvd,
                            count - bytes_recvd,
                            XPlat_NCBlockingRecvFlag );

        int err = errno;

        if( ret == -1 ) {
            if( (err == EINTR) || (err == EAGAIN) ) {
                continue;
            }
            else {
                /* fprintf(stderr, */
/*                         "Warning: XPlat::NCrecv - premature return from recv(). " */
/*                         "Got %zd of %zd bytes ('%s')\n",  */
/*                         bytes_recvd, count, strerror(err)); */
                return bytes_recvd;
            }
        }
        else if( ret == 0 ) {
            // the remote endpoint has gone away
            //fprintf(stderr, "XPlat DEBUG: NCrecv - recv() returned 0 (peer likely gone)\n");
            return -1;
        }
        else {
            bytes_recvd += ret;
            if( bytes_recvd < count ) {
                continue;
            }
            else {
                //fprintf(stderr, "XPlat DEBUG: NCrecv - returning %zd\n", bytes_recvd);
                return bytes_recvd;
            }
        }
    }
    assert(!"XPlat::NCrecv - invalid code path");
    return -1;
}
