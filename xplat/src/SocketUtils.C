/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "xplat/xplat_utils.h"
#include "xplat/Error.h"
#include "xplat/NetUtils.h"
#include "xplat/SocketUtils.h"

// Functions implemented herein are (mostly) cross-platform

static bool SetTcpNoDelay( XPlat_Socket sock )
{
#if defined(TCP_NODELAY)
    // turn off Nagle algorithm for coalescing packets
    int optval = 1;
    bool soret = XPlat::SocketUtils::SetOption( sock,
                            IPPROTO_TCP,
                            TCP_NODELAY,
                            (void*) &optval,
                            (socklen_t) sizeof(optval) );
    if( ! soret ) {
        xplat_dbg( 1, XPlat::xplat_printf(FLF, stderr, "failed to set option\n") );
        return false;
    }
#else
    xplat_dbg(1, XPlat::xplat_printf(FLF, stderr, 
                "WARNING: TCP_NODELAY not found! Performance WILL suffer.\n"));
#endif
    return true;
}

static bool SetCloseOnExec( XPlat_Socket sock )
{
#ifndef os_windows
    int fdflag, fret;
    fdflag = fcntl( sock, F_GETFD );
    if( fdflag == -1 ) {
        // failed to retrieve the socket descriptor flags
        xplat_dbg( 1, XPlat::xplat_printf(FLF, stderr, 
                              "failed to get flags\n") );     
    return false;
    }
    else {
        fret = fcntl( sock, F_SETFD, fdflag | FD_CLOEXEC );
        if( fret == -1 ) {
            // failed to set the socket descriptor flags
            xplat_dbg( 1, XPlat::xplat_printf(FLF, stderr, 
                              "failed to set flags\n") );
        return false;
        }
    }
#endif
    return true;
}

