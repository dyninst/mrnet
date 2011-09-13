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
    pthread_mutex_lock( &init_mutex );

    data = new PthreadMonitorData;

    pthread_mutex_unlock( &init_mutex );
}

Monitor::~Monitor( void )
{
    static pthread_mutex_t cleanup_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock( &cleanup_mutex );

    if( data != NULL ) {
        delete data;
        data = NULL;
    }

    pthread_mutex_unlock( &cleanup_mutex );
}

int Monitor::Lock( void )
{
    if( data != NULL )
        return data->Lock();
    return EINVAL;
}

int Monitor::Unlock( void )
{
    if( data != NULL )
        return data->Unlock();
    return EINVAL;
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

