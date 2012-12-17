/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#ifndef __xplat_types_unix_h
#define __xplat_types_unix_h

#include "xplat_config.h"

// first, get the standard C headers

#if HAVE_LIMITS_H
# include <limits.h>
#endif

#if HAVE_STDDEF_H
# include <stddef.h>
#endif

#if HAVE_STDIO_H
# include <stdio.h>
#endif

#if HAVE_STDLIB_H
# include <stdlib.h>
#endif

#if HAVE_STRING_H
# include <string.h>
#endif

#if HAVE_ASSERT_H
# include <assert.h>
#endif

#if HAVE_ERRNO_H
# include <errno.h>
#endif


// next, get the available POSIX/UNIX headers 

#if HAVE_FCNTL_H
# include <fcntl.h>
#endif

#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif

#if HAVE_NETDB_H
# include <netdb.h>
#endif

#if HAVE_STDBOOL_H
# include <stdbool.h>
#endif

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#if HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif

#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif

#if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif

#if HAVE_SYS_SOCKIO_H
# include <sys/sockio.h>
#endif

#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

// Printf macro for size_t types
#ifndef PRIszt
# ifdef os_solaris
#  define PRIszt  "lu"
#  define PRIsszt "ld"
# else
#  define PRIszt  "zu"
#  define PRIsszt "zd"
# endif
#endif

// socket types
typedef int XPlat_Socket;
typedef uint16_t XPlat_Port;

// some limits we use
#ifndef XPLAT_MAX_HOSTNAME_LEN
# ifdef _POSIX_HOST_NAME_MAX
#  define XPLAT_MAX_HOSTNAME_LEN (_POSIX_HOST_NAME_MAX+1)
# else
#  define XPLAT_MAX_HOSTNAME_LEN (256)
# endif
#endif

#ifndef XPLAT_PATH_MAX_LEN
# ifdef _POSIX_PATH_MAX
#  define XPLAT_PATH_MAX_LEN (_POSIX_PATH_MAX+1)
# else
#  define XPLAT_PATH_MAX_LEN (256)
# endif
#endif

// Needed for TCP_NODELAY on unix
#include <netinet/tcp.h>

#endif // __xplat_types_unix_h
