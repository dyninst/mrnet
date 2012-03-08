/****************************************************************************
 * Copyright © 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Mutex-pthread.C,v 1.5 2008/10/09 19:54:11 mjbrim Exp $
#include "Mutex-pthread.h"

struct Mutex_t* Mutex_construct( void )
{
    int ret;
    static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;

    struct Mutex_t* mut = NULL;

    pthread_mutex_lock( &init_mutex );
    pthread_rwlock_t *c = NULL;

    /* Construct *data */
    mut = (struct Mutex_t*) calloc( (size_t)1, sizeof(struct Mutex_t) );
    assert(mut != NULL);
    mut->data = PthreadMutexData_construct();
    assert(mut->data != NULL);

    /* Construct lock */
    c = (pthread_rwlock_t *)malloc(sizeof(pthread_rwlock_t));
    ret = pthread_rwlock_init(c, NULL);
    assert(!ret);
    mut->cleanup_initialized = true;
    mut->cleanup = (void *)c;

    pthread_mutex_unlock( &init_mutex );

    return mut;
}

int Mutex_destruct( struct Mutex_t* m )
{
    int rc = 0;
    int ret;
    
    if(m == NULL)
        return -1;

    ret = pthread_rwlock_wrlock((pthread_rwlock_t *)m->cleanup);

    if(m->cleanup == NULL)
        return -1;
    assert(!ret);

    if( m->data != NULL ) {
        PthreadMutexData_destruct( m->data );
        free(m->data);
        m->data = NULL;
    }
    else
        rc = -1;

    m->cleanup_initialized = 0;
    ret = pthread_rwlock_unlock((pthread_rwlock_t *)m->cleanup);
    assert(!ret);

    ret = pthread_rwlock_destroy((pthread_rwlock_t *)m->cleanup);
    assert(!ret);
    m->cleanup = NULL;

    return rc;
}

int Mutex_Lock( struct Mutex_t* m )
{
    int ret;
    if( (m != NULL) && m->cleanup_initialized && (m->cleanup != NULL) ) {
        ret = pthread_rwlock_rdlock((pthread_rwlock_t *)m->cleanup);
        if(ret)
            return ret;
        if( m->data != NULL ) {
            ret = PthreadMutex_Lock( m->data );
            assert(!pthread_rwlock_unlock((pthread_rwlock_t *)m->cleanup));
            return ret;
        }
        assert(!pthread_rwlock_unlock((pthread_rwlock_t *)m->cleanup));
        return EINVAL;
    }

    return EINVAL;
}

int Mutex_Unlock( struct Mutex_t* m )
{
    int ret;
    if( (m != NULL) && m->cleanup_initialized && (m->cleanup != NULL) ) {
        ret = pthread_rwlock_rdlock((pthread_rwlock_t *)m->cleanup);
        if(ret)
            return ret;
        if( m->data != NULL ) {
            ret = PthreadMutex_Unlock( m->data );
            assert(!pthread_rwlock_unlock((pthread_rwlock_t *)m->cleanup));
            return ret;
        }
        assert(!pthread_rwlock_unlock((pthread_rwlock_t *)m->cleanup));
        return EINVAL;
    }

    return EINVAL;
}

int Mutex_Trylock( struct Mutex_t* m )
{
    int ret;
    if( (m != NULL) && m->cleanup_initialized && (m->cleanup != NULL) ) {
        ret = pthread_rwlock_rdlock((pthread_rwlock_t *)m->cleanup);
        if(ret)
            return ret;
        if( m->data != NULL ) {
            ret = PthreadMutex_Unlock( m->data );
            assert(!pthread_rwlock_unlock((pthread_rwlock_t *)m->cleanup));
            return ret;
        }
        assert(!pthread_rwlock_unlock((pthread_rwlock_t *)m->cleanup));
        return EINVAL;
    }

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
