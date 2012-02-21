/****************************************************************************
 *  Copyright 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#ifndef __utils_lightweight_h
#define __utils_lightweight_h 1

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#ifndef UNUSED
# if defined(__GNUC__)
#   define UNUSED(x) UNUSED_ ## x __attribute__((unused))
# else
#   define UNUSED(x) x
# endif
#endif
 
#include "mrnet_lightweight/Types.h"

#define MRN_RELEASE_DATE_SECS 1308200400

#define NULL_STRING ""

/*************** Socket Utilities ***************/
int connectHost(XPlat_Socket *sock_in, char* hostname, 
                XPlat_Port port, int num_retry);

/*************** Debug Utilities ***************/

#if defined(DEBUG_MRNET)
# define _fprintf(X) fprintf X;
# define _perror(X) perror(X);
#else
# define _fprintf(X) ;
# define _perror(X) ;
#endif

extern int CUR_OUTPUT_LEVEL;
#define mrn_dbg( x, y) \
do{ \
    if (CUR_OUTPUT_LEVEL >= x) {      \
        y;                            \
    }                                 \
}while(0);


// FLF is used to call mrn_printf(FLF, ...)
#if !defined( __GNUC__)
#define FLF __FILE__,__LINE__,"unknown"
#else
#define FLF __FILE__,__LINE__,__FUNCTION__
#endif

#define mrn_dbg_func_end()                                    \
do { \
    mrn_dbg(3, mrn_printf(FLF, stderr, "Function exit\n"));   \
}while(0);

#define mrn_dbg_func_begin()                                  \
do { \
    mrn_dbg(3, mrn_printf(FLF, stderr, "Function start ...\n")); \
}while(0);


/*************** Timing Utilities ***************/
typedef struct {  
    struct timeval start_tv;
    struct timeval stop_tv;
    double start_d;
    double stop_d;
} Timer_t;

Timer_t new_Timer_t();

double tv2dbl(struct timeval tv);
struct timeval dbl2tv(double d);

void Timer_start(Timer_t time); 
void Timer_stop(Timer_t time);

double Timer_get_latency_secs(Timer_t time);
double Timer_get_latency_msecs(Timer_t time);

/*************** MISC ***************/
Rank getrank();
void setrank(Rank ir);

int isBigEndian();

#endif /* __utils_lightweight_h */

