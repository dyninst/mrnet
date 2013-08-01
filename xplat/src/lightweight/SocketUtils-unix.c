/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "xplat_lightweight/xplat_utils_lightweight.h"
#include "xplat_lightweight/Error.h"
#include "xplat_lightweight/NetUtils.h"
#include "xplat_lightweight/SocketUtils.h"

static bool_t SetFlag( XPlat_Socket sock, int flag )
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

static bool_t ClearFlag( XPlat_Socket sock, int flag )
{
    int fdflag = fcntl( sock, F_GETFL );
    if( -1 == fdflag ) {
        // failed to retrieve the socket status flags
        xplat_dbg( 1, xplat_printf(FLF, stderr, "failed to get flags\n") );
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

bool_t XPlat_SocketUtils_SetBlockingMode( XPlat_Socket sock, bool_t blocking )
{
    if( blocking )
        return ClearFlag( sock, O_NONBLOCK );
    else
        return SetFlag( sock, O_NONBLOCK );
}

bool_t XPlat_SocketUtils_Close( XPlat_Socket sock )
{
    int rc = close( sock ); 
    if( 0 == rc ) return true;
    return false;
}

ssize_t XPlat_SocketUtils_Send(XPlat_Socket s, XPlat_NCBuf_t* ncbufs, unsigned int nBufs)
{
    ssize_t sret, ret = 0;
    unsigned int i;
    
    // The simple way: one send per buffer 

    for( i = 0; i < nBufs; i++ ) {
        sret = XPlat_SocketUtils_send(s, ncbufs[i].buf, ncbufs[i].len);
        if( sret >= 0 )
            ret += sret;
    }
    return ret;
}

ssize_t XPlat_SocketUtils_Recv(XPlat_Socket s, XPlat_NCBuf_t* ncbufs, unsigned int nBufs)
{
    ssize_t rret, ret = 0;
    unsigned int i;

    // The simple way: one recv per buffer 

    for( i = 0; i < nBufs; i++ ) {
        rret = XPlat_SocketUtils_recv(s, ncbufs[i].buf, ncbufs[i].len);
        if( rret >= 0 )
            ret += rret;
    }
    return ret;
}

ssize_t XPlat_SocketUtils_send(XPlat_Socket s, const void *buf, size_t count)
{
    if( count == 0 )
        return 0;

    xplat_dbg(5, xplat_printf(FLF, stderr, 
                 "writing %"PRIszt" bytes to fd=%d)\n",
                 count, s));

    int flags = 0;
#if defined(os_linux)
    // don't generate SIGPIPE
    flags = MSG_NOSIGNAL;
#endif

    ssize_t bytes_written = 0;

    while( bytes_written !=  (ssize_t) count ) {

        ssize_t ret = send( s, ((const char*)buf) + bytes_written,
                            count - bytes_written,
                            flags );

        int err = errno;

        if( ret == -1 ) {
            if( (err == EINTR) || (err == EAGAIN) || (err == EWOULDBLOCK) ) {
                continue;
            }
            else {
                xplat_dbg(3, xplat_printf(FLF, stderr,
                         "Warning: premature return from send(). "
                         "Wrote %"PRIsszt" of %"PRIszt" bytes ('%s')\n",
                         bytes_written, count, strerror(err)));
                return bytes_written;
            }
        }
        else {
            bytes_written += ret;
            if( bytes_written < (ssize_t) count ) {
                continue;
            }
            else {
                xplat_dbg(5, xplat_printf(FLF, stderr, "returning %"PRIsszt"\n",
                                          bytes_written));
                return bytes_written;
            }
        }
    }
    assert(!"XPlat_SocketUtils_send - invalid code path");
    return -1;
}

ssize_t XPlat_SocketUtils_recv(XPlat_Socket s, void *buf, size_t count)
{
    if( count == 0 )
        return 0;

    ssize_t bytes_recvd = 0;

    while( bytes_recvd != (ssize_t) count ) {

        ssize_t ret = recv( s, ((char*)buf) + bytes_recvd,
                            count - bytes_recvd, 0 );

        int err = errno;

        if( ret == -1 ) {
            if( (err == EINTR) || (err == EAGAIN) ) {
                continue;
            }
            else {
                xplat_dbg(3, xplat_printf(FLF, stderr,
                             "Warning: premature return from recv(). "
                             "Got %" PRIsszt " of %" PRIszt " bytes ('%s')\n",
                             bytes_recvd, count, strerror(err)));
                return bytes_recvd;
            }
        }
        else if( ret == 0 ) {
            // the remote endpoint has gone away
            xplat_dbg(3, xplat_printf(FLF, stderr, 
                         "recv() returned 0 (peer likely gone)\n"));
            return -1;
        }
        else {
            bytes_recvd += ret;
            if( bytes_recvd < (ssize_t) count ) {
                continue;
            }
            else {
                xplat_dbg(5, xplat_printf(FLF, stderr, "returning %" PRIsszt "\n",
                                          bytes_recvd));
                return bytes_recvd;
            }
        }
    }
    assert(!"XPlat_SocketUtils_recv - invalid code path");
    return -1;
}
