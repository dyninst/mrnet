/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#ifndef MRNET_UTILS_H
#define MRNET_UTILS_H 1

#ifndef os_windows
# include "mrnet_config.h"
#else
#define COMMNODE_EXE "mrnet_commnode"
#endif

#if !defined (__STDC_LIMIT_MACROS)
#  define __STDC_LIMIT_MACROS
#endif
#if !defined (__STDC_CONSTANT_MACROS)
#  define __STDC_CONSTANT_MACROS
#endif
#if !defined (__STDC_FORMAT_MACROS)
#  define __STDC_FORMAT_MACROS
#endif

#define GCC_VERSION_INFO (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)


#ifndef UNUSED
# if GCC_VERSION_INFO > 30500 // If GCC Version > 3.5 
#  define UNUSED(x) x __attribute__((unused))
# else
#  define UNUSED(x) x
# endif
#endif
 
#ifdef os_solaris
# ifndef _REENTRANT
#  define _REENTRANT // needed to get strtok_r
# endif
# include <sys/int_limits.h> // integer types MIN/MAX
#endif

#include <cctype>
#include <cmath>
#include <cstdarg>
#include <vector>
#include <string>

#include "xplat/TLSKey.h"
#include "xplat/Thread.h"
#include "xplat/xplat_utils.h"
#include "mrnet/Types.h"


#ifdef USE_BOOST_TIMER
# include <boost/timer/timer.hpp>
#endif // USE_BOOST_TIMER

#define MRN_RELEASE_DATE_SECS 1308200400

#define NULL_STRING ""

namespace MRN
{

struct ltstr
{
    bool operator() ( std::string s1, std::string s2 ) const
    {
        return ( s1 < s2 );
    }
};

/*************** Socket Utilities ***************/
int connectHost( XPlat_Socket *sock_in, const std::string & hostname, 
                 XPlat_Port port, int num_retry = -1 );
int bindPort( XPlat_Socket *sock_in, XPlat_Port *port_in, 
              bool nonblock=false );
int getSocketConnection( XPlat_Socket bound_socket, 
                         int timeout_sec=0, bool nonblock=false );
int getPortFromSocket( XPlat_Socket sock, XPlat_Port *port );

/*************** Thread-level Storage ***************/
class Network;

class tsd_t {
 public:
    Rank process_rank;
    node_type_t node_type;
    Network* network;
};


/*************** Debug Utilities ***************/

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
#define FLF  __FILE__,__LINE__," "
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


/*************** Timing Utilities ***************/
double tv2dbl( struct timeval tv);
struct timeval dbl2tv(double d) ;

class Timer{
 public:
    struct timeval _start_tv, _stop_tv;
    double  _start_d, _stop_d;
    Timer(void);
    ~Timer(void);
    void start(void);
    void stop(void);
    double get_latency_secs(void);
    double get_latency_msecs(void);
    double get_latency_usecs(void);

 private:
#ifdef USE_BOOST_TIMER
    boost::timer::cpu_timer * _b_timer;
#endif 
};

/*************** MISC ***************/
bool isBigEndian();

} // namespace MRN

#endif /* MRNET_UTILS_H */
