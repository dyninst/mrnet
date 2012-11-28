/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#ifndef XPLAT_TYPES_UNIX_H
#define XPLAT_TYPES_UNIX_H

// first, get the standard C headers
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cerrno>

// next, get the available POSIX/UNIX headers 

#include "xplat_config.h"

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

#endif // XPLAT_TYPES_UNIX_H
