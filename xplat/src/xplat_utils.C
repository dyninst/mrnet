/****************************************************************************
 * Copyright 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller   *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "xplat/xplat_utils.h"
#include "xplat/PathUtils.h"
#include "xplat/Mutex.h"

#ifndef os_windows
#include <sys/time.h>
#else
#include "xplat/Types.h"
#endif

namespace XPlat
{

static FILE* xplat_printf_fp = NULL;

static XPlat::Mutex xplat_printf_mutex;

const int MIN_DEBUG_LEVEL = 0;
const int MAX_DEBUG_LEVEL = 5;
int CUR_DEBUG_LEVEL = 1;

void set_DebugLevel(int l) {
    if( l <= MIN_DEBUG_LEVEL ) {
        CUR_DEBUG_LEVEL = MIN_DEBUG_LEVEL;
    }
    else if( l >= MAX_DEBUG_LEVEL ) {
        CUR_DEBUG_LEVEL = MAX_DEBUG_LEVEL;
    }
    else {
        CUR_DEBUG_LEVEL = l;
    }
}

int get_DebugLevel(void) {
    return CUR_DEBUG_LEVEL;
}

void xplat_printf_init( FILE* ifp ) {
    xplat_printf_fp = ifp;
}

int xplat_printf( const char *file, int line, const char * func,
                  FILE * ifp, const char *format, ... ) {
    int retval = 1;
    va_list arglist;

    struct timeval tv;
    const char *thread_name = NULL;
    while( gettimeofday( &tv, NULL ) == -1 ) {}

    xplat_printf_mutex.Lock();

    // if we weren't set up, send to stderr (or whatever was specified)
    FILE *f = xplat_printf_fp;
    if( f == NULL ) {
        f = ifp;
    }

    if(XPlat_TLSKey != NULL) {
        thread_name = XPlat_TLSKey->GetName();
    }

    // print timestamp and thread info
    fprintf( f, "%ld.%06ld: %s(0x%lx): ", 
             tv.tv_sec-XPLAT_RELEASE_DATE_SECS, tv.tv_usec,
             ( thread_name != NULL ) ? thread_name : "UNKNOWN_THREAD",
             Thread::GetId());

    if( file ) {
        // print file, function, and line info
        fprintf( f, "(XPlat) %s[%d] %s - ", 
                 PathUtils::GetFilename(file).c_str(), line, func );
    }

    // print message
    va_start( arglist, format );
    retval = vfprintf( f, format, arglist );
    va_end( arglist );

    fflush( f );

    xplat_printf_mutex.Unlock();

    return retval;
}

} // namespace XPlat
