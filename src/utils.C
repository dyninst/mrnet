/****************************************************************************
 *  Copyright 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "utils.h"
#include "mrnet/MRNet.h"

#include "xplat/NetUtils.h"
#include "xplat/PathUtils.h"
#include "xplat/SocketUtils.h"
#include "xplat/Mutex.h"
#include "xplat/Error.h"
#include "xplat/Process.h"
using namespace XPlat;

#include <sys/stat.h>

#include <string>
#include <map>

// Some versions of boost have this in a timer 
// subfolder. 
#ifdef USE_BOOST_TIMER
#   include <boost/timer/timer.hpp>
#endif

#if !defined(os_windows)
# include <sys/time.h>
# include <net/if.h>
# include <netdb.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
#else
#define sleep(x) Sleep(1000*(DWORD)x)
#include <fcntl.h>
#endif // defined(os_windows)

#if defined(os_solaris)
# include <sys/sockio.h>         //only for solaris
#endif // defined(os_solaris)

static XPlat::Mutex mrn_printf_mutex;

namespace MRN
{

std::string LocalHostName = NULL_STRING;
std::string LocalDomainName = NULL_STRING;
std::string LocalNetworkName = NULL_STRING;
std::string LocalNetworkAddr = NULL_STRING;

Port LocalPort = 0;
double Timer::offset = 0;
bool Timer::first_time = true;

XPlat::TLSKey tsd_key;

void get_Version( int& major,
                  int& minor,
                  int& revision )
{
    major = MRNET_VERSION_MAJOR;
    minor = MRNET_VERSION_MINOR;
    revision = MRNET_VERSION_REV;
}

int connectHost( int *sock_in, const std::string & hostname, Port port, 
                 int num_retry /*=-1*/ )
{

    int err, sock = *sock_in;
    struct sockaddr_in server_addr;
    const char* host = hostname.c_str();

    mrn_dbg( 3, mrn_printf(FLF, stderr, "In connectHost(%s:%hu) sock:%d ...\n",
                           host, port, sock ) );

    if( sock <= 0 ) {
        sock = socket( AF_INET, SOCK_STREAM, 0 );
        if( sock == -1 ) {
            err = XPlat::NetUtils::GetLastError();
            mrn_dbg( 1, mrn_printf(FLF, stderr, "socket() failed, errno=%d\n", err) );
            return -1;
        }
        mrn_dbg( 5, mrn_printf(FLF, stderr, "socket() => %d\n", sock ));
    }

    memset( &server_addr, 0, sizeof( server_addr ) );
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons( port );

    XPlat::NetUtils::NetworkAddress naddr;
    int rc = XPlat::NetUtils::GetNetworkAddress( hostname, naddr );
    if( rc == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, 
                               "failed to convert name %s to network address\n",
                               host) );
        return -1;
    }

    in_addr_t addr = naddr.GetInAddr();
    memcpy( &server_addr.sin_addr, &addr, sizeof(addr) );

    int nConnectTries = 0;
    int cret = -1;
    do {
        cret = connect( sock, (sockaddr *) & server_addr, sizeof( server_addr ) );
        if( cret == -1 ) {
            err = XPlat::NetUtils::GetLastError();
            if( ! ( XPlat::Error::ETimedOut(err) || XPlat::Error::EConnRefused(err) ) ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "connect() to %s:%hu failed: %s\n",
                                       host, port,
                                       XPlat::Error::GetErrorString( err ).c_str()) );
                XPlat::SocketUtils::Close( sock );
                return -1;
            }

	    nConnectTries++;
            mrn_dbg( 3, mrn_printf(FLF, stderr, "connect() to %s:%hu timed out %d times\n",
                                   host, port, nConnectTries) );
	    if( (num_retry > 0) && (nConnectTries >= num_retry) )
                break;

	    // delay before trying again (more each time)
	    sleep( nConnectTries );
        }
    } while( cret == -1 );

    if( cret == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "connect() to %s:%hu failed with '%s' after %d tries\n",
                               host, port,
			       XPlat::Error::GetErrorString(err).c_str(),
                               nConnectTries) );
        XPlat::SocketUtils::Close( sock );
        return -1;
    }

#ifndef os_windows
    // Close socket on exec
    int fdflag = fcntl(sock, F_GETFD );
    if( fdflag == -1 ) {
        // failed to retrieve the socket descriptor flags
        mrn_dbg( 1, mrn_printf(FLF, stderr, "F_GETFD failed\n") );
    }
    int fret = fcntl( sock, F_SETFD, fdflag | FD_CLOEXEC );
    if( fret == -1 ) {
        // we failed to set the socket descriptor flags
        mrn_dbg( 1, mrn_printf(FLF, stderr, "F_SETFD close-on-exec failed\n") );
    }
