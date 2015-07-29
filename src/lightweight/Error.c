/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdarg.h>

#include "mrnet_lightweight/Error.h"
#include "utils_lightweight.h"

ErrorDef errors[] = {
    { ERR_NONE, ERR_INFO, ERR_IGNORE, "No Error"},
    { ERR_TOPOLOGY_FORMAT, ERR_CRIT, ERR_ABORT, "Topology error: file format"},
    { ERR_TOPOLOGY_CYCLE, ERR_CRIT, ERR_ABORT, "Topology error: cycle exists"},
    { ERR_TOPOLOGY_NOTCONNECTED, ERR_CRIT, ERR_ABORT, "Topology error: not fully connected"},
    { ERR_NETWORK_FAILURE, ERR_CRIT, ERR_ABORT, "Network failure"},
    { ERR_FORMATSTR, ERR_ERR, ERR_ALERT, "Format string mismatch"},
    { ERR_PACKING, ERR_CRIT, ERR_ABORT, "Packet encoding/decoding failure"},
    { ERR_INTERNAL, ERR_CRIT, ERR_ABORT, "Internal failure"},
    { ERR_SYSTEM, ERR_ERR, ERR_ABORT, "System/library call failure"}
};

void error(ErrorCode e, Rank UNUSED(r), const char* fmt, ... )
{
    static char buf[1024];

#ifndef os_windows
    ErrorResponse resp = errors[e].response;
#else
    int resp = errors[e].response;
#endif

    va_list arglist;
    va_start(arglist, fmt);
    vsnprintf(buf, (size_t)1024, fmt, arglist);
    va_end(arglist);

    switch(resp) {
    case ERR_ABORT:
        perror(buf);
        break;
    case ERR_ALERT:
        perror(buf);
        break;
    default:
        break;
    }
}