namespace XPlat
{

namespace SocketUtils
{
#ifndef os_windows
const XPlat_Socket InvalidSocket = -1;
#else
const XPlat_Socket InvalidSocket = INVALID_SOCKET;
#endif
const XPlat_Port InvalidPort = (XPlat_Port)0;


bool Connect( const std::string &hostname,
              const XPlat_Port port,
              XPlat_Socket& sock, 
              const unsigned num_retry /*=0*/ )
{
    const char* host = hostname.c_str();
    XPlat_Socket _sock = sock;
    int err;
    std::string err_str;

    xplat_dbg( 3, xplat_printf(FLF, stderr,
                               "host=%s port=%hu sock=%d\n",
                               host, port, _sock) );

    if( _sock == InvalidSocket ) {
        _sock = socket( AF_INET, SOCK_STREAM, 0 );
        if( _sock == InvalidSocket ) {
            err = XPlat::NetUtils::GetLastError();
            err_str = XPlat::Error::GetErrorString( err );
             xplat_dbg( 1, xplat_printf(FLF, stderr,
                                   "socket() failed with '%s'\n",
                                   err_str.c_str()) );
            return false;
        }
        xplat_dbg(5, xplat_printf(FLF, stderr,
                             "socket() => %d\n", _sock));
    }

    XPlat::NetUtils::NetworkAddress naddr;
    int rc = XPlat::NetUtils::GetNetworkAddress( hostname, naddr );
    if( rc == -1 ) {
        xplat_dbg( 1, xplat_printf(FLF, stderr,
                              "failed to convert name %s to network address\n",
                              host) );
        return false;
    }

    in_addr_t addr = naddr.GetInAddr();
    struct sockaddr_in server_addr;
    memset( &server_addr, 0, sizeof( server_addr ) );
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons( port );
    memcpy( &server_addr.sin_addr, &addr, sizeof(addr) );

    unsigned nConnectTries = 0;
    int cret = -1;
    do {
        cret = connect( _sock, (sockaddr *) &server_addr, sizeof(server_addr) );

        if( cret == -1 ) {
            err = XPlat::NetUtils::GetLastError();
            if( ! (XPlat::Error::ETimedOut(err) || 
                   XPlat::Error::EConnRefused(err) ||
                   XPlat::Error::EInterrupted(err))) {
                err_str = XPlat::Error::GetErrorString( err );
                xplat_dbg( 1, xplat_printf(FLF, stderr,
                                      "connect() to %s:%hu failed with '%s'\n",
                                      host, port, err_str.c_str()) );
                XPlat::SocketUtils::Close( _sock );
                return false;
            }

            xplat_dbg( 3, xplat_printf(FLF, stderr,
                                  "connect() to %s:%hu timed-out %u times\n",
                                  host, port, nConnectTries) );
            if( (num_retry > 0) && (nConnectTries >= num_retry) )
                break;

            // delay before trying again (more each time)
            if (!XPlat::Error::EInterrupted(err)) {
                nConnectTries++;
                sleep( nConnectTries );
            }
        }
    } while( -1 == cret );

    if( -1 == cret ) {
        err_str = XPlat::Error::GetErrorString( err );
        xplat_dbg( 1, xplat_printf(FLF, stderr,
                              "connect() to %s:%hu failed with '%s' after %u tries\n",
                              host, port, err_str.c_str(), nConnectTries) );
        XPlat::SocketUtils::Close( _sock );
        return false;
    }

    // Close socket on exec
    if( ! SetCloseOnExec(_sock) ) {
        xplat_dbg( 1, xplat_printf(FLF, stderr,
                              "failed to set close-on-exec\n") );
    }

    // Turn off Nagle algorithm
    if( ! SetTcpNoDelay(_sock) ) {
        xplat_dbg( 1, xplat_printf(FLF, stderr,
                              "failed to set TCP_NODELAY\n") );
    }

    xplat_dbg( 3, xplat_printf(FLF, stderr,
                          "Returning socket=%d\n", _sock) );
    sock = _sock;
    return true;
}


bool CreateListening( XPlat_Socket& sock, 
                      XPlat_Port& port, 
                      bool nonblock /*=false*/ )
{
    static int backlog = 128;
    int err;
    XPlat_Socket _sock;
    XPlat_Port _port = port;
    std::string err_str;

    if( InvalidPort == _port )
        _port = 0;

    _sock = socket( AF_INET, SOCK_STREAM, 0 );
    if( -1 == _sock ) {
        err = XPlat::NetUtils::GetLastError();
        err_str = XPlat::Error::GetErrorString( err );
        xplat_dbg( 1, xplat_printf(FLF, stderr,
                              "socket() failed with '%s'\n",
                               err_str.c_str()) );
        return false;
    }

    xplat_dbg( 3, xplat_printf(FLF, stderr,
                          "sock:%d, port:%d\n",
                          _sock, _port) );

    // Close socket on exec
    if( ! SetCloseOnExec(_sock) ) {
        xplat_dbg( 1, xplat_printf(FLF, stderr,
                              "failed to set close-on-exec\n") );     
    }

    // Set listening socket to non-blocking if requested
    if( nonblock ) {
        if( ! SetBlockingMode(_sock, false) )
            xplat_dbg( 1, xplat_printf(FLF, stderr,
                                  "failed to set non-blocking\n") );
    }


    struct sockaddr_in local_addr;
    memset( &local_addr, 0, sizeof( local_addr ) );
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl( INADDR_ANY );

    if( 0 != _port ) {

#ifndef os_windows
        // set the socket so that it does not hold onto its port after
        // the process exits (needed because on at least some platforms we
        // use well-known ports when connecting sockets)
        int soVal = 1;
        bool soret = SetOption( _sock, SOL_SOCKET, SO_REUSEADDR,
                                (void*)&soVal, sizeof(soVal) );
        if( ! soret ) {
            err = XPlat::NetUtils::GetLastError();
            err_str = XPlat::Error::GetErrorString( err );
            xplat_dbg( 1, xplat_printf(FLF, stderr,
                                  "setsockopt() failed with '%s'\n",
                                  err_str.c_str()) );
        }
#endif
        // try to bind and listen using the supplied port
        local_addr.sin_port = htons( _port );
        if( -1 == bind(_sock, (sockaddr*)&local_addr, sizeof(local_addr)) ) {
            err = XPlat::NetUtils::GetLastError();
            err_str = XPlat::Error::GetErrorString( err );
            xplat_dbg( 1, xplat_printf(FLF, stderr,
                                  "bind() to static port %d failed with '%s'\n",
                                   _port, err_str.c_str()) );
            XPlat::SocketUtils::Close( _sock );
            return false;
        }
    }

#ifndef os_windows
    // else, the system will assign a port for us in listen
    if( -1 == listen(_sock, backlog) ) {
        err = XPlat::NetUtils::GetLastError();
        err_str = XPlat::Error::GetErrorString( err );
        xplat_dbg( 1, xplat_printf(FLF, stderr,
                              "listen() failed with '%s'\n", err_str.c_str() ) );
        XPlat::SocketUtils::Close( _sock );
        return false;
    }
    // determine which port we were actually assigned to
    if( ! GetPort(_sock, _port) ) {
        xplat_dbg( 1, xplat_printf(FLF, stderr,
                              "failed to obtain port from socket\n" ) );
        XPlat::SocketUtils::Close( _sock );
        return false;
    }
#else
    // try binding ports, starting from 1st dynamic port
    _port = 49152;
    bool success = false;
    do {
        local_addr.sin_port = htons( _port );
        if( -1 == bind(_sock, (sockaddr*)&local_addr, sizeof(local_addr)) ) {
            err = XPlat::NetUtils::GetLastError();
            err_str = XPlat::Error::GetErrorString( err );
            if( XPlat::Error::EAddrInUse( err ) ) {
                ++_port;
                continue;
            }
            else {
                xplat_dbg( 1, xplat_printf(FLF, stderr,
                                      "bind() to dynamic port %hu failed with '%s'\n",
                                      _port, err_str.c_str() ) );
                XPlat::SocketUtils::Close( _sock );
                return false;
            }
        }
        else {
            if( -1 == listen(_sock, backlog) ) {
                err = XPlat::NetUtils::GetLastError();
                if( XPlat::Error::EAddrInUse( err ) ) {
                    ++_port;
                    continue;
                }
                else {
                    err_str = XPlat::Error::GetErrorString( err );
                    xplat_dbg( 1, xplat_printf(FLF, stderr,
                                          "listen() failed with '%s'\n",
                                          err_str.c_str() ) );
                    XPlat::SocketUtils::Close( _sock );
                    return false;
                }
            }
            success = true;
        }
    } while( ! success );
#endif

    // Turn off Nagle algorithm
    if( ! SetTcpNoDelay(_sock) ) {
        xplat_dbg( 1, xplat_printf(FLF, stderr,
                              "failed to set TCP_NODELAY\n") );
    }

    port = _port;
    sock = _sock;
    xplat_dbg( 3, xplat_printf(FLF, stderr,
                           "returning sock:%d, port:%hu\n",
                           sock, port) );
    return true;
}
 
bool AcceptConnection( const XPlat_Socket listen_sock, 
                       XPlat_Socket& connected_sock,
                       int timeout_sec /*=0*/,
                       bool nonblock /*=false*/ )
{
    int err;
    XPlat_Socket connection;
    std::string err_str;

    xplat_dbg( 3, xplat_printf(FLF, stderr,
                          "listening on socket=%d\n", listen_sock) );

    connected_sock = InvalidSocket;

    if( nonblock && (0 == timeout_sec) )
        timeout_sec = 1;

    do {
        if( timeout_sec > 0 ) {
            // use select with timeout
            fd_set readfds;
            FD_ZERO( &readfds );
            FD_SET( listen_sock, &readfds );
            
            struct timeval tv = {timeout_sec, 0};
        
            int maxfd = listen_sock + 1;
            int retval = select( maxfd, &readfds, NULL, NULL, &tv );
            err = XPlat::NetUtils::GetLastError();
            if( retval == 0 ) { // timed-out
                return false; // let our caller decide what's next
            }
            else if( retval < 0 ) {
                err_str = XPlat::Error::GetErrorString( err );
                xplat_dbg( 1, xplat_printf(FLF, stderr,
                                      "select() failed with '%s'\n", 
                                      err_str.c_str()) );
                return false;
            }
        }

        // now try to accept a connection
        connection = accept( listen_sock, NULL, NULL );
        if( -1 == connection ) {
            err = XPlat::NetUtils::GetLastError();
            if( nonblock && (EWOULDBLOCK == err) )
                return false; // let our caller decide what's next
        if( EWOULDBLOCK != err ) {
                err_str = XPlat::Error::GetErrorString( err );
                xplat_dbg( 1, xplat_printf(FLF, stderr,
                                      "accept() failed with '%s'\n", 
                               err_str.c_str()) );
        } 
            if( EINTR != err )
                return false;
        }
    } while( -1 == connection );

    // Set the socket to be blocking
    if( ! SetBlockingMode(connection, true) )
        xplat_dbg( 1, xplat_printf(FLF, stderr,
                              "failed to set blocking\n") );

    // Close socket on exec
    if( ! SetCloseOnExec(connection) ) {
        xplat_dbg( 1, xplat_printf(FLF, stderr,
                              "failed to set close-on-exec\n") );     
    }

    // Turn off Nagle algorithm
    if( ! SetTcpNoDelay(connection) ) {
        xplat_dbg( 1, xplat_printf(FLF, stderr,
                              "failed to set TCP_NODELAY\n") );
    }

    xplat_dbg( 3, xplat_printf(FLF, stderr,
                          "returning socket=%d\n", connection) );
    connected_sock = connection;
    return true;
}

bool GetPort( const XPlat_Socket sock, XPlat_Port& port )
{
    struct sockaddr_in local_addr;
    socklen_t sockaddr_len = sizeof( local_addr );
    if( getsockname( sock, (struct sockaddr*) &local_addr, &sockaddr_len ) == -1 ) {
        int err = XPlat::NetUtils::GetLastError();
        std::string err_str = XPlat::Error::GetErrorString( err );
        xplat_dbg( 1, xplat_printf(FLF, stderr,
                              "getsockname(%d) failed with %s\n",
                              sock, err_str.c_str()) );
        port = InvalidPort;
        return false;
    }

    port = ntohs( local_addr.sin_port );
    return true;
}

bool SetOption( XPlat_Socket sock, int level,
                int option, void* optval, socklen_t optsz )
{
#ifndef os_windows
    int ssoret = setsockopt( sock,
                             level,
                             option,
                             optval,
                             optsz );
#else
    int ssoret = setsockopt( sock,
                             level,
                             option,
                             (char *)optval,
                             optsz );
#endif
    if( ssoret == -1 ) return false;
    return true;
}

} // namespace SocketUtils

} // namespace XPlat

