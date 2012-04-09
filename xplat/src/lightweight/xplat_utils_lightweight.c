/****************************************************************************
 * Copyright 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller   *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "xplat_lightweight/xplat_utils_lightweight.h"
#include "xplat_lightweight/PathUtils.h"

#ifndef os_windows
#include <sys/time.h>
#else
#include "xplat_lightweight/Types.h"
#endif

static FILE* xplat_printf_fp = NULL;

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
    FILE *f = xplat_printf_fp;
    char *base_file;
    while( gettimeofday( &tv, NULL ) == -1 ) {}

    // If xplat_printf was not initiated, send output to given file pointer
    if( f == NULL ) {
        f = ifp;
    }

    // print timestamp and thread info
    fprintf( f, "%ld.%06ld: ", 
             tv.tv_sec-XPLAT_RELEASE_DATE_SECS, tv.tv_usec);

    if( file ) {
        // print file, function, and line info
        base_file = GetFilename(file);
        fprintf( f, "(XPlat) %s[%d] %s - ", base_file, line, func );
#ifndef os_windows
        free(base_file);
#endif
    }

    // print message
    va_start( arglist, format );
    retval = vfprintf( f, format, arglist );
    va_end( arglist );

    fflush( f );

    return retval;
}
