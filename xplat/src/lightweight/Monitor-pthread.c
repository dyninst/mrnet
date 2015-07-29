/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Monitor-pthread.C,v 1.7 2008/10/09 19:54:09 mjbrim Exp $
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "Monitor-pthread.h"
#include "xplat_lightweight/xplat_utils_lightweight.h"

Monitor_t* Monitor_construct( void )
{
    static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
    int failed = 0;

    Monitor_t* mon = NULL;

    pthread_mutex_lock( &init_mutex );

    mon = (Monitor_t*)calloc((size_t)1, sizeof(Monitor_t));
    if(mon == NULL) {
        failed = 1;
        goto ctor_fail;
    }

    mon->data = PthreadMonitorData_construct();
    if(mon->data == NULL) {
        failed = 1;
        goto ctor_fail;
    }

ctor_fail:
    pthread_mutex_unlock( &init_mutex );
    if(failed) {
        mon->data = NULL;
        mon = NULL;
    }
    return mon;
}

int Monitor_destruct( Monitor_t* m )
{
    static pthread_mutex_t cleanup_mutex = PTHREAD_MUTEX_INITIALIZER;
    int rc = 0;

    pthread_mutex_lock( &cleanup_mutex );
    
    if( (m != NULL) && (m->data != NULL) ) {
        PthreadMonitorData_destruct( m->data );
        free(m->data);
        m->data = NULL;
    }
    else
        rc = -1;

    pthread_mutex_unlock( &cleanup_mutex );

    return rc;
}

int Monitor_Lock( Monitor_t* m )
{
    if( (m != NULL) && (m->data != NULL) )
        return PthreadMonitor_Lock( m->data );
    else
        return EINVAL;
}

int Monitor_Unlock( Monitor_t* m )
{
    if( (m != NULL) && (m->data != NULL) )
        return PthreadMonitor_Unlock( m->data );
    else
        return EINVAL;
}

int Monitor_Trylock( Monitor_t* m )
{
    if( (m != NULL) && (m->data != NULL) )
        return PthreadMonitor_Trylock( m->data );
    else
        return EINVAL;
}

int Monitor_RegisterCondition( Monitor_t* m, int condid )
{
    if( (m != NULL) && (m->data != NULL) )
        return PthreadMonitor_RegisterCondition( m->data, condid );
    else
        return EINVAL;
}

int Monitor_WaitOnCondition( Monitor_t* m, int condid )
{
    if( (m != NULL) && (m->data != NULL) )
        return PthreadMonitor_WaitOnCondition( m->data, condid );
    else
        return EINVAL;
}

int Monitor_SignalCondition( Monitor_t* m, int condid )
{
    if( (m != NULL) && (m->data != NULL) )
        return PthreadMonitor_SignalCondition( m->data, condid );
    else
        return EINVAL;
}

int Monitor_BroadcastCondition( Monitor_t* m, int condid )
{
    if( (m != NULL) && (m->data != NULL) )
        return PthreadMonitor_BroadcastCondition( m->data, condid );
    else
        return EINVAL;
}

PthreadMonitorData_t* PthreadMonitorData_construct( void )
{
    int ret;
    pthread_mutexattr_t attrs;
    PthreadMonitorData_t *pmond = NULL;
    pmond = (PthreadMonitorData_t*)calloc((size_t)1,
            sizeof(PthreadMonitorData_t));
    if( pmond != NULL ) {
        ret = pthread_mutexattr_init( &attrs );
        assert(ret == 0);
        ret = pthread_mutexattr_settype( &attrs, PTHREAD_MUTEX_RECURSIVE );
        assert(ret == 0);
        ret = pthread_mutex_init( &(pmond->mutex), &attrs );
        assert(ret == 0);
        pmond->initialized = 1;
        pmond->cvmap = new_map_t();
    }
    return pmond;
}

int PthreadMonitorData_destruct( PthreadMonitorData_t* pmond )
{
    int i;
    int temp_key;
    if( (pmond != NULL) && pmond->initialized ) {
        /* Get each map and destroy condition */
        for(i = 0; i < (int)pmond->cvmap->size; i++) {
            temp_key = pmond->cvmap->keys[i];
            /* Not a pretty or efficient implementation. Adding functionality
             * to the map class to iterate would help, but probably not
             * necessary if just for this. */
            pthread_cond_destroy((pthread_cond_t*)get_val(pmond->cvmap, temp_key));
        }
        delete_map_t(pmond->cvmap);

        /* Destroy mutex */
        pmond->initialized = 0;
        pthread_mutex_destroy( &(pmond->mutex) );
        return 0;
    }
    return -1;
}

int PthreadMonitor_Lock( PthreadMonitorData_t* pmond )
{
    int rc = pthread_mutex_lock( &(pmond->mutex) ); 
    if( rc ) {
        xplat_dbg(1, xplat_printf(FLF, stderr,
                     "Error: pthread_mutex_lock() returned '%s'\n",
                     strerror( rc )));
        fflush(stderr);
    }
    return rc;
}

int PthreadMonitor_Unlock( PthreadMonitorData_t* pmond )
{
    int rc = pthread_mutex_unlock( &(pmond->mutex) ); 
    if( rc ) {
        xplat_dbg(1, xplat_printf(FLF, stderr,
                     "Error: pthread_mutex_unlock() returned '%s'\n",
                     strerror( rc )));
        fflush(stderr);
    }
    return rc;
}

int PthreadMonitor_Trylock( PthreadMonitorData_t* pmond )
{
    int rc = pthread_mutex_trylock( &(pmond->mutex) ); 
    return rc;
}

int PthreadMonitor_RegisterCondition( PthreadMonitorData_t* pmond, int cvid )
{
    int ret = 0;

    pthread_cond_t* newcv = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
    if( pthread_cond_init( newcv, NULL ) == 0 ) {
        insert(pmond->cvmap, cvid, newcv);
    }
    else {
        free(newcv);
        ret = -1;
    }
    return ret;
}

int PthreadMonitor_WaitOnCondition( PthreadMonitorData_t* pmond, int cvid )
{
    int ret = -1;

    pthread_cond_t* cv = get_val(pmond->cvmap, cvid);

    /* Assert if bad condition variable id */
    assert( cv != NULL );
    ret = pthread_cond_wait( cv, &(pmond->mutex) );

    return ret;
}

int PthreadMonitor_SignalCondition( PthreadMonitorData_t* pmond, int cvid )
{
    int ret = -1;

    pthread_cond_t* cv = get_val(pmond->cvmap, cvid);
    
    /* Assert if bad condition variable id */
    assert( cv != NULL );
    ret = pthread_cond_signal( cv );
    
    return ret;
}

int PthreadMonitor_BroadcastCondition( PthreadMonitorData_t* pmond, int cvid )
{
    int ret = -1;

    pthread_cond_t* cv = get_val(pmond->cvmap, cvid);
    
    /* Assert if bad condition variable id */
    assert( cv != NULL );
    ret = pthread_cond_broadcast( cv );
    
    return ret;
}
