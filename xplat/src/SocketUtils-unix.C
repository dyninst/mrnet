/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/
#include <sys/uio.h>
#include "xplat/xplat_utils.h"
#include "xplat/Error.h"
#include "xplat/NetUtils.h"
#include "xplat/SocketUtils.h"

// uncomment following if large data transfers hang
#define XPLAT_RECV_NO_BLOCK

const char* block_str = "blocking";
const char* nonblock_str = "non-blocking";

static bool SetFlag( XPlat_Socket sock, int flag )
{
    int fdflag = fcntl( sock, F_GETFL );
    if( -1 == fdflag ) {
        // failed to retrieve the socket status flags
        return false;
    }
    else {
        int fret;
        fret = fcntl( sock, F_SETFL, fdflag | flag );
        if( -1 == fret ) {
            // failed to set the socket status flags
            return false;
        }
    }
    return true;
}

static bool ClearFlag( XPlat_Socket sock, int flag )
{
    int fdflag = fcntl( sock, F_GETFL );
    if( -1 == fdflag ) {
        // failed to retrieve the socket status flags
        xplat_dbg( 1, XPlat::xplat_printf(FLF, stderr, "failed to get flags\n") );
        return false;
    }
    else {
        int fret;
        fret = fcntl( sock, F_SETFL, fdflag & ~flag );
        if( -1 == fret ) {
            // failed to set the socket status flags
            return false;
        }
    }
    return true;
}

namespace XPlat
{

namespace SocketUtils
{

bool SetBlockingMode( XPlat_Socket sock, bool blocking )
{
    if( blocking )
        return ClearFlag( sock, O_NONBLOCK );
    else
        return SetFlag( sock, O_NONBLOCK );
}

bool Close( const XPlat_Socket sock )
{
    int rc = close( sock );
    if( 0 == rc ) return true;
    return false;
}

#ifndef XPLAT_RECV_NO_BLOCK
const int BlockingRecvFlag = MSG_WAITALL;
#else
const int BlockingRecvFlag = 0;
#endif

ssize_t Send( XPlat_Socket s, NCBuf* ncbufs, unsigned int nBufs )
{
    NCBuf* currBuf = ncbufs;
    ssize_t ret = 0;
    unsigned int i, currBufNdx = 0;

    // common case is small number of bufs
    unsigned int currIovLen = ((nBufs > IOV_MAX ) ? IOV_MAX : 8 );
    struct iovec* currIov = (struct iovec*) calloc( currIovLen, 
                                                    sizeof(struct iovec) );

    while( currBufNdx < nBufs ) {

        // convert our buffer spec to writev's buffer spec
        ssize_t numBytesToSend = 0;
        for( i = 0; (i < currIovLen) && (currBufNdx < nBufs); i++ ) {
            size_t len = currBuf[currBufNdx].len;
            //xplat_dbg( 5, xplat_printf(FLF, stderr, "currBuf->len = %" PRIszt"\n", len) );
            currIov[i].iov_base = currBuf[currBufNdx].buf;
            currIov[i].iov_len = len;
            numBytesToSend += len;
            currBufNdx++;
        }

        unsigned int numIovBufs = i;

        // do the send
        ssize_t wret = writev( s, currIov, numIovBufs );
        int err = XPlat::NetUtils::GetLastError();

        if( wret < 0 ) {
            if( (err == EINTR) || (err == EAGAIN) || (err == EWOULDBLOCK) )
                continue;
            
            xplat_dbg( 1, xplat_printf(FLF, stderr, "Error: writev() failed with '%s'\n", 
                                  strerror(err)) );
            ret = wret;
            break; // out of while
        }

        ret += wret;

        if( wret != numBytesToSend ) {

            // xplat_dbg( 5, xplat_printf(FLF, stdout, "writev wrote %" PRIsszt" of %" PRIsszt" bytes\n", wret, numBytesToSend) );

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
                    wret = send( s, new_base, unsent );
                    if( wret < 0 ) {
                        free( currIov );
                        xplat_dbg( 3, xplat_printf(FLF, stderr, "Warning: wrote %" PRIsszt" of %" PRIsszt" bytes\n", 
                                              ret, numBytesToSend) );

                        return ret;
                    }
                    ret += wret;
                    i++;
                    break; // out of for
                }
                // else, running_total < wret
            }
     
            // i is now number fully sent, try to send remaining bufs in currIov
            if( i < numIovBufs ) {
                for (;i < numIovBufs; i++) { 
                    wret = send( s, currIov[i].iov_base, currIov[i].iov_len  );
                    err = XPlat::NetUtils::GetLastError();
                    if( wret < 0 ) {
                        free( currIov );
                        xplat_dbg( 1, xplat_printf(FLF, stderr, "Error: "
                                              "fallback send() failed with '%s'\n", 
                                              strerror(err)) );
                        return ret;
                    }   
                    ret += wret; 
                }
            }
        }
    }

