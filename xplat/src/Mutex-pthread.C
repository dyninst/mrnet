/****************************************************************************
 * Copyright © 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
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
    static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock( &init_mutex );

    data = new PthreadMutexData;

    pthread_mutex_unlock( &init_mutex );
}

Mutex::~Mutex( void )
{
    static pthread_mutex_t cleanup_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock( &cleanup_mutex );

    if( data != NULL ) {
        delete data;
        data = NULL;
    }

    pthread_mutex_unlock( &cleanup_mutex );
}

int Mutex::Lock( void )
{
    if( data != NULL )
        return data->Lock();
    else
        return EINVAL;
}

int Mutex::Unlock( void )
{
    if( data != NULL )
        return data->Unlock();
    else
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
