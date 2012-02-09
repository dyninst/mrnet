/****************************************************************************
 *  Copyright 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#ifndef utils_h
#define utils_h 1

#if !defined (__STDC_LIMIT_MACROS)
#  define __STDC_LIMIT_MACROS
#endif
#if !defined (__STDC_CONSTANT_MACROS)
#  define __STDC_CONSTANT_MACROS
#endif
#if !defined (__STDC_FORMAT_MACROS)
#  define __STDC_FORMAT_MACROS
#endif

#ifndef UNUSED
#if defined(__GNUC__)
#   define UNUSED(x) x __attribute__((unused)) /* UNUSED: x */ 
#else
#   define UNUSED(x) x
#endif
#endif
 
#include "mrnet/Types.h"
#include "xplat/Types.h"
#include "xplat/TLSKey.h"
#include "xplat/Thread.h"

#include <cassert>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef os_windows // unix

# include "config.h"

# if HAVE_FCNTL_H
#  include <fcntl.h>
# endif
# if HAVE_SIGNAL_H
#  include <signal.h>
# endif
# if HAVE_UNISTD_H
#  include <unistd.h>
# endif
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# endif

#else // windows

# include <fcntl.h>
# include <io.h>
# include <sys/timeb.h>

# define srand48(x) srand((unsigned int)x)
# define drand48 (double)rand
# define srandom(x) srand(x)
# define random rand
# define snprintf _snprintf
# define sleep(x) Sleep(1000*(DWORD)x)
# define EWOULDBLOCK WSAEWOULDBLOCK
# define strdup _strdup

inline int gettimeofday(struct timeval *tv, struct timezone *tz)
{
    int ret_val;
    struct __timeb64 now;
    ret_val = _ftime64_s(&now);
    if(ret_val) {
        return ret_val;
    }
    if (tv != NULL) {
        tv->tv_sec = (long)now.time;
        tv->tv_usec = now.millitm * 1000;
    }
    return 0;
}

#endif // ifndef os_windows

#include <vector>
#include <string>

#define MRN_RELEASE_DATE_SECS 1308200400

#define NULL_STRING ""

namespace MRN
{

int connectHost( int *sock_in, const std::string & hostname, Port port,
                 int num_retry = -1 );

int bindPort( int *sock_in, Port *port_in, bool nonblock=false );
int getSocketConnection( int bound_socket, int& inout_errno,
                         int timeout_sec=0, bool nonblock=false );
int getPortFromSocket( int sock, Port *port );

struct ltstr
{
    bool operator(  ) ( std::string s1, std::string s2 ) const
    {
        return ( s1 < s2 );
    }
};

extern XPlat::TLSKey tsd_key;

class Network;

class tsd_t {
 public:
    XPlat::Thread::Id thread_id;
    const char *thread_name;
    Rank process_rank;
    node_type_t node_type;
    Network* network;
};

#if defined(DEBUG_MRNET)
#  define _fprintf(X) fprintf X ;
#  define _perror(X) perror(X);
#else
#  define _fprintf(X)  ;
#  define _perror(X) ;
#endif                          // defined(DEBUG_MRNET)

extern int CUR_OUTPUT_LEVEL; 
#define mrn_dbg( x, y ) \
do{ \
    if( MRN::CUR_OUTPUT_LEVEL >= x ){           \
        y;                                      \
    }                                           \
}while(0)

//FLF is used to call mrn_printf(FLF, ...)
#if !defined( __GNUC__ )
#define FLF  __FILE__,__LINE__,"unknown"
#else
#define FLF  __FILE__,__LINE__,__FUNCTION__
#endif

#define mrn_dbg_func_end()                    \
do { \
    mrn_dbg(3, MRN::mrn_printf(FLF, stderr, "Function exit\n"));    \
} while(0)

#define mrn_dbg_func_begin()                    \
do { \
    mrn_dbg(3, MRN::mrn_printf(FLF, stderr, "Function start ...\n")); \
} while(0)


/* struct timeval/double conversion */
double tv2dbl( struct timeval tv);
struct timeval dbl2tv(double d) ;

class Timer{
 public:
    struct timeval _start_tv, _stop_tv;
    double  _start_d, _stop_d;
    
    Timer( void );
    void start( void );
    void stop( void );
    void stop( double d );
    double get_timer ( void );
    double get_latency_secs( void );
    double get_latency_msecs( void );
    double get_latency_usecs( void );
    double get_offset_msecs( void );

 private:
    static double offset;
    static bool first_time;
};

bool isBigEndian();
void endianTest();

}// namespace MRN



#endif                          /* utils_h */
