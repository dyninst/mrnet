/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
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
#include <boost/shared_ptr.hpp>

using namespace XPlat;

static boost::shared_ptr<XPlat::Mutex> mrn_printf_mutex(new XPlat::Mutex());

namespace MRN
{

std::string LocalHostName = NULL_STRING;
std::string LocalDomainName = NULL_STRING;
std::string LocalNetworkName = NULL_STRING;
std::string LocalNetworkAddr = NULL_STRING;

Port LocalPort = 0;

void get_Version( int& major,
                  int& minor,
                  int& revision )
{
    major = MRNET_VERSION_MAJOR;
    minor = MRNET_VERSION_MINOR;
    revision = MRNET_VERSION_REV;
}

int connectHost( XPlat_Socket *sock_in, const std::string & hostname, 
                 XPlat_Port port, int num_retry /*=-1*/ )
{
    XPlat_Socket sock = *sock_in;

    unsigned nretry = 0;
    if( num_retry > 0 ) 
        nretry = (unsigned) num_retry;

    bool rc = XPlat::SocketUtils::Connect( hostname, port, 
                                           sock, nretry );
    if( rc ) {
        *sock_in = (int) sock;
        return 0;
    }
    else {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "failed to connect to %s:%hu\n",
                               hostname.c_str(), port) );
        return -1;
    }
}

int bindPort( XPlat_Socket *sock_in, XPlat_Port *port_in, 
              bool nonblock /*=false*/ )
{
    XPlat_Socket sock = *sock_in;
    XPlat_Port port = *port_in;
    bool rc = XPlat::SocketUtils::CreateListening( sock, 
                                                   port, 
                                                   nonblock );
    if( rc ) {
        *sock_in = sock;
        *port_in = port;
        return 0;
    }
    else {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "failed to bind port=%hu\n",
                               *port_in) );
        return -1;
    }

}

int getSocketConnection( XPlat_Socket bound_socket,
                         int timeout_sec /*=0*/ , bool nonblock /*=false*/ )
{
    XPlat_Socket connection = XPlat::SocketUtils::InvalidSocket;
    bool rc = XPlat::SocketUtils::AcceptConnection( bound_socket,
                                                    connection,
                                                    timeout_sec,
                                                    nonblock );
    if( rc ) {
        return connection;
    }
    else {
        mrn_dbg( 1, mrn_printf(FLF, stderr, 
                               "failed to get connection on socket=%d\n",
                               bound_socket) );
        return -1;
    }   
}


int getPortFromSocket( XPlat_Socket sock, XPlat_Port *port )
{
    XPlat_Port p;
    XPlat_Socket s = sock;
    bool rc = XPlat::SocketUtils::GetPort( s, p );
    if( rc ) {
        *port = p;
        return 0;
    }
    return -1;
}

extern char* MRN_DEBUG_LOG_DIRECTORY;

static FILE* mrn_printf_fp = NULL;

void mrn_printf_init( FILE* ifp )
{
    mrn_printf_fp = ifp;
    xplat_printf_init(mrn_printf_fp);
    if(XPlat::get_DebugLevel() <= CUR_OUTPUT_LEVEL) {
        XPlat::set_DebugLevel(CUR_OUTPUT_LEVEL);
    }
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
        xplat_printf_init(mrn_printf_fp);
        if(XPlat::get_DebugLevel() <= CUR_OUTPUT_LEVEL) {
            XPlat::set_DebugLevel(CUR_OUTPUT_LEVEL);
        }
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
    
    mrn_printf_mutex->Lock();

    // get thread info
    XPlat::Thread::Id tid = 0;
    const char* thread_name = NULL;
    Rank rank = UnknownRank;
    node_type_t node_type = UNKNOWN_NODE;
    //Network* net = NULL;
    
    if(XPlat_TLSKey != NULL) {
        tid = XPlat_TLSKey->GetTid();
        thread_name = XPlat_TLSKey->GetName();
        tsd_t *tsd = (tsd_t*) XPlat_TLSKey->GetUserData();
        if( tsd != NULL ) {
            rank = tsd->process_rank;
            node_type = tsd->node_type;
            //net = tsd->network;
        }
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
             XPlat::Thread::GetId() );

    if( file ) {
        // print file, function, and line info
        fprintf( f, "%s[%d] %s - ", 
                 XPlat::PathUtils::GetFilename(file).c_str(), line, func );
    }

    // print message
    va_start( arglist, format );
    retval = vfprintf( f, format, arglist );
    va_end( arglist );
    
    mrn_printf_mutex->Unlock();

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

Timer::Timer(void)
{
#ifdef USE_BOOST_TIMER
    _b_timer = NULL;
#else
    _stop_d = 0.0;
    _start_d = 0.0;
#endif
}

Timer::~Timer(void)
{
#ifdef USE_BOOST_TIMER
    if( NULL == _b_timer )
        delete _b_timer;
    _b_timer = NULL;
#else
    _stop_d = 0.0;
    _start_d = 0.0;
#endif
}

void Timer::start( void )
{
#ifdef USE_BOOST_TIMER
    if( NULL != _b_timer )
        delete _b_timer;
    _b_timer = new boost::timer::cpu_timer;
#else
    while(gettimeofday(&_start_tv, NULL) == -1) {}
    _start_d = tv2dbl( _start_tv );
#endif
}
    
void Timer::stop(void)
{
#ifdef USE_BOOST_TIMER
    if( NULL != _b_timer )
        _b_timer->stop();
#else 
    while(gettimeofday(&_stop_tv, NULL) == -1) {}
    _stop_d = tv2dbl( _stop_tv );
#endif
}

double Timer::get_latency_secs(void)
{
#ifdef USE_BOOST_TIMER
    // No need to check if timer is started, boost does that for us.
    double elapsed = 0.0;
    if( NULL != _b_timer )
        elapsed = (double) _b_timer->elapsed().wall / 1000000000.0 ;
    return elapsed;
#else
    if( _start_d > _stop_d )
        return 0.0;
    return _stop_d - _start_d;
#endif
}
    
double Timer::get_latency_msecs(void)
{
    return 1000 * get_latency_secs();
}

double Timer::get_latency_usecs(void)
{
    return 1000000 * get_latency_secs();
}

bool isBigEndian()
{
    unsigned int one = 1;
    unsigned char* arr = (unsigned char *)&one;

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

