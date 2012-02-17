/****************************************************************************
 * Copyright © 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#ifndef XPLAT_TYPES_UNIX_H
#define XPLAT_TYPES_UNIX_H

#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#ifndef compiler_sun
#include <stdint.h>
#endif
#include <inttypes.h>

// Printf macro for size_t types
#define PRIszt  "zu"
#define PRIsszt "zd"

#endif // XPLAT_TYPES_UNIX_H
