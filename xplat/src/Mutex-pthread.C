/****************************************************************************
 * Copyright © 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Mutex-pthread.C,v 1.5 2008/10/09 19:54:11 mjbrim Exp $
#include <cassert>
#include <cerrno>
#include "Mutex-pthread.h"

namespace XPlat
{

Mutex::Mutex( void )
{
    int ret;
    static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock( &init_mutex );

    data = new PthreadMutexData;
    pthread_rwlock_t *c = new pthread_rwlock_t;
    assert(data != NULL);
    assert(c != NULL);

    ret = pthread_rwlock_init(c, NULL);
    assert(!ret);
    cleanup_initialized = true;

    cleanup = (void *)c;

    pthread_mutex_unlock( &init_mutex );
}

Mutex::~Mutex( void )
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

int Mutex::Lock( void )
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

int Mutex::Unlock( void )
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
