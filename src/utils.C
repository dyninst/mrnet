/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "config.h"
#include "utils.h"

#include "mrnet/Types.h"
#include "mrnet/MRNet.h"

#include "xplat/NetUtils.h"
#include "xplat/PathUtils.h"
#include "xplat/SocketUtils.h"
#include "xplat/Mutex.h"
#include "xplat/Error.h"
#include "xplat/Process.h"
#include "xplat/Thread.h"
using namespace XPlat;

#include <sys/stat.h>

#include <string>
#include <map>

#if !defined(os_windows)
# include <sys/time.h>
# include <net/if.h>
# include <netdb.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
#endif // defined(os_windows)

#if defined(os_solaris)
# include <sys/sockio.h>         //only for solaris
#endif // defined(os_solaris)

static XPlat::Mutex gethostbyname_mutex;

namespace MRN
{

static Rank myrank=UnknownRank;

std::string LocalHostName="";
std::string LocalDomainName="";
std::string LocalNetworkName="";
std::string LocalNetworkAddr="";

Port LocalPort=0;
double Timer::offset=0;
bool Timer::first_time=true;

XPlat::TLSKey tsd_key;

static struct hostent * copy_hostent( struct hostent *);
static void delete_hostent( struct hostent *in );
static struct hostent * mrnet_gethostbyname( const char * name );

struct hostent * copy_hostent( struct hostent *in)
{
    struct hostent * out = new struct hostent;
    unsigned int i=0;

    //copy h_name, h_addrtype, and h_length
    out->h_name = strdup( in->h_name );
    out->h_addrtype = in->h_addrtype;
    out->h_length = in->h_length;

    //deep copy h_aliases
    unsigned int count=0;
    while( in->h_aliases[count] != NULL )
        count++;

    out->h_aliases = new char * [ count+1 ];
    for(i=0; i<count; i++ ){
        out->h_aliases[i] = strdup( in->h_aliases[i] );
    }
    out->h_aliases[count] = NULL;

    //deep copy h_addr_list
    count=0;
    while( in->h_addr_list[count] != 0 )
        count++;

    out->h_addr_list = new char * [ count+1 ];
    for(i=0; i<count; i++ ){
        out->h_addr_list[i] = new char[4];
        out->h_addr_list[i][0] = in->h_addr_list[i][0];
        out->h_addr_list[i][1] = in->h_addr_list[i][1];
        out->h_addr_list[i][2] = in->h_addr_list[i][2];
        out->h_addr_list[i][3] = in->h_addr_list[i][3];
    }
    out->h_addr_list[count] = NULL;

    return out;
}

void delete_hostent( struct hostent *in )
{
    free(in->h_name);

    unsigned int count=0;
    while( in->h_aliases[count] != NULL )
        free( in->h_aliases[count++] );
    delete [] in->h_aliases;

    count=0;
    while( in->h_addr_list[count] != NULL )
        delete [] in->h_addr_list[count++];

    delete [] in->h_addr_list;
    delete in;
}

struct hostent * mrnet_gethostbyname( const char * name )
{

    gethostbyname_mutex.Lock();

    struct hostent * temp_hostent = gethostbyname( name );

    if( temp_hostent == NULL ){
        gethostbyname_mutex.Unlock();
        return NULL;
    }
    struct hostent * ret_hostent = copy_hostent(temp_hostent);
  
    gethostbyname_mutex.Unlock();
    return ret_hostent;
}

int connectHost( int *sock_in, const std::string & hostname, Port port, 
                 int num_retry /*=-1*/ )
{

    int err, sock = *sock_in;
    struct sockaddr_in server_addr;
    struct hostent *server_hostent = NULL;
    const char* host = hostname.c_str();

    mrn_dbg( 3, mrn_printf(FLF, stderr, "In connectHost(%s:%d) sock:%d ...\n",
                           host, port, sock ) );

    if( sock <= 0 ) {
        sock = socket( AF_INET, SOCK_STREAM, 0 );
        if( sock == -1 ) {
            err = XPlat::NetUtils::GetLastError();
            perror( "socket()" );
            mrn_dbg( 1, mrn_printf(FLF, stderr, "socket() failed, errno=%d\n", err) );
            return -1;
        }
        mrn_dbg( 5, mrn_printf(FLF, stderr, "socket() => %d\n", sock ));
    }

    server_hostent = mrnet_gethostbyname( host );
    if( server_hostent == NULL ) {
        perror( "gethostbyname()" );
        return -1;
    }

    memset( &server_addr, 0, sizeof( server_addr ) );
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons( port );
    memcpy( &server_addr.sin_addr, server_hostent->h_addr_list[0],
            sizeof( struct in_addr ) );
    delete_hostent( server_hostent );

    unsigned int nConnectTries = 0;
    int cret = -1;
    do {
        cret = connect( sock, (sockaddr *) & server_addr, sizeof( server_addr ) );
        if( cret == -1 ) {
            err = XPlat::NetUtils::GetLastError();
            if( ! ( XPlat::Error::ETimedOut(err) || XPlat::Error::EConnRefused(err) ) ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "connect() to %s:%d failed: %s\n",
                                       host, port,
                                       XPlat::Error::GetErrorString( err ).c_str()) );
                return -1;
            }

	    nConnectTries++;
            mrn_dbg( 3, mrn_printf(FLF, stderr, "connect() to %s:%d timed out %d times\n",
                                   host, port, nConnectTries) );
	    if( (num_retry > 0) && (nConnectTries >= num_retry) )
                break;

	    // delay before trying again (more each time)
	    sleep( nConnectTries );
        }
    } while( cret == -1 );

    if( cret == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "connect() to %s:%d failed: %s\n",
                               host, port,
			       XPlat::Error::GetErrorString( err ).c_str() ) );
        return -1;
    }

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
                "Leaving Connect_to_host(). Returning sock: %d\n", sock ) );
    *sock_in = sock;
    return 0;
}