#endif

#if defined(TCP_NODELAY)
    // turn off Nagle algorithm for coalescing packets
    int optVal = 1;
    int ssoret = setsockopt( sock,
                             IPPROTO_TCP,
                             TCP_NODELAY,
                             (const char*)&optVal,
                             sizeof( optVal ) );
    if( ssoret == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "failed to set TCP_NODELAY\n" ) );
    }
#endif

    mrn_dbg( 3, mrn_printf(FLF, stderr,
                "Leaving Connect_to_host(). Returning sock: %d\n", sock ) );
    *sock_in = sock;
    return 0;
}

int bindPort( int *sock_in, Port *port_in, bool nonblock /*=false*/ )
{
    int err, sock;
    Port port = *port_in;

    if( port == UnknownPort )
        port = 0;

    sock = socket( AF_INET, SOCK_STREAM, 0 );
    if( sock == -1 ) {
        err = XPlat::NetUtils::GetLastError();
        mrn_dbg( 1, mrn_printf(FLF, stderr, "socket() failed: %s\n",
                               XPlat::Error::GetErrorString(err).c_str() ) );
        return -1;
    }

    mrn_dbg( 3, mrn_printf(FLF, stderr, "In bindPort(sock:%d, port:%d)\n",
                           sock, port) );

    // Close socket on exec
#ifndef os_windows
    int fdflag, fret;
    fdflag = fcntl(sock, F_GETFD );
    if( fdflag == -1 ) {
        // failed to retrieve the socket descriptor flags
        mrn_dbg( 1, mrn_printf(FLF, stderr, "F_GETFD failed\n") );     
    }
    else {
        fret = fcntl( sock, F_SETFD, fdflag | FD_CLOEXEC );
        if( fret == -1 ) {
            // failed to set the socket descriptor flags
            mrn_dbg( 1, mrn_printf(FLF, stderr, "F_SETFD close-on-exec failed\n") );
        }
    }
#endif

    // Set listening socket to non-blocking if requested
    if( nonblock ) {

        mrn_dbg( 5, mrn_printf(FLF, stderr, 
                               "Setting listening socket to non blocking\n") );

#ifndef os_windows
        fdflag = fcntl(sock, F_GETFL );
        if( fdflag == -1 ) {
            // failed to retrieve the socket status flags
            mrn_dbg( 1, mrn_printf(FLF, stderr, "F_GETFL failed\n") );
        }
        else {
            fret = fcntl( sock, F_SETFL, fdflag | O_NONBLOCK );
            if( fret == -1 ) {
                // failed to set the socket status flags
                mrn_dbg( 1, mrn_printf(FLF, stderr, 
                        "failed to set listening socket to non blocking\n") );
            }
        }
#else
        unsigned long mode = 1; // 0 is blocking, !0 is non-blocking
        if( ioctlsocket(sock, FIONBIO, &mode) != 0 ) {
            // failed to set the socket flags
            mrn_dbg( 1, mrn_printf(FLF, stderr, 
                                   "failed to set listening socket to non blocking\n") );
        }
#endif
    }


    struct sockaddr_in local_addr;
    memset( &local_addr, 0, sizeof( local_addr ) );
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl( INADDR_ANY );

    if( port != 0 ) {

#ifndef os_windows
        // set the socket so that it does not hold onto its port after
        // the process exits (needed because on at least some platforms we
        // use well-known ports when connecting sockets)
        int soVal = 1;
        int soRet = setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, 
                                (const char*)&soVal, sizeof(soVal) );
        
        if( soRet < 0 ) {
            err = XPlat::NetUtils::GetLastError();
            mrn_dbg( 1, mrn_printf(FLF, stderr, "setsockopt() failed: %s\n",
                                   XPlat::Error::GetErrorString(err).c_str() ) );
        }
#endif
        // try to bind and listen using the supplied port
        local_addr.sin_port = htons( port );
        if( bind(sock, (sockaddr*)&local_addr, sizeof(local_addr)) == -1 ) {
            err = XPlat::NetUtils::GetLastError();
            mrn_dbg( 1, mrn_printf(FLF, stderr, "bind() to static port %d failed: %s\n",
                                   port, XPlat::Error::GetErrorString(err).c_str() ) );
            XPlat::SocketUtils::Close( sock );
            return -1;
        }
    }
    // else, the system will assign a port for us in listen

