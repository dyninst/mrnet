/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "utils.h"
#include "mrnet/Error.h"

namespace MRN
{

ErrorDef errors[] = {
    { ERR_NONE, ERR_INFO, ERR_IGNORE, "MRNet: No Error"},
    { ERR_TOPOLOGY_FORMAT, ERR_CRIT, ERR_ABORT, "MRNet: Topology error: file has incorrect format"},
    { ERR_TOPOLOGY_CYCLE, ERR_CRIT, ERR_ABORT, "MRNet: Topology error: cycle exists"},
    { ERR_TOPOLOGY_NOTCONNECTED, ERR_CRIT, ERR_ABORT, "MRNet: Topology error: not fully connected"},
    { ERR_NETWORK_FAILURE, ERR_CRIT, ERR_ABORT, "MRNet: Network failure"},
    { ERR_FORMATSTR, ERR_ERR, ERR_ALERT, "MRNet: Format string mismatch"},
    { ERR_PACKING, ERR_CRIT, ERR_ABORT, "MRNet: Packet encoding/decoding failure"},
    { ERR_INTERNAL, ERR_CRIT, ERR_ABORT, "MRNet: Internal failure"},
    { ERR_SYSTEM, ERR_ERR, ERR_ABORT, "MRNet: System/library call failure"},
    { ERR_CODE_LAST, ERR_LEVEL_LAST, ERR_RESPONSE_LAST, "MRNet: Bad error code"}
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

void Error::perror(const char *str) const
{
    mrn_dbg(1, mrn_printf(FLF, stderr, "%s: %s\n", errors[MRN_errno].msg, str) );
}

} // namespace MRN
