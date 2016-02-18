/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "xplat_lightweight/xplat_utils_lightweight.h"
#include "xplat_lightweight/Error.h"
#include "xplat_lightweight/NetUtils.h"
#include "xplat_lightweight/SocketUtils.h"

// Functions implemented herein are (mostly) cross-platform

#ifndef os_windows
const XPlat_Socket InvalidSocket = -1;
#else
const XPlat_Socket InvalidSocket = INVALID_SOCKET;
#endif
const XPlat_Port InvalidPort = (XPlat_Port)0;

static bool_t SetTcpNoDelay( XPlat_Socket sock )
{
#if defined(TCP_NODELAY)
    // turn off Nagle algorithm for coalescing packets
    int optval = 1;
    bool_t soret = XPlat_SocketUtils_SetOption( sock, 
                                                IPPROTO_TCP, 
                                                TCP_NODELAY,
                                                (void*) &optval, 
                                                (socklen_t) sizeof(optval) );
    if( ! soret ) {
        xplat_dbg( 1, xplat_printf(FLF, stderr, 
                              "failed to set option\n") );
        return false;
    }
#else
    xplat_dbg(1, xplat_printf(FLF, stderr,
                "WARNING: TCP_NODELAY not found! Performance WILL suffer.\n"));
#endif
    return true;
}

static bool_t SetCloseOnExec( XPlat_Socket sock )
{
#ifndef os_windows
    int fdflag, fret;
    fdflag = fcntl( sock, F_GETFD );
    if( fdflag == -1 ) {
        // failed to retrieve the socket descriptor flags
        xplat_dbg( 1, xplat_printf(FLF, stderr,
                              "failed to get flags\n") );     
	return false;
    }
    else {
        fret = fcntl( sock, F_SETFD, fdflag | FD_CLOEXEC );
        if( fret == -1 ) {
            // failed to set the socket descriptor flags
            xplat_dbg( 1, xplat_printf(FLF, stderr,
                              "failed to set flags\n") );
	    return false;
        }
    }
#endif
    return true;
}

bool_t XPlat_SocketUtils_Connect( const char* host,
                                  const XPlat_Port port,
                                  XPlat_Socket* sock,
                                  unsigned num_retry )
{
    unsigned nConnectTries = 0;
    int err, cret = -1;
    XPlat_Socket _sock = *sock;
    const char* err_str = NULL;
    struct hostent *server_hostent = NULL;
    struct sockaddr_in server_addr;

    xplat_dbg( 3, xplat_printf(FLF, stderr,
                          "host=%s port=%hu sock=%d\n",
                          host, port, _sock) );

    if( InvalidSocket == _sock ) {
        _sock = socket( AF_INET, SOCK_STREAM, 0 );
        if ( InvalidSocket == _sock ) {
            err = XPlat_NetUtils_GetLastError();
            err_str = XPlat_Error_GetErrorString(err);
            xplat_dbg( 1, xplat_printf(FLF, stderr,
                                  "socket() failed with '%s'\n", err_str) );
            return false;
        }
        xplat_dbg( 5, xplat_printf(FLF, stderr,
                              "socket() => %d\n", _sock) );
    }

    server_hostent = gethostbyname( host ); 
    if ( NULL == server_hostent ) {
        err = XPlat_NetUtils_GetLastError();
        err_str = XPlat_Error_GetErrorString(err);
        xplat_dbg( 1, xplat_printf(FLF, stderr,
                              "gethostbyname() failed with '%s'\n", err_str) );
        return false;
    } 

    memset( &server_addr, 0, sizeof(server_addr) );
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons( port );
    memcpy( &server_addr.sin_addr, server_hostent->h_addr_list[0],
            sizeof(struct in_addr) );

    do {
        cret = connect( _sock, (struct sockaddr *) &server_addr, 
                        (socklen_t) sizeof(server_addr) );
        if( -1 == cret ) {
            err = XPlat_NetUtils_GetLastError();
            err_str = XPlat_Error_GetErrorString(err);
            xplat_dbg( 5, xplat_printf(FLF, stderr,
                                  "connect() failed with '%s'\n", err_str) );
            if( ! (XPlat_Error_ETimedOut(err) || 
                   XPlat_Error_EConnRefused(err)) ) {
                xplat_dbg(1, xplat_printf(FLF, stderr,
                                     "connect() to %s:%hu failed with '%s'\n", 
                                      host, port, err_str));
                return false;
            }

            nConnectTries++;
            xplat_dbg( 3, xplat_printf(FLF, stderr,
                                  "timed out %d times\n", nConnectTries) );
            if( (num_retry > 0) && (nConnectTries >= num_retry) )
                break;

            // delay before trying again (more each time)
            sleep( nConnectTries );
        }
    } while( -1 == cret );

    if( -1 == cret ) {
        err_str = XPlat_Error_GetErrorString(err);
        xplat_dbg( 1, xplat_printf(FLF, stderr,
                              "connect() to %s:%d failed with '%s'\n", 
                              host, port, err_str) );
        return false;
    }

    xplat_dbg( 5, xplat_printf(FLF, stderr,
                          "connected to %s:%d \n", host, port) );

    // Close socket on exec
    if( ! SetCloseOnExec(_sock) ) {
        xplat_dbg( 1, xplat_printf(FLF, stderr, "XPlat_SocketUtils_Connect - " 
                              "failed to set close-on-exec\n") );
    }

    // Turn off Nagle algorithm
    if( ! SetTcpNoDelay(_sock) ) {
        xplat_dbg( 1, xplat_printf(FLF, stderr, 
                              "failed to set TCP_NODELAY\n") );
    }

    xplat_dbg( 3, xplat_printf(FLF, stderr,             
                          "returning socket=%d\n", _sock) );
    *sock = _sock;
    return true;
}