#ifndef os_windows
    if( listen(sock, 128) == -1 ) {
        err = XPlat::NetUtils::GetLastError();
        mrn_dbg( 1, mrn_printf(FLF, stderr, "listen() failed: %s\n",
                               XPlat::Error::GetErrorString(err).c_str() ) );
        XPlat::SocketUtils::Close( sock );
        return -1;
    }

    // we have a listening socket
    *sock_in = sock;

    // determine which port we were actually assigned to
    if( getPortFromSocket( sock, port_in ) != 0 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr,
            "failed to obtain port from socket\n" ) );
        XPlat::SocketUtils::Close( sock );
        return -1;
    }
#else
    // let the system assign a port, start from 1st dynamic port
    port = 49152;
    bool success = false;

    do {
        local_addr.sin_port = htons( port );
        if( bind(sock, (sockaddr*)&local_addr, sizeof(local_addr)) != 0/*== -1*/ ) {
            err = XPlat::NetUtils::GetLastError();
            if( XPlat::Error::EAddrInUse( err ) ) {
                ++port;
                continue;
            }
            else {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "bind() to dynamic port %d failed: %s\n",
                            port, XPlat::Error::GetErrorString( err ).c_str() ) );
                XPlat::SocketUtils::Close( sock );
                return -1;
            }
        }
        else {
            if( listen(sock, 64) == -1 ) {
                err = XPlat::NetUtils::GetLastError();
                if( XPlat::Error::EAddrInUse( err ) ) {
                    ++port;
                    continue;
                }
                else {
                    mrn_dbg( 1, mrn_printf(FLF, stderr, "listen() failed: %s\n",
                                XPlat::Error::GetErrorString( err ).c_str() ) );
                    XPlat::SocketUtils::Close( sock );
                    return -1;
                }
            }
            success = true;
        }
    } while( ! success );

    *sock_in = sock;
    *port_in = port;
#endif



#if defined(TCP_NODELAY)
    // turn off Nagle algorithm for coalescing packets
    int optVal = 1;
    int ssoret = setsockopt( sock,
                             IPPROTO_TCP,
                             TCP_NODELAY,
                             (const char*)&optVal,
                             sizeof( optVal ) );
    if( ssoret == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "failed to set TCP_NODELAY\n" ) );
    }
#endif // defined(TCP_NODELAY)

    mrn_dbg( 3, mrn_printf(FLF, stderr,
                           "Leaving bindPort(). Returning sock:%d, port:%d\n",
                           sock, *port_in ) );
    return 0;
}

int getSocketConnection( int bound_socket , int& inout_errno,
                         int timeout_sec /*=0*/ , bool nonblock /*=false*/ )
{
    int connected_socket;
    int fdflag;

    mrn_dbg( 3, mrn_printf(FLF, stderr, "In get_connection(sock:%d).\n",
                bound_socket ) );

    if( nonblock && (timeout_sec == 0) )
        timeout_sec = 5;

    do {
        if( timeout_sec > 0 ) {
            // use select with timeout
            fd_set readfds;
            FD_ZERO( &readfds );
            FD_SET( bound_socket, &readfds );
            
            struct timeval tv = {timeout_sec, 0};
        
            int maxfd = bound_socket + 1;
            int retval = select( maxfd, &readfds, NULL, NULL, &tv );
            inout_errno = XPlat::NetUtils::GetLastError();
            if( retval == 0 ) { // timed-out
                if( ! nonblock )
                    return -1; // let our caller decide what's next

                continue;
            }
            else if( retval < 0 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "select() failed with '%s'\n", 
		                       strerror(inout_errno)) );
                return -1;
            }
        }
        connected_socket = accept( bound_socket, NULL, NULL );
        if( connected_socket == -1 ) {
            inout_errno = XPlat::NetUtils::GetLastError();
            if( nonblock && (inout_errno == EWOULDBLOCK) )
                continue;
	    if( inout_errno != EWOULDBLOCK ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "accept() failed with '%s'\n", 
		                       strerror(inout_errno)) );
	    } 
            if( inout_errno != EINTR )
                return -1;
        }
    } while( (connected_socket == -1) && (inout_errno == EINTR) );

    if( -1 == connected_socket )
        return -1;

    // Set the socket to be blocking
    mrn_dbg( 5, mrn_printf(FLF, stderr, 
                           "Setting accepted connection socket to blocking\n") );
