/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#ifndef XPLAT_TYPES_WIN_H
#define XPLAT_TYPES_WIN_H

// first, get the standard C headers
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cassert>

// next, the windows headers
#include <fcntl.h>
#include <io.h>
#include <sys/timeb.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sys/types.h>
#include <sys/stat.h>

// Microsoft's compiler does not provide typedefs for specific-sized integers
// in <stdint.h>, or any other header.
typedef __int8 int8_t;
typedef __int16 int16_t;
typedef __int32 int32_t;
typedef __int64 int64_t;

typedef unsigned __int8 uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;

#if !defined(INT8_MAX)
#  define INT8_MAX _I8_MAX
#  define UINT8_MAX _UI8_MAX
#endif // !defined(INT8_MAX)

#if !defined(INT16_MAX)
#  define INT16_MAX _I16_MAX
#  define UINT16_MAX _UI16_MAX
#endif // !defined(INT16_MAX)

#if !defined(INT32_MAX)
#  define INT32_MAX _I32_MAX
#  define UINT32_MAX _UI32_MAX
#endif // !defined(INT32_MAX)

#if !defined(INT64_MAX)
#  define INT64_MAX _I64_MAX
#  define UINT64_MAX _UI64_MAX
#endif // !defined(INT64_MAX)

// ssize_t
#if !defined(ssize_t)
# ifdef SSIZE_T
#  define ssize_t SSIZE_T
# else
#  define ssize_t int64_t
# endif // SSIZE_T
#endif // !defined(ssize_t)

// Printf macros for size_t and bit-width types
#define PRIszt  "Iu"
#define PRIsszt "Id"
#define PRId32  "I32d"
#define PRIu32  "I32u"
#define PRId64  "I64d"
#define PRIu64  "I64u"
#define PRIi64  PRId64

// "address" type
typedef char* caddr_t;

// length of socket address struct
typedef int socklen_t;

// IPv4 address type
typedef uint32_t in_addr_t;

// socket type
typedef SOCKET XPlat_Socket;
typedef uint16_t XPlat_Port;

// error defs
#ifndef EADDRINUSE
# define EADDRINUSE WSAEADDRINUSE
#endif

#ifndef EBADF 
# define EBADF WSAEBADF
#endif

#ifndef ECONNREFUSED
# define ECONNREFUSED WSAECONNREFUSED
#endif

#ifndef EINPROGRESS
# define EINPROGRESS WSAEINPROGRESS
#endif

#ifndef EINTR
# define EINTR WSAEINTR
#endif

#ifndef EINVAL
# define EINVAL WSAEINVAL
#endif

#ifndef EFAULT
# define EFAULT WSAEFAULT
#endif

#ifndef ENOTSOCK
# define ENOTSOCK WSAENOTSOCK
#endif

#ifndef ETIMEDOUT
# define ETIMEDOUT WSAETIMEDOUT
#endif

#ifndef EWOULDBLOCK 
# define EWOULDBLOCK WSAEWOULDBLOCK
#endif


// some limits we use
#ifndef XPLAT_MAX_HOSTNAME_LEN
# define XPLAT_MAX_HOSTNAME_LEN (256)
#endif

#ifndef XPLAT_PATH_MAX_LEN
# define XPLAT_PATH_MAX_LEN (256)
#endif

// sleep
#define sleep(x) Sleep(1000*(DWORD)x)

// gettimeofday
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

// random numbers
#define srand48(x) srand((unsigned int)x)
#define drand48() (double)rand()

// misc functions
#ifndef snprintf 
# define snprintf _snprintf
#endif

#ifndef strdup 
# define strdup _strdup
#endif

#ifndef strtok_r 
# define strtok_r strtok_s
#endif


#endif // XPLAT_TYPES_WIN_H
