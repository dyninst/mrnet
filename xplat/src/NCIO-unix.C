/****************************************************************************
 * Copyright © 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
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

#include "xplat/Error.h"
#include "xplat/NetUtils.h"
#include "xplat/NCIO.h"

#ifdef compiler_sun
#include <stdio.h>
#include <limits.h>
#endif

#define XPLAT_MAX_WRITEV_BUFSIZE 409600 // 100 pages (assuming page is 4KB)

namespace XPlat
{

const int NCBlockingRecvFlag = MSG_WAITALL;

ssize_t
NCSend( XPSOCKET s, NCBuf* ncbufs, unsigned int nBufs )
{
    NCBuf* currBuf = ncbufs;
    ssize_t ret = 0;
    unsigned int i, currBufNdx = 0;

    unsigned int currIovLen = ((nBufs > IOV_MAX ) ? IOV_MAX : 8 );
    struct iovec* currIov = (struct iovec*) calloc( currIovLen, 
                                                    sizeof(struct iovec) );

    while( currBufNdx < nBufs ) {

        // convert our buffer spec to writev's buffer spec
        ssize_t numBytesToSend = 0;
        for( i = 0; (i < currIovLen) && (currBufNdx < nBufs);  ) {
            size_t len = currBuf[currBufNdx].len;
            // fprintf(stdout, "XPlat DEBUG: NCSend - currBuf->len = %zd\n", len);
            if( len <= XPLAT_MAX_WRITEV_BUFSIZE ) {
                currIov[i].iov_base = currBuf[currBufNdx].buf;
                currIov[i].iov_len = len;
                numBytesToSend += len;
                currBufNdx++;
                i++;
            }
            else {
                // split big buffer into chunks of size XPLAT_MAX_WRITEV_BUFSIZE
                size_t offset = 0;
                for( ; (i < currIovLen) && (len > 0); i++ ) {

                    currIov[i].iov_base = currBuf[currBufNdx].buf + offset;

                    if( len > XPLAT_MAX_WRITEV_BUFSIZE ) {
                        currIov[i].iov_len = XPLAT_MAX_WRITEV_BUFSIZE;
                        numBytesToSend += XPLAT_MAX_WRITEV_BUFSIZE;
                        len -= XPLAT_MAX_WRITEV_BUFSIZE;
                        offset += XPLAT_MAX_WRITEV_BUFSIZE;
                    }
                    else {
                        currIov[i].iov_len = len;
                        numBytesToSend += len;
                        offset += len;
                        len = 0;
                    }
                }
                if( i == currIovLen ) {
                    if (offset < currBuf[currBufNdx].len ) {
                        // update current buffer base/len to account for part that we will send
                        currBuf[currBufNdx].buf += offset;
                        currBuf[currBufNdx].len -= offset;
                    }
                    else currBufNdx++;
                }
                else currBufNdx++;
            }
        }

        unsigned int numIovBufs = i;

        // do the send
        // /* DEBUG LOOP */
//         for( unsigned int u = 0; u < numIovBufs; u++ )
//             fprintf(stdout, "XPlat DEBUG: NCSend - iov->len = %zd\n", currIov[u].iov_len);
//         /* END DEBUG LOOP */
        ssize_t wret = writev( s, currIov, numIovBufs );
        int err = XPlat::NetUtils::GetLastError();

        if( wret < 0 ) {
            if( (err == EINTR) || (err == EAGAIN) || (err == EWOULDBLOCK) )
                continue;
            
            fprintf(stderr, "Error: XPlat::NCSend - writev() : %s\n", 
                    strerror(err));
            ret = wret;
            break; // out of while
        }

        ret += wret;

        if( wret != numBytesToSend ) {

            // fprintf(stdout, "XPlat DEBUG: NCSend - writev wrote %zd of %zd bytes\n", wret, numBytesToSend);

            // find unsent or partial-send buffers
            ssize_t running_total = 0;
            for( i = 0; i < numIovBufs; i++ ) {
                running_total += currIov[i].iov_len;
                if( running_total == wret ) { 
                    // buffers up to current index were sent in full
                    i++;
                    break; // out of for
                }
                else if( running_total > wret ) {
                    // current index was partially sent, send the rest
                    size_t unsent = running_total - wret;
                    ssize_t sent = currIov[i].iov_len - unsent;
                    char* new_base = (char*)currIov[i].iov_base + sent;
                    wret = NCsend( s, new_base, unsent );
                    if( wret < 0 ) {
                        free( currIov );
                        fprintf(stderr, "Warning: XPlat::NCSend - wrote %zd of %zd bytes\n", 
                                ret, numBytesToSend);

                        return ret;
                    }
                    i++;
                    break; // out of for
                }
                // else, running_total < wret
            }
     
            // i is now number fully sent, try to send remaining bufs in currIov
            if( i < numIovBufs ) {
                wret = writev( s, currIov + i, numIovBufs - i );
                err = XPlat::NetUtils::GetLastError();
                if( wret < 0 ) {
                    free( currIov );
                    fprintf(stderr, "Error: XPlat::NCSend - fallback writev() : %s\n", 
                            strerror(err));
                    return ret;
                }
            }
        }
    }

    free( currIov );
    return ret;
}