#ifndef os_windows
    int fret;
    fdflag = fcntl(connected_socket, F_GETFL );
    if( fdflag == -1 ) {
        // failed to retrieve the socket status flags
        mrn_dbg( 1, mrn_printf(FLF, stderr, "F_GETFL failed\n") );
    }
    else if( fdflag & O_NONBLOCK ) {
        fret = fcntl( connected_socket, F_SETFL, fdflag & ~O_NONBLOCK );
        if(fret == -1 ) {
            // failed to set the socket status flags
            mrn_dbg( 1, mrn_printf(FLF, stderr, 
                                   "Setting socket connection to blocking failed\n") );
        }
    }
#else
    unsigned long mode = 0; // 0 is blocking, !0 is non-blocking
    if (ioctlsocket(connected_socket, FIONBIO, &mode) != 0) {
        // failed to set the socket flags
        mrn_dbg( 1, mrn_printf(FLF, stderr, 
                               "Setting socket connection to blocking failed\n") );
    }
#endif

#ifndef os_windows
    // Close socket on exec
    fdflag = fcntl( connected_socket, F_GETFD );
    if( fdflag == -1 ) {
        // failed to retrieve the socket descriptor flags 
        mrn_dbg( 1, mrn_printf(FLF, stderr, "F_GETFD failed\n") );    
    }
    fret = fcntl( connected_socket, F_SETFD, fdflag | FD_CLOEXEC );
    if( fret == -1 ) {
        // we failed to set the socket descriptor flags
        mrn_dbg( 1, mrn_printf(FLF, stderr, "F_SETFD close-on-exec failed\n") );
    }
#endif

#if defined(TCP_NODELAY)
    // turn off Nagle algorithm for coalescing packets
    int optVal = 1;
    int ssoret = setsockopt( connected_socket,
                             IPPROTO_TCP,
                             TCP_NODELAY,
                             (const char*)&optVal,
                             sizeof( optVal ) );
    if( ssoret == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "failed to set TCP_NODELAY\n") );
    }
#endif

    mrn_dbg( 3, mrn_printf(FLF, stderr,
                           "Leaving get_connection(). Returning sock:%d\n",
                           connected_socket) );
    return connected_socket;
}


int getPortFromSocket( int sock, Port *port )
{
    struct sockaddr_in local_addr;
    socklen_t sockaddr_len = sizeof( local_addr );

    if( getsockname( sock, (sockaddr*) & local_addr, &sockaddr_len ) == -1 ) {
        perror( "getsockname" );
        return -1;
    }

    *port = ntohs( local_addr.sin_port );
    return 0;
}

extern char* MRN_DEBUG_LOG_DIRECTORY;

static FILE* mrn_printf_fp = NULL;

void mrn_printf_init( FILE* ifp )
{
    mrn_printf_fp = ifp;
} 

void mrn_printf_setup( int rank, node_type_t type )
{
    if( mrn_printf_fp == NULL ) {

        std::string this_host;
        struct stat s;
        char node_type[3];
        char host[256];
        char logdir[256];
        char logfile[512];
        logfile[0] = '\0';

        switch( type ) {
        case FE_NODE:
            sprintf( node_type, "FE" );
            break;
        case BE_NODE:
            sprintf( node_type, "BE" );
            break;
        case CP_NODE:
            sprintf( node_type, "CP" );
            break;
        default:
            return;
        };

        XPlat::NetUtils::GetLocalHostName(this_host);
        strncpy( host, this_host.c_str(), 256 );
        host[255] = '\0';

        // find log directory
        const char* varval = MRN_DEBUG_LOG_DIRECTORY;
        if( varval != NULL ) {
            if( (stat(varval, &s) == 0) && (S_IFDIR & s.st_mode) )
                snprintf( logdir, sizeof(logdir), "%s", varval );
        }
    
        // set file name format
        int pid = XPlat::Process::GetProcessId();
        snprintf( logfile, sizeof(logfile), "%s/%s_%s_%d.%d",
                  logdir, node_type, host, rank, pid );

        mrn_printf_fp = fopen( logfile, "w" );
    }
}