int bindPort( int *sock_in, Port *port_in )
{
    int err, soVal, soRet;
    int sock = *sock_in;
    Port port;

    if( *port_in == UnknownPort ){
        port = 0;
    }
    else{
        port = *port_in;
    }

    struct sockaddr_in local_addr;

    mrn_dbg( 3, mrn_printf(FLF, stderr, "In bindPort(sock:%d, port:%d)\n",
                sock, port ) );

    sock = socket( AF_INET, SOCK_STREAM, 0 );
    if( sock == -1 ) {
        err = XPlat::NetUtils::GetLastError();
        perror( "socket()" );
        mrn_dbg( 1, mrn_printf(FLF, stderr, "socket() failed: %s\n",
                                       XPlat::Error::GetErrorString( err ).c_str() ) );
        return -1;
    }

    // set the socket so that it does not hold onto its port after
    // the process exits (needed because on at least some platforms we
    // use well-known ports when connecting sockets)
    soVal = 1;
    soRet = setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, 
                        (const char*)&soVal, sizeof(soVal) );
    if( soRet < 0 ) {
        err = XPlat::NetUtils::GetLastError();
        perror( "setsockopt()" );
        mrn_dbg( 1, mrn_printf(FLF, stderr, "setsockopt() failed: %s\n",
                                       XPlat::Error::GetErrorString( err ).c_str() ) );
        return -1;
    }

    memset( &local_addr, 0, sizeof( local_addr ) );
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl( INADDR_ANY );

    if( port != 0 ) {
        local_addr.sin_port = htons( port );
        if( bind( sock, (sockaddr*)&local_addr, sizeof( local_addr ) ) == -1 ) {
            err = XPlat::NetUtils::GetLastError();
            perror( "bind()" );
            mrn_dbg( 1, mrn_printf(FLF, stderr, "bind() to static port %d failed: %s\n",
                                   port, XPlat::Error::GetErrorString( err ).c_str() ) );
            return -1;
        }
    }
    else {
        // let the system assign a port, start from 1st dynamic port
        port = 49152;
        local_addr.sin_port = htons( port );

        while( bind( sock, (sockaddr*)&local_addr, sizeof( local_addr ) ) == -1 ) {
            err = XPlat::NetUtils::GetLastError();
            if( XPlat::Error::EAddrInUse( err ) ) {
                local_addr.sin_port = htons( ++port );
                continue;
            }
            else {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "bind() to dynamic port %d failed: %s\n",
                                       port, XPlat::Error::GetErrorString( err ).c_str() ) );
                return -1;
            }
        }
    }

    if( listen( sock, 64 ) == -1 ) {
        perror( "listen()" );
        mrn_dbg( 1, mrn_printf(FLF, stderr, "%s", "" ) );
        return -1;
    }

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

    // determine which port we were actually assigned to
    if( getPortFromSocket( sock, port_in ) != 0 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr,
            "failed to obtain port from socket\n" ) );
        XPlat::SocketUtils::Close( sock );
        return -1;
    }
    
    *sock_in = sock;
    mrn_dbg( 3, mrn_printf(FLF, stderr,
                           "Leaving bindPort(). Returning sock:%d, port:%d\n",
                           sock, *port_in ) );
    return 0;
}

int getSocketConnection( int bound_socket )
{
    int connected_socket;

    mrn_dbg( 3, mrn_printf(FLF, stderr, "In get_connection(sock:%d).\n",
                bound_socket ) );

    do{
        connected_socket = accept( bound_socket, NULL, NULL );
        if( connected_socket == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "%s", "" ) );
            perror( "accept()" );

            if ( errno != EINTR ) {
                return -1;
            }
        }
    } while ( ( connected_socket == -1 ) && ( errno == EINTR ) );

#if defined(TCP_NODELAY)
    // turn off Nagle algorithm for coalescing packets
    int optVal = 1;
    int ssoret = setsockopt( connected_socket,
                             IPPROTO_TCP,
                             TCP_NODELAY,
                             (const char*)&optVal,
                             sizeof( optVal ) );
    if( ssoret == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "failed to set TCP_NODELAY\n" ) );
    }
