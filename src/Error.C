/****************************************************************************
 *  Copyright 2003-2008 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <stdarg.h>

#include "mrnet/Error.h"

namespace MRN
{

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


void Error::error( ErrorCode e, Rank, const char * fmt, ... ) const
{
    static char buf[1024];

    MRN_errno = e;

    va_list arglist;
    va_start( arglist, fmt );
    vsnprintf( buf, 1024, fmt, arglist );
    va_end( arglist );

    ErrorResponse resp = errors[e].response;
    switch( resp ) {
    case ERR_ABORT:
        perror( buf );
        //TODO: really abort? exit(-1);
        break;
    case ERR_ALERT:
        perror( buf );
        //TODO: really report? Event::new_Event( ERROR_EVENT, e, r, string(buf) );
        break;
    default:
        break;
    }
}

} // namespace MRN
