/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if ! defined(__Error_h)
#define __Error_h

#include <list>
#include <cstdio>
#include "mrnet/Types.h"

namespace MRN
{

typedef enum{
    ERR_INFO=0,
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

typedef struct 
{
    ErrorCode code;
    ErrorLevel level;
    ErrorResponse response;
    const char *msg;
} ErrorDef;

extern ErrorDef errors[];

class Error{
 protected:
    mutable ErrorCode MRN_errno;

 public:
    Error(): MRN_errno(ERR_NONE) { }
    virtual ~Error() { }

    inline bool has_Error() const {
        return (MRN_errno != ERR_NONE);
    }

    inline void perror(const char *str) const {
        fprintf(stderr, "%s: %s\n", errors[MRN_errno].msg, str );
    }

    virtual void error( ErrorCode, Rank, const char *, ... ) const;
};

} // namespace MRN
#endif /* __Error_h */
