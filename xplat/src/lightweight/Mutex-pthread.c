/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Mutex-pthread.C,v 1.5 2008/10/09 19:54:11 mjbrim Exp $
#include "Mutex-pthread.h"
#include "xplat_lightweight/xplat_utils_lightweight.h"

struct Mutex_t* Mutex_construct( void )
{
    static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
    int failed = 0;

    struct Mutex_t* mut = NULL;

    pthread_mutex_lock( &init_mutex );

    /* Construct *data */
    mut = (struct Mutex_t*) calloc( (size_t)1, sizeof(struct Mutex_t) );
    if(mut == NULL) {
        failed = 1;
        goto ctor_fail;
    }
    mut->data = PthreadMutexData_construct();
    if(mut->data == NULL) {
        failed = 1;
        goto ctor_fail;
    }

ctor_fail:
    pthread_mutex_unlock( &init_mutex );
    if(failed) {
        mut->data = NULL;
        mut = NULL;
    }

    return mut;
}

int Mutex_destruct( struct Mutex_t* m )
{
    static pthread_mutex_t cleanup_mutex = PTHREAD_MUTEX_INITIALIZER;
    int rc = 0;
    
    pthread_mutex_lock( &cleanup_mutex );

    if( (m != NULL) && m->data != NULL ) {
        PthreadMutexData_destruct( m->data );
        free(m->data);
        m->data = NULL;
    }
    else
        rc = -1;

    pthread_mutex_unlock( &cleanup_mutex );

    return rc;
}

int Mutex_Lock( struct Mutex_t* m )
{
    if( (m != NULL) && (m->data != NULL) )
        return PthreadMutex_Lock( m->data );
    else
        return EINVAL;
}

int Mutex_Unlock( struct Mutex_t* m )
{
    if( (m != NULL) && (m->data != NULL) )
        return PthreadMutex_Unlock( m->data );
    else
        return EINVAL;
}

int Mutex_Trylock( struct Mutex_t* m )
{
    if( (m != NULL) && (m->data != NULL) )
        return PthreadMutex_Trylock( m->data );
    else
        return EINVAL;
}

PthreadMutexData_t* PthreadMutexData_construct( void )
{
    int ret;
    pthread_mutexattr_t mattrs;
    PthreadMutexData_t* pmd = NULL;
    pmd = (PthreadMutexData_t*) calloc( (size_t)1, sizeof(PthreadMutexData_t) );
    if( pmd != NULL ) {
        ret = pthread_mutexattr_init( &mattrs );
        assert(ret == 0);
        ret = pthread_mutexattr_settype( &mattrs, PTHREAD_MUTEX_ERRORCHECK );
        assert(ret == 0);
        ret = pthread_mutex_init( &(pmd->mutex), &mattrs );
        if( ret == 0 ) pmd->initialized = 1;
    }
    return pmd;
}

int PthreadMutexData_destruct( PthreadMutexData_t* pmd )
{
    if( (pmd != NULL) && pmd->initialized ) {
        pmd->initialized = 0;
        pthread_mutex_destroy( &(pmd->mutex) );
	return 0;
    }
    return -1;
}

int PthreadMutex_Lock( PthreadMutexData_t* pmd )
{
    return pthread_mutex_lock( &(pmd->mutex) );
}

int PthreadMutex_Unlock( PthreadMutexData_t* pmd )
{
    return pthread_mutex_unlock( &(pmd->mutex) );
}

int PthreadMutex_Trylock( PthreadMutexData_t* pmd )
{
    return pthread_mutex_trylock( &(pmd->mutex) );
}
