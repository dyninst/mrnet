/****************************************************************************
 * Copyright © 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Monitor-pthread.C,v 1.7 2008/10/09 19:54:09 mjbrim Exp $
#include <cassert>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include "Monitor-pthread.h"

namespace XPlat
{

Monitor::Monitor( void )
{
    static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
    int ret;
    pthread_mutex_lock( &init_mutex );

    data = new PthreadMonitorData;
    pthread_rwlock_t *c = new pthread_rwlock_t;
    assert(data != NULL);
    assert(c != NULL);

    ret = pthread_rwlock_init(c, NULL);
    assert(!ret);
    cleanup_initialized = true;

    cleanup = (void *)c;

    pthread_mutex_unlock( &init_mutex );
}

Monitor::~Monitor( void )
{
    int ret;

    ret = pthread_rwlock_wrlock((pthread_rwlock_t *)cleanup);
    
    // Make sure no one destroys the rwlock twice
    if(cleanup == NULL) {
        return;
    }
    assert(!ret);

    if( data != NULL ) {
        delete data;
        data = NULL;
    }
    
    cleanup_initialized = false;
    ret = pthread_rwlock_unlock((pthread_rwlock_t *)cleanup);
    assert(!ret);

    ret = pthread_rwlock_destroy((pthread_rwlock_t *)cleanup);
    assert(!ret);
    cleanup = NULL;
}

int Monitor::Lock( void )
{
    int ret;
    if( cleanup_initialized && (cleanup != NULL) ) {
        ret = pthread_rwlock_rdlock((pthread_rwlock_t *)cleanup);
        if(ret)
            return ret;

        if( data != NULL ) {
            ret = data->Lock();
            assert(!pthread_rwlock_unlock((pthread_rwlock_t *)cleanup));
            return ret;
        }
        assert(!pthread_rwlock_unlock((pthread_rwlock_t *)cleanup));
        return EINVAL;
    }

    return EINVAL;
}

int Monitor::Unlock( void )
{
    int ret;
    if( cleanup_initialized && (cleanup != NULL) ) {
        ret = pthread_rwlock_rdlock((pthread_rwlock_t *)cleanup);
        if(ret)
            return ret;

        if( data != NULL ) {
            ret = data->Unlock();
            assert(!pthread_rwlock_unlock((pthread_rwlock_t *)cleanup));
            return ret;
        }
        assert(!pthread_rwlock_unlock((pthread_rwlock_t *)cleanup));
        return EINVAL;
    }

    return EINVAL;
}

int Monitor::RegisterCondition( unsigned int condid )
{
    int ret;
    if( cleanup_initialized && (cleanup != NULL) ) {
        ret = pthread_rwlock_rdlock((pthread_rwlock_t *)cleanup);
        if(ret)
            return ret;

        if( data != NULL ) {
            ret = data->RegisterCondition( condid );
            assert(!pthread_rwlock_unlock((pthread_rwlock_t *)cleanup));
            return ret;
        }
        assert(!pthread_rwlock_unlock((pthread_rwlock_t *)cleanup));
        return -1;
    }

    return -1;
}

int Monitor::WaitOnCondition( unsigned int condid )
{
    int ret;
    if( cleanup_initialized && (cleanup != NULL) ) {
        ret = pthread_rwlock_rdlock((pthread_rwlock_t *)cleanup);
        if(ret)
            return ret;

        if( data != NULL ) {
            ret = data->WaitOnCondition( condid );
            assert(!pthread_rwlock_unlock((pthread_rwlock_t *)cleanup));
            return ret;
        }
        assert(!pthread_rwlock_unlock((pthread_rwlock_t *)cleanup));
        return -1;
    }

    return -1;
}

int Monitor::SignalCondition( unsigned int condid )
{
    int ret;
    if( cleanup_initialized && (cleanup != NULL) ) {
        ret = pthread_rwlock_rdlock((pthread_rwlock_t *)cleanup);
        if(ret)
            return ret;

        if( data != NULL ) {
            ret = data->SignalCondition( condid );
            assert(!pthread_rwlock_unlock((pthread_rwlock_t *)cleanup));
            return ret;
        }
        assert(!pthread_rwlock_unlock((pthread_rwlock_t *)cleanup));
        return -1;
    }

    return -1;
}

int Monitor::BroadcastCondition( unsigned int condid )
{
    int ret;
    if( cleanup_initialized && (cleanup != NULL) ) {
        ret = pthread_rwlock_rdlock((pthread_rwlock_t *)cleanup);
        if(ret)
            return ret;

        if( data != NULL ) {
            ret = data->BroadcastCondition( condid );
            assert(!pthread_rwlock_unlock((pthread_rwlock_t *)cleanup));
            return ret;
        }
        assert(!pthread_rwlock_unlock((pthread_rwlock_t *)cleanup));
        return -1;
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
        fprintf(stderr, "XPlat Error: pthread_mutex_lock() returned '%s'\n",
                strerror( rc ));
        fflush(stderr);
    }
    return rc;
}

int
PthreadMonitorData::Unlock( void )
{
    int rc = pthread_mutex_unlock( &mutex ); 
    if( rc ) {
        fprintf(stderr, "XPlat Error: pthread_mutex_unlock() returned '%s'\n",
                strerror( rc ));
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