ssize_t
NCRecv( XPSOCKET s, NCBuf* ncbufs, unsigned int nBufs )
{
    ssize_t ret = 0;

    unsigned int nBufsLeftToRecv = nBufs;
    NCBuf* currBuf = ncbufs;
    while( nBufsLeftToRecv > 0 ) {

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
        msg.msg_accrights = NULL;
        msg.msg_accrightslen = 0;
#else
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
        msg.msg_flags = 0;
#endif

        ssize_t numBytesToRecv = 0;
        for( unsigned int i = 0; i < nBufsToRecv; i++ ) {
            msg.msg_iov[i].iov_base = currBuf[i].buf;
            msg.msg_iov[i].iov_len = currBuf[i].len;
            numBytesToRecv += currBuf[i].len;
        }

        // do the receive
        ssize_t rret = recvmsg( s, &msg, NCBlockingRecvFlag );
        if( rret < 0 ) {
            perror( "XPlat::NCRecv - recvmsg()");
            ret = rret;
#if ! (defined(os_solaris) && defined(compiler_sun))
            int err = msg.msg_flags;
            fprintf(stderr, "Warning: XPlat::NCRecv error msg_flags=%x\n", err);
#endif
            break;
        } else {
            ret += rret;

            if( rret != numBytesToRecv ) {
                // this shouldn't happen, because we asked for blocking recvmsg
                fprintf(stderr, "Warning: XPlat::NCRecv - got %zd of %zd bytes\n", 
                        rret, numBytesToRecv);
                return ret;
            }
        }
        delete[] msg.msg_iov;

        // advance through buffers
        nBufsLeftToRecv -= nBufsToRecv;
        currBuf += nBufsToRecv;
    }

    return ret;
}

ssize_t NCsend( XPSOCKET s, const void *buf, size_t count )
{
    if( count == 0 )
        return 0;

    //fprintf(stderr, "XPlat DEBUG: NCsend - writing %zd bytes to fd=%d)\n", count, s);

    // don't generate SIGPIPE
    int flags = MSG_NOSIGNAL;

    ssize_t bytes_written = 0;

    while( bytes_written != count ) {

        ssize_t ret = ::send( s, ((char*)buf) + bytes_written,
                              count - bytes_written,
                              flags );

        int err = XPlat::NetUtils::GetLastError();

        if( ret == -1 ) {
            if( (err == EINTR) || (err == EAGAIN) || (err == EWOULDBLOCK) ) {
                continue;
            }
            else {
                std::string errstr = XPlat::Error::GetErrorString( err );
                fprintf(stderr,
                        "Warning: XPlat::NCsend - premature return from send(). "
                        "Wrote %zd of %zd bytes ('%s')\n", 
                        bytes_written, count, errstr.c_str());
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

ssize_t NCrecv( XPSOCKET s, void *buf, size_t count )
{
    if( count == 0 )
        return 0;

    ssize_t bytes_recvd = 0;

    while( bytes_recvd != count ) {

        ssize_t ret = ::recv( s, ((char*)buf) + bytes_recvd,
                              count - bytes_recvd,
                              XPlat::NCBlockingRecvFlag );

        int err = XPlat::NetUtils::GetLastError();

        if( ret == -1 ) {
            if( err == EINTR ) {
                continue;
            }
            else {
                std::string errstr = XPlat::Error::GetErrorString( err );
                fprintf(stderr,
                        "Warning: XPlat::NCrecv - premature return from recv(). "
                        "Got %zd of %zd bytes ('%s')\n", 
                        bytes_recvd, count, errstr.c_str());
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

} // namespace XPlat

