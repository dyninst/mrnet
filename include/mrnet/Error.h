/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
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
    ERR_CRIT,
    ERR_LEVEL_LAST
} ErrorLevel;

typedef enum {
    ERR_IGNORE=0,
    ERR_ALERT,
    ERR_RETRY,
    ERR_ABORT,
    ERR_RESPONSE_LAST
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
    ERR_SYSTEM,
    ERR_CODE_LAST
} ErrorCode;

typedef struct 
{
    ErrorCode code;
    ErrorLevel level;
    ErrorResponse response;
    const char *msg;
} ErrorDef;

extern ErrorDef errors[];

class Error {

 protected:
    mutable ErrorCode MRN_errno;

 public:

    // BEGIN MRNET API

    inline bool has_Error() const 
    {
        return (MRN_errno != ERR_NONE);
    }

    inline ErrorCode get_Error() const
    {
        return MRN_errno;
    }

    inline const char* get_ErrorStr( ErrorCode err ) const
    {
        if( err < ERR_CODE_LAST )
            return errors[err].msg;
        else
            return errors[ERR_CODE_LAST].msg;
    }

    void perror(const char *str) const;

    // END MRNET API

    Error(): MRN_errno(ERR_NONE) { }
    virtual ~Error() { }

    virtual void error( ErrorCode, Rank, const char *, ... ) const;
};

} // namespace MRN
#endif /* __Error_h */