int mrn_printf( const char *file, int line, const char * func,
                FILE * ifp, const char *format, ... )
{
    static bool retry = true;

    int retval = 1;
    va_list arglist;

    struct timeval tv;
    while( gettimeofday( &tv, NULL ) == -1 ) {}
    
    mrn_printf_mutex.Lock();

    // get thread info
    XPlat::Thread::Id tid = 0;
    const char* thread_name = NULL;
    Rank rank = UnknownRank;
    node_type_t node_type = UNKNOWN_NODE;
    Network* net = NULL;
    
    tsd_t *tsd = (tsd_t*) tsd_key.Get();
    if( tsd != NULL ) {
        tid = tsd->thread_id;
        thread_name = tsd->thread_name;
        rank = tsd->process_rank;
        node_type = tsd->node_type;
        net = tsd->network;
    }

    if( mrn_printf_fp == NULL ) {
        if( (MRN_DEBUG_LOG_DIRECTORY != NULL) && retry ) { 
            // try to open log file
            if( (rank != UnknownRank) &&
                (node_type != UNKNOWN_NODE) )
                mrn_printf_setup( rank, node_type );
        }
    }

    FILE *f = mrn_printf_fp;
    if( f == NULL ) {
        f = ifp;
        if( MRN_DEBUG_LOG_DIRECTORY != NULL )
            retry = false;
    }

    // print timestamp and thread info
    fprintf( f, "%ld.%06ld: %s(0x%lx): ", 
             tv.tv_sec-MRN_RELEASE_DATE_SECS, tv.tv_usec,
             ( thread_name != NULL ) ? thread_name : "UNKNOWN_THREAD",
             tid );

    if( file ) {
        // print file, function, and line info
        fprintf( f, "%s[%d] %s() - ", 
                 XPlat::PathUtils::GetFilename( file ).c_str(), line, func );
    }

    // print message
    va_start( arglist, format );
    retval = vfprintf( f, format, arglist );
    va_end( arglist );
    
    mrn_printf_mutex.Unlock();

    fflush( f );
     
    return retval;
}

/* Convert struct timeval to a double */
double tv2dbl( struct timeval tv)
{
    return ((double)tv.tv_sec + (double)tv.tv_usec / 1000000.0);
}

/* Convert a double to a struct timeval */
struct timeval dbl2tv(double d) 
{
  struct timeval tv;

  tv.tv_sec = (long) d;
  tv.tv_usec = (long) ((d - (long) d) * 1000000.0);
  
  return tv;
}

void Timer::start( void ){
#ifdef USE_BOOST_TIMER 
    _b_timer.start();
#else
    while(gettimeofday(&_start_tv, NULL) == -1) {}
    //fprintf(stderr, "offset: %lf secs\n", offset/1000.0 );
    _start_d = tv2dbl( _start_tv ) + ( offset / 1000.0 );
    _start_tv = dbl2tv( _start_d );
#endif
}
    
void Timer::stop( void ){
#ifdef USE_BOOST_TIMER
    _b_timer.stop();
#else 
    while(gettimeofday(&_stop_tv, NULL) == -1) {}
    //fprintf(stderr, "offset: %lf secs\n", offset/1000.0 );
    _stop_d = tv2dbl( _stop_tv ) + ( offset / 1000.0 );
    _stop_tv = dbl2tv( _stop_d );
#endif
}

double Timer::get_latency_secs( ) {
#ifdef USE_BOOST_TIMER
    // No need to check if timer is started, boost does that for us.
    return double(_b_timer.elapsed().user / 1000000000.0);
#else
    if (_start_d == 0.0 || _stop_d == 0.0)
        return 0.0;
    return _stop_d - _start_d;
#endif
}
    
double Timer::get_latency_msecs( ) {
    return 1000 * get_latency_secs();
}

double Timer::get_latency_usecs( ) {
    return 1000000 * get_latency_secs();
}

Timer::Timer( void ) {
#ifndef USE_BOOST_TIMER
    _stop_d = 0;
    _start_d = 0;
#endif
}

bool isBigEndian() {
    unsigned int one = 1;
    unsigned char * arr = (unsigned char *)&one;

    if (!arr[0])
        return true;
    else
        return false;   
}

// void endianTest() {
// #if defined(WORDS_BIGENDIAN)
//     mrn_dbg(5, mrn_printf(FLF, stderr, "config says BIG_ENDIAN\n"));
// #else
//     mrn_dbg(5, mrn_printf(FLF, stderr, "config says LITTLE_ENDIAN\n"));
// #endif

//     if (isBigEndian()) {
//         mrn_dbg(5, mrn_printf(FLF, stderr, "test returns big endian\n"));
//     } else {
//         mrn_dbg(5, mrn_printf(FLF, stderr, "test returns little endian\n"));
//     }

// }

} // namespace MRN