bool_t XPlat_SocketUtils_CreateListening( XPlat_Socket* sock, 
                                          XPlat_Port* port, 
                                          bool_t nonblock )
{
    static int backlog = 128;
    int err, optval;
    bool_t soret;
    XPlat_Socket _sock;
    XPlat_Port _port = *port;
    const char* err_str = NULL;
    struct sockaddr_in local_addr;

    if( InvalidPort == _port )
        _port = 0;

    _sock = socket( AF_INET, SOCK_STREAM, 0 );
    if( -1 == _sock ) {
        err = XPlat_NetUtils_GetLastError();
        err_str = XPlat_Error_GetErrorString(err);
        xplat_dbg( 1, xplat_printf(FLF, stderr,
                              "socket() failed with '%s'\n", err_str) );
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
        if( ! XPlat_SocketUtils_SetBlockingMode(_sock, false) )
            xplat_dbg( 1, xplat_printf(FLF, stderr,
                                  "failed to set non-blocking\n") );
    }

#ifndef os_windows
    /* Set the socket so that it does not hold onto its port after
     * the process exits (needed because on at least some platforms we
     * use well-known ports when connecting sockets) */
    optval = 1;
    soret = XPlat_SocketUtils_SetOption( _sock, SOL_SOCKET, SO_REUSEADDR, 
                                         (void*) &optval, 
                                         (socklen_t) sizeof(optval) );
    if( ! soret ) {
        err = XPlat_NetUtils_GetLastError();
        err_str = XPlat_Error_GetErrorString(err);
        xplat_dbg( 1, xplat_printf(FLF, stderr,
                              "setsockopt() failed with '%s'\n", err_str) );
    }
#endif

    memset( &local_addr, 0, sizeof(local_addr) );
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl( INADDR_ANY );

    if( 0 != _port ) {
        // try to bind and listen using the supplied port
        local_addr.sin_port = htons( _port );
        if( -1 == bind(_sock, (struct sockaddr*)&local_addr, sizeof(local_addr)) ) {
            err = XPlat_NetUtils_GetLastError();
            err_str = XPlat_Error_GetErrorString(err);
            xplat_dbg( 1, xplat_printf(FLF, stderr,
                                  "bind() to static port %d failed with '%s'\n",
                                   _port, err_str) );
            XPlat_SocketUtils_Close( _sock );
            return false;
        }
    }

#ifndef os_windows
    // else, the system will assign a port for us in listen
    if( -1 == listen(_sock, backlog) ) {
        err = XPlat_NetUtils_GetLastError();
        err_str = XPlat_Error_GetErrorString(err);
        xplat_dbg( 1, xplat_printf(FLF, stderr,
                              "listen() failed with '%s'\n", err_str) );
        XPlat_SocketUtils_Close( _sock );
        return false;
    }
    // determine which port we were actually assigned to
    if( ! XPlat_SocketUtils_GetPort(_sock, &_port) ) {
        xplat_dbg( 1, xplat_printf(FLF, stderr,
                              "failed to obtain port from socket\n" ) );
        XPlat_SocketUtils_Close( _sock );
        return false;
    }
#else
    // try binding ports, starting from 1st dynamic port
    _port = 49152;
    success = false;
    do {
        local_addr.sin_port = htons( _port );
        if( -1 == bind(_sock, (struct sockaddr*)&local_addr, sizeof(local_addr)) ) {
            err = XPlat_NetUtils_GetLastError();
            if( XPlat_Error_EAddrInUse( err ) ) {
                ++_port;
                continue;
            }
            else {
                err_str = XPlat_Error_GetErrorString(err);
                xplat_dbg( 1, xplat_printf(FLF, stderr,
                                      "bind() to dynamic port %d failed with '%s'\n",
                                      _port, err_str) );
                XPlat_SocketUtils_Close( _sock );
                return false;
            }
        }
        else {
            if( -1 == listen(_sock, backlog) ) {
                err = XPlat_NetUtils_GetLastError();
                if( XPlat_Error_EAddrInUse( err ) ) {
                    ++_port;
                    continue;
                }
                else {
                    err_str = XPlat_Error_GetErrorString(err);
                    xplat_dbg( 1, xplat_printf(FLF, stderr,
                                          "listen() failed with '%s'\n", err_str) );
                    XPlat_SocketUtils_Close( _sock );
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

    *port = _port;
    *sock = _sock;
    xplat_dbg( 3, xplat_printf(FLF, stderr,
                          "returning socket=%d, port=%hu\n", _sock, _port) );
 
    return true;
}

bool_t XPlat_SocketUtils_AcceptConnection( XPlat_Socket listen_sock, 
                                           XPlat_Socket* connected_sock,
                                           int timeout_sec,
                                           bool_t nonblock )
{
    int err, maxfd, retval;
    XPlat_Socket connection;
    const char* err_str;
    fd_set readfds;
    struct timeval tv;

    xplat_dbg( 3, xplat_printf(FLF, stderr,
                          "listening on socket=%d\n", listen_sock) );

    *connected_sock = InvalidSocket;

    if( nonblock && (0 == timeout_sec) )
        timeout_sec = 1;

    do {
        if( timeout_sec > 0 ) {
            // use select with timeout
            FD_ZERO( &readfds );
            FD_SET( listen_sock, &readfds );
            
            tv.tv_sec = timeout_sec;
            tv.tv_usec = 0;
        
            maxfd = listen_sock + 1;
            retval = select( maxfd, &readfds, NULL, NULL, &tv );
            err = XPlat_NetUtils_GetLastError();
            if( retval == 0 ) { // timed-out
                return false; // let caller decide what's next
            }
            else if( retval < 0 ) {
                err_str = XPlat_Error_GetErrorString(err);
                xplat_dbg( 1, xplat_printf(FLF, stderr,
                                      "select() failed with '%s'\n", err_str) );
                return false;
            }
        }

        // now try to accept a connection
        connection = accept( listen_sock, NULL, NULL );
        if( -1 == connection ) {
            err = XPlat_NetUtils_GetLastError();
            if( nonblock && (EWOULDBLOCK == err) )
                return false; // let caller decide what's next
	    if( EWOULDBLOCK != err ) {
                err_str = XPlat_Error_GetErrorString(err);
                xplat_dbg( 1, xplat_printf(FLF, stderr,
                                      "accept() failed with '%s'\n", err_str) );
	    } 
            if( EINTR != err )
                return false;
        }
    } while( -1 == connection );

    // Set the socket to be blocking
    if( ! XPlat_SocketUtils_SetBlockingMode(connection, true) )
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
    *connected_sock = connection;
    return true;
}

bool_t XPlat_SocketUtils_GetPort( const XPlat_Socket sock, 
                                  XPlat_Port* port )
{
    const char* err_str;
    int err;
    struct sockaddr_in local_addr;
    socklen_t sockaddr_len = sizeof( local_addr );
    if( -1 == getsockname(sock, (struct sockaddr*) &local_addr, &sockaddr_len) ) {
        err = XPlat_NetUtils_GetLastError();
        err_str = XPlat_Error_GetErrorString(err);
        xplat_dbg( 1, xplat_printf(FLF, stderr, 
                              "getsockname(%d) failed with %s\n",
                              sock, err_str) );
        return false;
    }

    *port = ntohs( local_addr.sin_port );
    return 0;
}

bool_t XPlat_SocketUtils_SetOption( XPlat_Socket sock, int level,
                                    int option, void* optval, socklen_t optsz )
{
    int ssoret = setsockopt( sock,
                             level,
                             option,
                             optval,
                             optsz );
    if( ssoret == -1 ) return false;
    return true;
}
