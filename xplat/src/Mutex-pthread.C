/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
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
    if(data == NULL) {
        failed = true;
        goto ctor_fail;
    }

ctor_fail:
    pthread_mutex_unlock( &init_mutex );
    if(failed) {
        xplat_dbg(1, xplat_printf(FLF, stderr, 
                     "Error: Failed to construct Mutex\n"));
        data = NULL;
    }
}

Mutex::~Mutex( void )
{
    if( data != NULL ) {
        delete data;
        data = NULL;
    }
}

int Mutex::Lock( void )
{
    int ret = -1;
    if( data != NULL ) {
        return data->Lock();
    }

    return EINVAL;
}

int Mutex::Unlock( void )
{
    int ret = -1;
    if( data != NULL ) {
        return data->Unlock();
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