#endif // defined(TCP_NODELAY)

    mrn_dbg( 3, mrn_printf(FLF, stderr,
                "Leaving get_connection(). Returning sock:%d\n",
                connected_socket ) );
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

int mrn_printf( const char *file, int line, const char * func,
                FILE * ifp, const char *format, ... )
{
    static FILE * fp = NULL;
    extern const char *node_type;
    int retval;
    va_list arglist;

    struct timeval tv;
    while( gettimeofday( &tv, NULL ) == -1 ) {}

    _global_network->printf_mutex.Lock();

    int rank = getrank();

    if( (fp == NULL) && (rank != UnknownRank) ) {

        FILE * tmp_fp = NULL;
        std::string this_host;
        struct stat s;
        char host[256];
        char logdir[256];
        char logfile[512];
        logfile[0] = '\0';

        const char* home = getenv("HOME");

        XPlat::NetUtils::GetLocalHostName(this_host);
        strncpy( host, this_host.c_str(), 256 );
        host[255] = '\0';

        // find log directory
        const char* varval = getenv( "MRNET_DEBUG_LOG_DIRECTORY" );
        if( varval != NULL ) {
            if( (stat(varval, &s) == 0) && (S_IFDIR & s.st_mode) )
                snprintf( logdir, sizeof(logdir), "%s", varval );
        }
        else if( home != NULL ) {
            snprintf( logdir, sizeof(logdir), "%s/mrnet-log", home );
            if( ! ((stat(logdir, &s) == 0) && (S_IFDIR & s.st_mode)) )
                snprintf( logdir, sizeof(logdir), "/tmp" );
        }
        // set file name format
        snprintf( logfile, sizeof(logfile), "%s/%s_%s_%d",
                  logdir, node_type, host, rank );
        tmp_fp = fopen( logfile, "w" );
	if( tmp_fp != NULL )
            fp = tmp_fp;
    }

    FILE *f = fp;
    if( f == NULL ) 
        f = ifp;

    if( file ) {
        // get thread name
        const char *thread_name = NULL;

        tsd_t *tsd = ( tsd_t * )tsd_key.Get();
        if( tsd != NULL )
            thread_name = tsd->thread_name;
            
        fprintf( f, "%ld.%ld: %s(0x%lx): %d %s: %s(): %d: ",
                 tv.tv_sec-MRN_RELEASE_DATE_SECS, tv.tv_usec,
                 ( thread_name != NULL ) ? thread_name : "<unknown thread>",
                 XPlat::Thread::GetId(), XPlat::Process::GetProcessId(), 
                 XPlat::PathUtils::GetFilename( file ).c_str(), func, line );
    }

    va_start( arglist, format );
    retval = vfprintf( f, format, arglist );
    va_end( arglist );
    fflush( f );
    
    _global_network->printf_mutex.Unlock();
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
    while(gettimeofday(&_start_tv, NULL) == -1) {}
    //fprintf(stderr, "offset: %lf secs\n", offset/1000.0 );
    _start_d = tv2dbl( _start_tv ) + ( offset / 1000.0 );
    _start_tv = dbl2tv( _start_d );
}
    
void Timer::stop( void ){
    while(gettimeofday(&_stop_tv, NULL) == -1) {}
    //fprintf(stderr, "offset: %lf secs\n", offset/1000.0 );
    _stop_d = tv2dbl( _stop_tv ) + ( offset / 1000.0 );
    _stop_tv = dbl2tv( _stop_d );
}

void Timer::stop( double d ){
    _stop_d = d;
    _stop_tv = dbl2tv( _stop_d );
}
    
double Timer::get_latency_secs( ) {
    return _stop_d - _start_d;
}
    
double Timer::get_latency_msecs( ) {
    return 1000 * get_latency_secs();
}

double Timer::get_latency_usecs( ) {
    return 1000000 * get_latency_secs();
}

Timer::Timer( void ) {

#ifdef USE_NTPQ_TIMER_INIT
    if( !first_time ) {
        return;
    }
    first_time=false;

    char cmd[]="/usr/sbin/ntpq -c rv | grep offset | awk '{print $1}'";
    //char cmd[]="/usr/sbin/ntpq -c rv | grep offset | awk '{print $3}'";
    char line[512];
    int line_len = 512;
    
    FILE * cmd_out_fp = popen( cmd, "r" );
    if( cmd_out_fp == NULL ) {
        perror( cmd );
        return;
    }
    if( fgets(line, line_len, cmd_out_fp ) == NULL ) {
        perror( "fgets()");
        return;
    }
    if( sscanf( line, "offset=%lf,", &offset ) == 0 ) {
        perror( "sscanf()");
        offset=0;
        return;
    }
#endif

}

Rank getrank() {return myrank;}
void setrank( Rank ir ) {myrank=ir;}
}                               // namespace MRN