    free( currIov );
    return ret;
}

ssize_t Recv( XPlat_Socket s, NCBuf* ncbufs, unsigned int nBufs )
{
    ssize_t ret = 0;

#if defined(XPLAT_RECV_NO_BLOCK)

    // The simple way: one recv per buffer 

    for( unsigned int i = 0; i < nBufs; i++ ) {
        ssize_t rret = recv( s, ncbufs[i].buf, ncbufs[i].len );
        if( rret >= 0 )
            ret += rret;
    }

#else // !defined(XPLAT_RECV_NO_BLOCK)

    // Use blocking recvmsg to get all buffers in one call 

    int err;
    unsigned int i, nBufsLeftToRecv = nBufs;
    NCBuf* currBuf = ncbufs;
    while( nBufsLeftToRecv > 0 ) {

        // determine how many bufs we will try to receive
        unsigned int nBufsToRecv = 
            ((nBufsLeftToRecv > IOV_MAX ) ? IOV_MAX : nBufsLeftToRecv);
        
        // convert our buffer spec to recvmsg's buffer spec
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
        for( i = 0; i < nBufsToRecv; i++ ) {
            msg.msg_iov[i].iov_base = currBuf[i].buf;
            msg.msg_iov[i].iov_len = currBuf[i].len;
            numBytesToRecv += currBuf[i].len;
        }

        // do the receive
        ssize_t rret = recvmsg( s, &msg, 0 );
        if( rret < 0 ) {
            err = XPlat::NetUtils::GetLastError();        
            xplat_dbg( 1, xplat_printf(FLF, stderr, "Error: "
                                  "recvmsg() failed with '%s'\n", 
                                  strerror(err)) );
            ret = rret;
#if ! (defined(os_solaris) && defined(compiler_sun))
            err = msg.msg_flags;
            xplat_dbg( 1, xplat_printf(FLF, stderr, "Warning: "
                                  "error msg_flags=%x\n", err) );
#endif
            break; // out of while
        }

        ret += rret;

        if( rret != numBytesToRecv ) {

            // loop to find missing bufs and recv them
            ssize_t running_total = 0;
            for( i = 0; i < nBufsToRecv; i++ ) {
                running_total += msg.msg_iov[i].iov_len;
                if( running_total == rret ) { 
                    // buffers up to current index were received in full
                    i++;
                    break; // out of for
                }
                else if( running_total > rret ) {
                    // current index was partially received, recv the rest
                    size_t torecv = running_total - rret;
                    ssize_t recvd = msg.msg_iov[i].iov_len - torecv;
                    char* new_base = (char*)msg.msg_iov[i].iov_base + recvd;
                    rret = recv( s, new_base, torecv );
                    if( rret < 0 ) {
                        delete[] msg.msg_iov;
                        return ret;
                    }
                    ret += rret;
                    i++;
                    break; // out of for
                }
                // else, running_total < rret
            }
     
            // i is now number fully sent, try to recv remaining bufs in msg_iov
            for( ; i < nBufsToRecv; i++ ) {
                rret = recv( s, msg.msg_iov[i].iov_base , msg.msg_iov[i].iov_len  );
                if( rret < 0 ) {
                    delete[] msg.msg_iov;
                    return ret;
                }
                ret += rret;    
            }
        }

        delete[] msg.msg_iov;

        // advance through buffers
        nBufsLeftToRecv -= nBufsToRecv;
        currBuf += nBufsToRecv;
    }

#endif // defined(XPLAT_RECV_NO_BLOCK)

    return ret;
}

