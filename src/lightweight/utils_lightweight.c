/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/


#include "utils_lightweight.h"
#include "mrnet_lightweight/Network.h"
#include "xplat_lightweight/Error.h"
#include "xplat_lightweight/NetUtils.h"
#include "xplat_lightweight/PathUtils.h"
#include "xplat_lightweight/Process.h"
#include "xplat_lightweight/SocketUtils.h"

#include <stdarg.h>

Rank myrank = (Rank)-1;

void get_Version( int* major,
                  int* minor,
                  int* revision )
{
    if( major != NULL )
        *major = MRNET_VERSION_MAJOR;
    if( minor != NULL )
        *minor = MRNET_VERSION_MINOR;
    if( revision != NULL )
        *revision = MRNET_VERSION_REV;
}

int connectHost( XPlat_Socket *sock_in, char* hostname, 
                 XPlat_Port port, int num_retry )
{
    XPlat_Socket sock = *sock_in;
    unsigned nretry = 0;
    bool_t rc;

    if( num_retry > 0 ) 
        nretry = (unsigned) num_retry;

    rc = XPlat_SocketUtils_Connect( hostname, port, &sock, nretry );
    if( rc ) {
        *sock_in = sock;
        return 0;
    }
    else {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "failed to connect to %s:%hu\n",
                               hostname, port) );
        return -1;
    }
}

/*************** Debug Utilities ***************/

extern char* MRN_DEBUG_LOG_DIRECTORY;

int mrn_printf( const char *file, int line, const char * func,
                FILE * ifp, const char *format, ... )
{
    static FILE* fp = NULL;
    char* base_file;
    const char* node_type = "BE";
    int retval;
    va_list arglist;
    struct stat s;

    struct timeval tv;
    Rank rank = getrank();
    char host[XPLAT_MAX_HOSTNAME_LEN];
    char logdir[256];
    char logfile[512];
    
    const char* varval = MRN_DEBUG_LOG_DIRECTORY;

    FILE *f;
    static int retry = true;
    int pid = Process_GetProcessId();

    while (gettimeofday( &tv, NULL ) == -1 ) {}
    
    if( (MRN_DEBUG_LOG_DIRECTORY != NULL) && (retry) ) {

        if ( (fp == NULL) && (rank != UnknownRank) ) {
            logfile[0] = '\0';

            XPlat_NetUtils_GetLocalHostName(host);
            host[XPLAT_MAX_HOSTNAME_LEN-1] = '\0';

            // find log directory
            if (varval != NULL) {
                if ( (stat(varval, &s) == 0) && (S_IFDIR & s.st_mode) ) {
                    snprintf( logdir, sizeof(logdir), "%s", varval);
            
                    // set file name format
                    snprintf(logfile, sizeof(logfile), "%s/%s_%s_%d.%d",
                            logdir, node_type, host, rank, pid);
                    fp = fopen(logfile, "w");
                    xplat_printf_init(fp);
                    if(get_DebugLevel() <= CUR_OUTPUT_LEVEL) {
                        set_DebugLevel(CUR_OUTPUT_LEVEL);
                    }
                } else {
                    retry = false; 
                }
            }    
        }
    }

    f = fp;
    if (f == NULL)
        f = ifp;

    if (file) {
        base_file = GetFilename(file);
        fprintf( f, "%ld.%06ld: %s[%d] %s - ",
                 tv.tv_sec-MRN_RELEASE_DATE_SECS, 
                 tv.tv_usec, 
                 base_file,
                 line,
                 func );
#ifndef os_windows
        free(base_file);
#endif
    }

    va_start(arglist, format);
    retval = vfprintf(f, format, arglist);
    va_end(arglist);
    fflush(f);
   
    return retval;
}

/*************** Timing Utilities ***************/

double tv2dbl(struct timeval tv)
{
    return ((double)tv.tv_sec + (double)tv.tv_usec / 1000000.0);
}

struct timeval dbl2tv(double d)
{
    struct timeval tv;
    tv.tv_sec = (long) d;
    tv.tv_usec = (long) ((d - (long) d) * 1000000.0);
    return tv;
}

Timer_t* new_Timer_t(void)
{
    Timer_t* t = (Timer_t*) calloc(1, sizeof(Timer_t));
    return t;
}

void Timer_start(Timer_t* t) 
{
    if( NULL != t ) {
        while (gettimeofday(&(t->start_tv), NULL) == -1) {}
        t->start_d = tv2dbl(t->start_tv);
        t->start_tv = dbl2tv(t->start_d);
    }
}

void Timer_stop(Timer_t* t)
{
    while (gettimeofday(&(t->stop_tv), NULL) == -1) {}
    t->stop_d = tv2dbl(t->stop_tv);
    t->stop_tv = dbl2tv(t->stop_d);
}

double Timer_get_latency_secs(Timer_t* t) 
{
    if( NULL != t ) {
        if( t->stop_d > t->start_d )
            return t->stop_d - t->start_d;
    }
    return 0.0;
}   

double Timer_get_latency_msecs(Timer_t* t)
{
    return 1000 * Timer_get_latency_secs(t);
}

double Timer_get_latency_usecs(Timer_t* t)
{
    return 1000 * Timer_get_latency_msecs(t);
}

/*************** MISC ***************/

Rank getrank(void) { return myrank; }
void setrank(Rank ir) { myrank = ir; }

int isBigEndian(void) {
    unsigned int one = 1;
    unsigned char * arr = (unsigned char *)&one;

    if (!arr[0])
        return 1;
    else
        return 0;   

}

/* void endianTest() { */
/* #if defined(WORDS_BIGENDIAN) */
/*     mrn_dbg(5, mrn_printf(FLF, stderr, "config says BIG_ENDIAN\n")); */
/* #else */
/*     mrn_dbg(5, mrn_printf(FLF, stderr, "config says LITTLE_ENDIAN\n")); */
/* #endif */

/*     if (isBigEndian()) { */
/*         mrn_dbg(5, mrn_printf(FLF, stderr, "test returns big endian\n")); */
/*     } else { */
/*         mrn_dbg(5, mrn_printf(FLF, stderr, "test returns little endian\n")); */
/*     } */

/* } */

