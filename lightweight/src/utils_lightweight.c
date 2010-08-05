/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/


#include <sys/types.h>
#include <sys/stat.h>

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "config.h"

#include "mrnet_lightweight/Network.h"
#include "utils_lightweight.h"

#include "xplat_lightweight/NetUtils.h"
#include "xplat_lightweight/Error.h"
#include "xplat_lightweight/PathUtils.h"

#if !defined(os_windows)
#include <sys/time.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#else 
#define sleep(x) Sleep(1000*(DWORD)x)
#include <winsock2.h>
#endif // defined(os_windows)

#if defined(os_solaris)
#include <sys/sockio.h> // only for solaris
#endif //defined(os_solaris)

Rank myrank = (Rank)-1;

#ifdef os_windows
int gettimeofday(struct timeval *tv, struct timezone *tz)
{
    struct _timeb now;
    _ftime(&now);
    if (tv != NULL) {
        tv->tv_sec = now.time;
        tv->tv_usec = now.millitm * 1000;
    }
    return 0;
}
#endif


int connectHost ( int *sock_in, /*const*/ char* hostname, 
                  Port port, int num_retry)
{
    int err, sock = *sock_in;
    struct sockaddr_in server_addr;
    struct hostent *server_hostent = NULL;
    const char* host = hostname;
    unsigned int nConnectTries = 0;
    int cret = -1;
    int optVal;
    int ssoret;

    mrn_dbg(3, mrn_printf(FLF, stderr, "In connectHost(%s:%d) sock:%d..\n",
                          host, port, sock));

    if (sock <= 0) {
        sock = socket ( AF_INET, SOCK_STREAM, 0 );
        if ( sock == -1 ) {
            err = NetUtils_GetLastError();
            perror( "socket()" );
            mrn_dbg(1, mrn_printf(FLF, stderr, "socket() failed, errno=%d\n", err));
            return -1;
        }
        mrn_dbg(5, mrn_printf(FLF, stderr, "socket() => %d\n", sock));
    }

    server_hostent = gethostbyname( host ); 
    if ( server_hostent == NULL ) {
        perror ( "gethostbyname()" );
        return -1;
    } 

    memset( &server_addr, 0, sizeof( server_addr ));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons (port);
    memcpy (&server_addr.sin_addr, server_hostent->h_addr_list[0],
            sizeof (struct in_addr ));

    do {
        cret = connect (sock, (struct sockaddr *) & server_addr, sizeof (server_addr));
        if (cret == -1 ) {
            err = NetUtils_GetLastError(); 
            mrn_dbg(5, mrn_printf(FLF, stderr, "connectHost: connect() failed, err=%d\n", err));
            if (!(Error_ETimedOut(err) || Error_EConnRefused(err))) {
                mrn_dbg(1, mrn_printf(FLF, stderr, "connect() to %s:%d failed: %s\n", 
                                      host, port, Error_GetErrorString(err)));
                return -1;
            }

            nConnectTries++;
            mrn_dbg(3, mrn_printf(FLF, stderr, "connect() to %s:%d timed out %d times\n", 
                                  host, port, nConnectTries));
            if ((num_retry > 0 ) && (nConnectTries >= num_retry ))
                break;

            // delay before trying again
            sleep (nConnectTries);
        }
    } while ( cret == -1 );

    if (cret == -1) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "connect() to %s:%d failed: %s\n", 
                              host, port, Error_GetErrorString(err)));
        return -1;
    }

    mrn_dbg(5, mrn_printf(FLF, stderr, "connectHost: connected!\n"));

#if defined(TCP_NODELAY)
    // turn of Nagle algorithm for coalescing packets
    optVal = 1;
    ssoret = setsockopt (sock,
                         IPPROTO_TCP,
                         TCP_NODELAY,
                         (const char*)&optVal,
                         sizeof( optVal));
    if (ssoret == -1 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "failed to set TCP_NODELAY\n"));
    }

