/****************************************************************************
 * Copyright © 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Mutex-pthread.C,v 1.5 2008/10/09 19:54:11 mjbrim Exp $
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include "Mutex-pthread.h"
#include "xplat/xplat_utils.h"

namespace XPlat
{

Mutex::Mutex( void )
{
    static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
    int ret;
    bool failed = false;
    pthread_mutex_lock( &init_mutex );

    data = new PthreadMutexData;
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
        xplat_dbg(1, xplat_printf(FLF, stderr, 
                     "XPlat Error: Failed to construct Mutex\n"));
        destruct_sync_initialized = false;
        destruct_sync = NULL;
        data = NULL;
    }
}

Mutex::~Mutex( void )
{
    int ret;

    ret = pthread_rwlock_wrlock((pthread_rwlock_t *)destruct_sync);

    // Make sure no one destroys the rwlock twice
    if(destruct_sync == NULL) {
        return;
    }

    // Check for wrlock error
    if(ret) {
        xplat_dbg(1, xplat_printf(FLF, stderr, 
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
        xplat_dbg(1, xplat_printf(FLF, stderr, 
                    "XPlat Error: destruct_sync unlock returned '%s'\n",
                    strerror( ret )));
    }

    ret = pthread_rwlock_destroy((pthread_rwlock_t *)destruct_sync);
    if(ret) {
        xplat_dbg(1, xplat_printf(FLF, stderr, 
                    "XPlat Error: destruct_sync destroy returned '%s'\n",
                    strerror( ret )));
    }
    destruct_sync = NULL;
}

int Mutex::Lock( void )
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
                xplat_dbg(1, xplat_printf(FLF, stderr, 
                            "XPlat Error: destruct_sync unlock returned '%s'\n",
                            strerror( rw_ret )));
            }
            return ret;
        }

        rw_ret = pthread_rwlock_unlock((pthread_rwlock_t *)destruct_sync);
        if(rw_ret) {
            xplat_dbg(1, xplat_printf(FLF, stderr, 
                        "XPlat Error: destruct_sync unlock returned '%s'\n",
                        strerror( rw_ret )));
        }
        return EINVAL;
    }
        
    return EINVAL;
}

int Mutex::Unlock( void )
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
				xplat_dbg(1, xplat_printf(FLF, stderr, 
							"XPlat Error: destruct_sync unlock returned '%s'\n",
							strerror( rw_ret )));
			}
            return ret;
        }

        rw_ret = pthread_rwlock_unlock((pthread_rwlock_t *)destruct_sync);
        if(rw_ret) {
            xplat_dbg(1, xplat_printf(FLF, stderr, 
                        "XPlat Error: destruct_sync unlock returned '%s'\n",
                        strerror( rw_ret )));
        }
        return EINVAL;
    }

    return EINVAL;
}

PthreadMutexData::PthreadMutexData( void )
  : initialized(false)
{
    if( ! initialized ) {
        int ret;
        pthread_mutexattr_t mattrs;

        ret = pthread_mutexattr_init( &mattrs );
        assert( ret == 0 );
        ret = pthread_mutexattr_settype( &mattrs, PTHREAD_MUTEX_ERRORCHECK );
        assert( ret == 0 );

        ret = pthread_mutex_init( &mutex, &mattrs );
        if( ret == 0 ) initialized = true;
    }
}

PthreadMutexData::~PthreadMutexData( void )
{
    if( initialized ) {
        initialized = false;
        pthread_mutex_destroy( &mutex );
    }
}

int PthreadMutexData::Lock( void )
{
    return pthread_mutex_lock( &mutex );
}

int PthreadMutexData::Unlock( void )
{
    return pthread_mutex_unlock( &mutex );
}

} // namespace XPlat
