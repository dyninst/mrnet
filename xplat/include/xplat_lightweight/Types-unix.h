/****************************************************************************
 * Copyright © 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#ifndef XPLAT_TYPES_UNIX_H
#define XPLAT_TYPES_UNIX_H

#include "xplat_config.h"

#if HAVE_NETDB_H
# include <netdb.h>
#endif

#if HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif

#if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif

#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif

// Printf macro for size_t types
#define PRIszt  "zu"
#define PRIsszt "zd"

#endif // XPLAT_TYPES_UNIX_H
