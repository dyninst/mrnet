/****************************************************************************
 * Copyright © 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__ncio_h)
#define __ncio_h 1

#include "xplat_lightweight/Types.h"

#if defined(WIN32)
#include <winsock2.h>
typedef SOCKET XPSOCKET;
#else
typedef int XPSOCKET;
#include <sys/uio.h>
#include <sys/socket.h>
#endif // defined(WIN32)

typedef struct
{
  size_t len;
  char* buf;
} NCBuf_t;

NCBuf_t* new_NCBuf_t();

ssize_t XPlat_NCSend(XPSOCKET s, NCBuf_t* bufs, unsigned int nBufs);
ssize_t XPlat_NCRecv(XPSOCKET s, NCBuf_t* bufs, unsigned int nBufs);

ssize_t XPlat_NCsend(XPSOCKET s, const void *buf, size_t count);
ssize_t XPlat_NCrecv(XPSOCKET s, void *buf, size_t count);

#endif /* __ncio_h */