ssize_t send( XPlat_Socket s, const void *buf, size_t count )
{
    if( count == 0 )
        return 0;

    //xplat_dbg( 5, xplat_printf(FLF, stderr, 
    //                      "writing %" PRIszt" bytes to fd=%d)\n", count, s) );

    int flags = 0;
#if defined(os_linux)
    // don't generate SIGPIPE
    flags = MSG_NOSIGNAL;
#endif

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
                xplat_dbg( 3, xplat_printf(FLF, stderr, "Warning: "
                                      "premature return from send() ('%s'). "
                                      "Wrote %" PRIsszt" of %" PRIszt" bytes.\n", 
                                      errstr.c_str(), bytes_written, count) );
                return bytes_written;
            }
        }
        else {
            bytes_written += ret;
            if( bytes_written < count ) {
                continue;
            }
            else {
                xplat_dbg( 5, xplat_printf(FLF, stderr,
                                      "returning %" PRIsszt"\n", bytes_written) );
                return bytes_written;
            }
        }
    }
    assert(!"XPlat::SocketUtils::send - invalid code path");
    return -1;
}

ssize_t recv( XPlat_Socket s, void *buf, size_t count )
{
    if( count == 0 )
        return 0;

    ssize_t bytes_recvd = 0;

    while( bytes_recvd != count ) {

        ssize_t ret = ::recv( s, ((char*)buf) + bytes_recvd,
                              count - bytes_recvd,
                              BlockingRecvFlag );

        int err = XPlat::NetUtils::GetLastError();

        if( ret == -1 ) {
            if( (err == EINTR) || (err == EAGAIN) ) {
                continue;
            }
            else if( err == ECONNRESET ) {
                // the remote endpoint has gone away
                //xplat_dbg( 5, xplat_printf(FLF, stderr, "recv() got connection reset\n") );
                return -1;
            }
            else {
                std::string errstr = XPlat::Error::GetErrorString( err );
                xplat_dbg( 3, xplat_printf(FLF, stderr,
                                      "Warning: premature return from recv(). "
                                      "Got %" PRIsszt" of %" PRIszt" bytes ('%s')\n", 
                                      bytes_recvd, count, errstr.c_str()) );
                return bytes_recvd;
            }
        }
        else if( ret == 0 ) {
            // the remote endpoint has gone away
            xplat_dbg( 5, xplat_printf(FLF, stderr, "recv() returned 0 (peer likely gone)\n"));
            return -1;
        }
        else {
            bytes_recvd += ret;
            if( bytes_recvd < count ) {
                continue;
            }
            else {
                xplat_dbg( 5, xplat_printf(FLF, stderr,
                                      "returning %" PRIsszt"\n", bytes_recvd) );
                return bytes_recvd;
            }
        }
    }
    assert(!"XPlat::SocketUtils::recv - invalid code path");
    return -1;
}

int Shutdown(XPlat_Socket s, SDHowType how) {
    int in_how;

    if(how == XPLAT_SHUT_RD) {
        in_how = SHUT_RD;
    } else if(how == XPLAT_SHUT_WR) {
        in_how = SHUT_WR;
    } else if(how == XPLAT_SHUT_RDWR) {
        in_how = SHUT_RDWR;
    } else {
        xplat_dbg(1, xplat_printf(FLF, stderr, "Invalid 'how' given\n"));
        return -1;
    }

    return shutdown((int)s, in_how);
}


} // namespace SocketUtils

} // namespace XPlat

