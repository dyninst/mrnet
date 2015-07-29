/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "TimeKeeper.h"


namespace MRN
{

int TimeKeeper::default_timeout = 100; /* 100 ms */

/* get minimum timeout in milliseconds */
int TimeKeeper::get_Timeout(void) const
{
    return _min_timeout;
}

void TimeKeeper::set_DefaultTimeout(int default_ms) {
    TimeKeeper::default_timeout = default_ms;
}

/* update registered timeouts based on elapsed time,
   fills set of stream ids whose timers have expired */
void TimeKeeper::notify_Elapsed( unsigned int elapsed_ms, 
                                 std::set< unsigned int >& expired_streams )
{
    int new_min_to = TimeKeeper::default_timeout;

    _tk_mutex.Lock();

    std::map< unsigned int, unsigned int >::iterator miter = _strm_timeouts.begin();
    while( miter != _strm_timeouts.end() ) {

        unsigned int strm_id = miter->first;
        unsigned int strm_to = miter->second;
        
        if( strm_to <= elapsed_ms ) {
            // elapsed > stream timeout, so remove from map and add to expired
            std::map< unsigned int, unsigned int >::iterator tmp = miter++;
            _strm_timeouts.erase( tmp );
            expired_streams.insert( strm_id );
        }
        else {
            // subtract elapsed time
            miter->second -= elapsed_ms;
        
            // update min timeout if appropriate
            if( (int)miter->second < new_min_to )
                new_min_to = miter->second;
    
            miter++;
        }
    }

    _min_timeout = new_min_to;

    _tk_mutex.Unlock();
}

/* register/clear timeout for specified stream */
bool TimeKeeper::register_Timeout( unsigned int strm_id, unsigned int timeout_ms )
{
    bool rc = true;

    _tk_mutex.Lock();

    std::map< unsigned int, unsigned int >::iterator miter = _strm_timeouts.find( strm_id );
    if( miter == _strm_timeouts.end() ) {
        
        // add to map
        _strm_timeouts[strm_id] = timeout_ms;

        // update min timeout if appropriate
        if( (int)timeout_ms < _min_timeout )
            _min_timeout = (int)timeout_ms;
    }
    else
        rc = false;

    _tk_mutex.Unlock();

    return rc;
}

bool TimeKeeper::clear_Timeout( unsigned int strm_id )
{
     bool rc = true;

    _tk_mutex.Lock();

    std::map< unsigned int, unsigned int >::iterator miter = _strm_timeouts.find( strm_id );
    if( miter != _strm_timeouts.end() ) {

        unsigned int strm_to = miter->second;
        _strm_timeouts.erase( miter );        

        if( _min_timeout == (int)strm_to ) {
            // find new minimum timeout
            std::set< unsigned int > dummy;
            notify_Elapsed( 0, dummy );
        }
    }
    else
        rc = false;

    _tk_mutex.Unlock();

    return rc;
}


} // namespace MRN
