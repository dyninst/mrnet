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
#include "xplat_dbg.h"

namespace XPlat
{

Monitor::Monitor( void )
{
    static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
    int ret;
    bool failed = false;
    pthread_mutex_lock( &init_mutex );

    data = new PthreadMonitorData;
    pthread_rwlock_t *c = new pthread_rwlock_t;
    if(data == NULL || c == NULL) {
        failed = true;
        goto ctor_fail;
    }

    ret = pthread_rwlock_init(c, NULL);
    if(ret) {
        failed = true;
        goto ctor_fail;
    }
    destruct_sync_initialized = true;

    destruct_sync = (void *)c;

ctor_fail:
    pthread_mutex_unlock( &init_mutex );
    if(failed) {
        xplat_dbg(1, fprintf(stderr, 
                     "XPlat Error: Failed to construct Monitor\n"));
        destruct_sync_initialized = false;
        destruct_sync = NULL;
        data = NULL;
    }
}

Monitor::~Monitor( void )
{
    int ret;

    ret = pthread_rwlock_wrlock((pthread_rwlock_t *)destruct_sync);
    
    // Make sure no one destroys the rwlock twice
    if(destruct_sync == NULL) {
        return;
    }
    
    // Check for wrlock error
    if(ret) {
        xplat_dbg(1, fprintf(stderr, 
                     "XPlat Error: destruct_sync wrlock returned '%s'\n",
                     strerror( ret )));
        return;
    }

    if( data != NULL ) {
        delete data;
        data = NULL;
    }
    
    destruct_sync_initialized = false;
    ret = pthread_rwlock_unlock((pthread_rwlock_t *)destruct_sync);
    if(ret) { 
        xplat_dbg(1, fprintf(stderr, 
                     "XPlat Error: destruct_sync unlock returned '%s'\n",
                     strerror( ret )));
    }

    ret = pthread_rwlock_destroy((pthread_rwlock_t *)destruct_sync);
    if(ret) { 
        xplat_dbg(1, fprintf(stderr, 
                     "XPlat Error: destruct_sync destroy returned '%s'\n",
                     strerror( ret )));
    }
    destruct_sync = NULL;
}

int Monitor::Lock( void )
{
    int ret, rw_ret;
    if( destruct_sync_initialized && (destruct_sync != NULL) ) {
        ret = pthread_rwlock_rdlock((pthread_rwlock_t *)destruct_sync);
        if(ret)
            return ret;

        if( data != NULL ) {
            ret = data->Lock();
            rw_ret = pthread_rwlock_unlock((pthread_rwlock_t *)destruct_sync);
            if(rw_ret) {
                xplat_dbg(1, fprintf(stderr, 
                            "XPlat Error: destruct_sync unlock returned '%s'\n",
                            strerror( rw_ret )));
            }
            return ret;
        }
        rw_ret = pthread_rwlock_unlock((pthread_rwlock_t *)destruct_sync);
        if(rw_ret) {
            xplat_dbg(1, fprintf(stderr, 
                        "XPlat Error: destruct_sync unlock returned '%s'\n",
                        strerror( rw_ret )));
        }
        return EINVAL;
    }

    return EINVAL;
}

int Monitor::Unlock( void )
{
    int ret, rw_ret;
    if( destruct_sync_initialized && (destruct_sync != NULL) ) {
        ret = pthread_rwlock_rdlock((pthread_rwlock_t *)destruct_sync);
        if(ret)
            return ret;

        if( data != NULL ) {
            ret = data->Unlock();
            rw_ret = pthread_rwlock_unlock((pthread_rwlock_t *)destruct_sync);
            if(rw_ret) {
                xplat_dbg(1, fprintf(stderr, 
                            "XPlat Error: destruct_sync unlock returned '%s'\n",
                            strerror( rw_ret )));
            }
            return ret;
        }
        rw_ret = pthread_rwlock_unlock((pthread_rwlock_t *)destruct_sync);
        if(rw_ret) {
            xplat_dbg(1, fprintf(stderr, 
                         "XPlat Error: destruct_sync unlock returned '%s'\n",
                         strerror( rw_ret )));
        }
        return EINVAL;
    }

    return EINVAL;
}

int Monitor::RegisterCondition( unsigned int condid )
{
    int ret, rw_ret;
    if( destruct_sync_initialized && (destruct_sync != NULL) ) {
        ret = pthread_rwlock_rdlock((pthread_rwlock_t *)destruct_sync);
        if(ret)
            return ret;

        if( data != NULL ) {
            ret = data->RegisterCondition( condid );
            rw_ret = pthread_rwlock_unlock((pthread_rwlock_t *)destruct_sync);
            if(rw_ret) {
                xplat_dbg(1, fprintf(stderr, 
                            "XPlat Error: destruct_sync unlock returned '%s'\n",
                            strerror( rw_ret )));
            }
            return ret;
        }
        rw_ret = pthread_rwlock_unlock((pthread_rwlock_t *)destruct_sync);
        if(rw_ret) {
            xplat_dbg(1, fprintf(stderr, 
                         "XPlat Error: destruct_sync unlock returned '%s'\n",
                         strerror( rw_ret )));
        }
        return -1;
    }

    return -1;
}

int Monitor::WaitOnCondition( unsigned int condid )
{
    int ret, rw_ret;
    if( destruct_sync_initialized && (destruct_sync != NULL) ) {
        ret = pthread_rwlock_rdlock((pthread_rwlock_t *)destruct_sync);
        if(ret)
            return ret;

        if( data != NULL ) {
            ret = data->WaitOnCondition( condid );
            rw_ret = pthread_rwlock_unlock((pthread_rwlock_t *)destruct_sync);
            if(rw_ret) {
                xplat_dbg(1, fprintf(stderr, 
                            "XPlat Error: destruct_sync unlock returned '%s'\n",
                            strerror( rw_ret )));
            }
            return ret;
        }
        rw_ret = pthread_rwlock_unlock((pthread_rwlock_t *)destruct_sync);
        if(rw_ret) {
            xplat_dbg(1, fprintf(stderr, 
                         "XPlat Error: destruct_sync unlock returned '%s'\n",
                         strerror( rw_ret )));
        }
        return -1;
    }

    return -1;
}

int Monitor::SignalCondition( unsigned int condid )
{
    int ret, rw_ret;
    if( destruct_sync_initialized && (destruct_sync != NULL) ) {
        ret = pthread_rwlock_rdlock((pthread_rwlock_t *)destruct_sync);
        if(ret)
            return ret;

        if( data != NULL ) {
            ret = data->SignalCondition( condid );
            rw_ret = pthread_rwlock_unlock((pthread_rwlock_t *)destruct_sync);
            if(rw_ret) {
                xplat_dbg(1, fprintf(stderr, 
                            "XPlat Error: destruct_sync unlock returned '%s'\n",
                            strerror( rw_ret )));
            }
            return ret;
        }
        rw_ret = pthread_rwlock_unlock((pthread_rwlock_t *)destruct_sync);
        if(rw_ret) {
            xplat_dbg(1, fprintf(stderr, 
                         "XPlat Error: destruct_sync unlock returned '%s'\n",
                         strerror( rw_ret )));
        }
        return -1;
    }

    return -1;
}

int Monitor::BroadcastCondition( unsigned int condid )
{
    int ret, rw_ret;
    if( destruct_sync_initialized && (destruct_sync != NULL) ) {
        ret = pthread_rwlock_rdlock((pthread_rwlock_t *)destruct_sync);
        if(ret)
            return ret;

        if( data != NULL ) {
            ret = data->BroadcastCondition( condid );
            rw_ret = pthread_rwlock_unlock((pthread_rwlock_t *)destruct_sync);
            if(rw_ret) {
                xplat_dbg(1, fprintf(stderr, 
                             "XPlat Error: destruct_sync unlock returned '%s'\n",
                             strerror( rw_ret )));
            }
            return ret;
        }
        rw_ret = pthread_rwlock_unlock((pthread_rwlock_t *)destruct_sync);
        if(rw_ret) {
            xplat_dbg(1, fprintf(stderr, 
                         "XPlat Error: destruct_sync unlock returned '%s'\n",
                         strerror( rw_ret )));
        }
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