#endif // defined (TCP_NODELAY)

    mrn_dbg(3, mrn_printf(FLF, stderr, "Leaving connectHost(). Returning sock: %d\n", sock));

    *sock_in = sock;
    return 0;
}


int mrn_printf( const char *file, int line, const char * func,
                FILE * ifp, const char *format, ... )
{
    static FILE * fp = NULL;
    char *node_type = "be";
    int retval;
    va_list arglist;

    struct timeval tv;
    int rank = getrank();
    FILE * tmp_fp = NULL;
    char* this_host = (char*)malloc(sizeof(char)*256);
    assert(this_host);
    struct stat s;
    char host[256];
    char logdir[256];
    char logfile[512];
    const char* home = getenv("HOME");
    const char* varval = getenv( "MRNET_DEBUG_LOG_DIRECTORY" );
    FILE *f;
  
    while (gettimeofday( &tv, NULL ) == -1 ) {}

    if ( (fp == NULL) && (rank != UnknownRank) ) {
        logfile[0] = '\0';

        NetUtils_GetLocalHostName(this_host);
        strncpy(host, this_host, 256);
        host[255] = '\0';

        // find log directory
        if (varval != NULL) {
            if ( (stat(varval, &s) == 0) && (S_IFDIR & s.st_mode) )
                snprintf( logdir, sizeof(logdir), "%s", varval);
        }
        else if (home != NULL) {
            snprintf( logdir, sizeof(logdir), "%s/mrnet-log", home );
            if(! ((stat(logdir, &s) == 0) && (S_IFDIR & s.st_mode)))
                snprintf( logdir, sizeof(logdir), "/tmp" );
        }
        // set file name format
        snprintf(logfile, sizeof(logfile), "%s/%s_%s_%d",
                 logdir, node_type, host, rank );
        tmp_fp = fopen(logfile, "w");
        if (tmp_fp != NULL)
            fp = tmp_fp;
    }
  
    f = fp;
    if (f == NULL)
        f = ifp;

    if (file) {
    
        fprintf( f, "%ld.%ld: %s: %s(): %d: ",
                 tv.tv_sec-MRN_RELEASE_DATE_SECS, 
                 tv.tv_usec, 
                 GetFilename(file),
                 func, 
                 line);
    }

    va_start(arglist, format);
    retval = vfprintf(f, format, arglist);
    va_end(arglist);
    fflush(f);

    return retval;
}

Timer_t new_Timer_t()
{
    Timer_t time;
    time.offset = 0;
    time.first_time=true;

    return time;
}

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

void Timer_start(Timer_t time) 
{
    while (gettimeofday(&(time.start_tv), NULL) == -1) {}
    time.start_d = tv2dbl(time.start_tv) + (time.offset/1000.0);
    time.start_tv = dbl2tv(time.start_d);
}

void Timer_stop(Timer_t time)
{
    while (gettimeofday(&(time.stop_tv), NULL) == -1) {}
    time.stop_d = tv2dbl(time.stop_tv) + (time.offset/1000.0);
    time.stop_tv = dbl2tv(time.stop_d);
}

double Timer_get_latency_secs(Timer_t time) 
{
    return time.stop_d - time.start_d;
}   

double Timer_get_latency_msecs(Timer_t time)
{
    return 1000 * Timer_get_latency_secs(time);
}

Rank getrank() {return myrank;}

void setrank(Rank ir) {
    myrank=ir;
}

int isBigEndian() {
    unsigned int one = 1;
    unsigned char * arr = (unsigned char *)&one;

    if (!arr[0])
        return 1;
    else
        return 0;   

}

void endianTest() {
#if defined(WORDS_BIGENDIAN)
    mrn_dbg(5, mrn_printf(FLF, stderr, "config says BIG_ENDIAN\n"));
#else
    mrn_dbg(5, mrn_printf(FLF, stderr, "config says LITTLE_ENDIAN\n"));
#endif

    if (isBigEndian()) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "test returns big endian\n"));
    } else {
        mrn_dbg(5, mrn_printf(FLF, stderr, "test returns little endian\n"));
    }

}

