/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__error_h)
#define _error_h 1

#include "mrnet_lightweight/Types.h"

typedef enum {
    ERR_INFO,
    ERR_WARN,
    ERR_ERR,
    ERR_CRIT
} ErrorLevel;

typedef enum {
    ERR_IGNORE=0,
    ERR_ALERT,
    ERR_RETRY,
    ERR_ABORT
} ErrorResponse;

typedef enum {
    ERR_NONE=0,
    ERR_TOPOLOGY_FORMAT,
    ERR_TOPOLOGY_CYCLE,
    ERR_TOPOLOGY_NOTCONNECTED,
    ERR_NETWORK_FAILURE,
    ERR_FORMATSTR,
    ERR_PACKING,
    ERR_INTERNAL,
    ERR_SYSTEM
} ErrorCode;

typedef struct {
    ErrorCode code;
    ErrorLevel level;
    ErrorResponse response;
    const char *msg;
} ErrorDef;


void error( ErrorCode, Rank _rank, const char*, ... );

#endif /* __error_h */
