/****************************************************************************
 *  Copyright 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#ifndef XPLAT_UTILS_H
#define XPLAT_UTILS_H 1

#include <cstdio>
#include <cstdarg>
#include "xplat/Thread.h"
#include "xplat/TLSKey.h"

namespace XPlat {

#define XPLAT_RELEASE_DATE_SECS 1308200400

extern int CUR_DEBUG_LEVEL;

//FLF is used to call xplat_printf(FLF, ...)
#if !defined( __GNUC__ )
#define FLF  __FILE__,__LINE__," "
#else
#define FLF  __FILE__,__LINE__,__FUNCTION__
#endif

#define xplat_dbg(x,y) \
do{ \
  if(XPlat::CUR_DEBUG_LEVEL >= x) y; \
}while(0)


void set_DebugLevel(int l);
int get_DebugLevel(void);
void xplat_printf_init( FILE* ifp );
int xplat_printf( const char *file, int line, const char * func,
                  FILE * ifp, const char *format, ... );

#define xplat_dbg_func_end() \
do { \
    xplat_dbg(3, XPlat::xplat_printf(FLF, stderr, "Function exit\n")); \
} while(0)

#define xplat_dbg_func_begin() \
do { \
    xplat_dbg(3, XPlat::xplat_printf(FLF, stderr, "Function start ...\n")); \
} while(0)

} // namespace XPlat

#endif /* XPLAT_UTILS_H */
