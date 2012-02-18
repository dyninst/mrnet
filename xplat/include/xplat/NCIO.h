/****************************************************************************
 * Copyright © 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// NCIO : Network Communication I/O
#ifndef XPLAT_NCIO_H
#define XPLAT_NCIO_H

#include "xplat/Types.h"

namespace XPlat
{

#ifdef WIN32
typedef SOCKET XPSOCKET;
#else
typedef int XPSOCKET;
#endif // WIN32

struct NCBuf
{
    size_t len;
    char* buf;
};

ssize_t NCSend( XPSOCKET s, NCBuf* bufs, unsigned int nBufs );
ssize_t NCRecv( XPSOCKET s, NCBuf* bufs, unsigned int nBufs );

ssize_t NCsend( XPSOCKET s, const void *buf, size_t count );
ssize_t NCrecv( XPSOCKET s, void *buf, size_t count );

// flag to specify with send/recv for blocking recv, if available.
extern const int NCBlockingRecvFlag;

} // namespace XPlat

#endif // XPLAT_NCIO_H
