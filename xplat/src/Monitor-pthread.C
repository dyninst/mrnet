/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Monitor-pthread.C,v 1.7 2008/10/09 19:54:09 mjbrim Exp $
#include <cassert>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <climits>
#include "Monitor-pthread.h"
#include "xplat/xplat_utils.h"

namespace XPlat
{

Monitor::Monitor( void )
{
    static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
    int ret;
    bool failed = false;
    pthread_mutex_lock( &init_mutex );

    data = new PthreadMonitorData;
    if(data == NULL) {
        failed = true;
        goto ctor_fail;
    }

ctor_fail:
    pthread_mutex_unlock( &init_mutex );
    if(failed) {
        xplat_dbg(1, xplat_printf(FLF, stderr, 
                     "Error: Failed to construct Monitor\n"));
        data = NULL;
    }
}

Monitor::~Monitor( void )
{
    if( data != NULL ) {
        delete data;
        data = NULL;
    }
}

int Monitor::Lock( void )
{
    int ret = -1;
    if( data != NULL ) {
        ret = data->Lock();
        return ret;
    }

    return EINVAL;
}

int Monitor::Unlock( void )
{
    int ret = -1;
    if( data != NULL ) {
        ret = data->Unlock();
        return ret;
    }

    return EINVAL;
}

int Monitor::RegisterCondition( unsigned int condid )
{
    int ret = -1;
    if( data != NULL ) {
        ret = data->RegisterCondition( condid );
        return ret;
    }

    return -1;
}

int Monitor::WaitOnCondition( unsigned int condid )
{
    int ret = -1;
    if( data != NULL ) {
        ret = data->WaitOnCondition( condid );
        return ret;
    }

    return -1;
}

int Monitor::TimedWaitOnCondition( unsigned int condid, int milliseconds )
{
    int ret = -1;
    if( data != NULL ) {
        ret = data->TimedWaitOnCondition( condid, milliseconds );
        return ret;
    }

    return -1;
}

int Monitor::SignalCondition( unsigned int condid )
{
    int ret = -1;
    if( data != NULL ) {
        ret = data->SignalCondition( condid );
        return ret;
    }

    return -1;
}

int Monitor::BroadcastCondition( unsigned int condid )
{
    int ret = -1;
    if( data != NULL ) {
        ret = data->BroadcastCondition( condid );
        return ret;
    }

    return -1;
}

PthreadMonitorData::PthreadMonitorData( void )
  : initialized( false )
{
    if( ! initialized ) {
        int ret;
        pthread_mutexattr_t attrs;
    
        ret = pthread_mutexattr_init( & attrs );
        assert( ret == 0 );
        ret = pthread_mutexattr_settype( & attrs, PTHREAD_MUTEX_RECURSIVE );
        assert( ret == 0 );

        ret = pthread_mutex_init( & mutex, & attrs );
        assert( ret == 0 );
        initialized = true;
    }
}

PthreadMonitorData::~PthreadMonitorData( void )
{
    if( initialized ) {
        initialized = false;

        for( ConditionVariableMap::iterator iter = cvmap.begin();
             iter != cvmap.end(); iter++ ) {
            pthread_cond_destroy( iter->second );
            delete iter->second;
        }
        cvmap.clear();

        pthread_mutex_destroy( &mutex );
    }
}

int
PthreadMonitorData::Lock( void )
{
    int rc = pthread_mutex_lock( &mutex ); 
    if( rc ) {
        xplat_dbg( 1, xplat_printf(FLF, stderr,
                      "Error: pthread_mutex_lock() returned '%s'\n",
                      strerror( rc )));
        fflush(stderr);
    }
    return rc;
}

int
PthreadMonitorData::Unlock( void )
{
    int rc = pthread_mutex_unlock( &mutex ); 
    if( rc ) {
        xplat_dbg( 1, xplat_printf(FLF, stderr,
                      "Error: pthread_mutex_unlock() returned '%s'\n",
                      strerror( rc )));
        fflush(stderr);
    }
    return rc;
}

int
PthreadMonitorData::RegisterCondition( unsigned int cvid )
{
    int ret = 0;

    pthread_cond_t* newcv = new pthread_cond_t;
    if( pthread_cond_init( newcv, NULL ) == 0 )
    {
        cvmap[cvid] = newcv;
    }
    else
    {
        delete newcv;
        ret = -1;
    }
    return ret;
}


int
PthreadMonitorData::WaitOnCondition( unsigned int cvid )
{
    int ret = -1;

    ConditionVariableMap::iterator iter = cvmap.find( cvid );
    if( iter != cvmap.end() )
    {
        pthread_cond_t* cv = cvmap[cvid]; 
        assert( cv != NULL );
        ret = pthread_cond_wait( cv, &mutex );
    }
    else
    {
        // bad condition variable id
        // TODO how to indicate the error
    	assert( 0 );
    }
    return ret;
}


// Returns -1 for a fatal error, 0 on success, and 1 if the time given in 
// milliseconds has expired before the condition variable has been signalled.
int
PthreadMonitorData::TimedWaitOnCondition( unsigned int cvid, int milliseconds )
{
    int gt_ret, ret = -1;

    if( milliseconds < 0 ) {
        return ret;
    }

    pthread_cond_t *cv = NULL;
    ConditionVariableMap::iterator iter = cvmap.find( cvid );
    if( iter != cvmap.end() ) {
        cv = cvmap[cvid];
        if( NULL == cv ) {
            xplat_dbg(1, xplat_printf(FLF, stderr, 
                                      "NULL condition variable\n"));
            return ret;
        }
    } 
    else {
        return ret;
    }

    // Get time of day for 'abstime' arg of pthread_cond_timedwait
    struct timeval tv;
    gt_ret = gettimeofday( &tv, NULL );
    if( -1 == gt_ret ) {
        int err = errno;
        xplat_dbg(1, xplat_printf(FLF, stderr,
                                  "gettimeofday() failed - %s\n", 
                                  strerror(err)));
        return ret;
    }

    time_t sec = milliseconds / 1000;
    long msec = milliseconds % 1000;
    long nanosec = msec * 1000000L; // ms -> ns
    sec += tv.tv_sec;
    nanosec += (tv.tv_usec * 1000L);
    if( nanosec >= 1000000000L ) {
        nanosec -= 1000000000L;
        sec++;
    }

    struct timespec ts;
    ts.tv_sec = sec;
    ts.tv_nsec = nanosec;

    ret = pthread_cond_timedwait( cv, &mutex, &ts );
    if( ETIMEDOUT == ret ) {
        xplat_dbg(3, xplat_printf(FLF, stderr,  "time out!\n"));
    }
    else if( ret ) {
        xplat_dbg(1, xplat_printf(FLF, stderr,
                                  "pthread_cond_timedwait() failed - %s\n", 
                                  strerror(ret)));
        ret = -1;
    }

    return ret;
}


int
PthreadMonitorData::SignalCondition( unsigned int cvid )
{
    int ret = -1;

    ConditionVariableMap::iterator iter = cvmap.find( cvid );
    if( iter != cvmap.end() )
    {
        pthread_cond_t* cv = cvmap[cvid];
        assert( cv != NULL );
        ret = pthread_cond_signal( cv );
    }
    else
    {
        // bad condition variable id
        // TODO how to indicate the error?
    }
    return ret;
}


int
PthreadMonitorData::BroadcastCondition( unsigned int cvid )
{
    int ret = -1;

    ConditionVariableMap::iterator iter = cvmap.find( cvid );
    if( iter != cvmap.end() )
    {
        pthread_cond_t* cv = cvmap[cvid];
        assert( cv != NULL );
        ret = pthread_cond_broadcast( cv );
    }
    else
    {
        // bad condition variable id
        // TODO how to indicate the error?
    }
    return ret;
}

} // namespace XPlat

