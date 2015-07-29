/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__timekeeper_h)
#define __timekeeper_h 1

#include <map>
#include <set>

#include "xplat/Mutex.h"

namespace MRN
{

class TimeKeeper {

 public:

    TimeKeeper(void) : 
        _min_timeout(default_timeout)
    {}

    /* get minimum timeout in milliseconds */
    int get_Timeout() const;

    void set_DefaultTimeout(int default_ms);

    /* update registered timeouts based on elapsed time,
       fills set of stream ids whose timers have expired */
    void notify_Elapsed( unsigned int elapsed_ms, std::set< unsigned int >& expired_streams );

    /* register/clear timeout for specified stream */
    bool register_Timeout( unsigned int strm_id, unsigned int timeout_ms );
    bool clear_Timeout( unsigned int strm_id );

 private:

    static int default_timeout;

    /* minimum timeout in ms */
    int _min_timeout;

    /* map< stream id, timeout in ms > */
    std::map< unsigned int, unsigned int > _strm_timeouts;
    mutable XPlat::Mutex _tk_mutex;
};


} // namespace MRN

#endif  /* __timekeeper_h */
