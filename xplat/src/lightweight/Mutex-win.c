/****************************************************************************
 * Copyright © 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Mutex-win.C,v 1.0 2012/02/02 12:58:13 samanas Exp $
#include "Mutex-win.h"

Mutex_t* Mutex_construct( void )
{
    static CRITICAL_SECTION init_mutex;
    Mutex_t *mut;
    InitializeCriticalSection(&init_mutex);

    EnterCriticalSection(&init_mutex);

    /* Construct data */
    mut = (Mutex_t *)malloc(sizeof(Mutex_t));
    assert(mut != NULL);
    mut->data = (void *)WinMutexData_construct();
    assert(mut->data != NULL);

    /* Construct cleanup lock */
    c = CreateMutex(NULL,FALSE,NULL);
    assert(c != NULL);

    mut->cleanup_initialized = true;
    mut->cleanup = (void *)c;


    LeaveCriticalSection(&init_mutex);

    return mut;
}

int Mutex_destruct( Mutex_t* m )
{
    DWORD ret;
    int rc = 0;

    if(m == NULL)
        return -1;

    ret = WaitForSingleObject((HANDLE)m->cleanup, INFINITE);

    if(m->cleanup == NULL) {
        return;
    }

    if(m->data != NULL) {
        rc = WinMutexData_destruct(m->data);
        free(m->data);
        m->data = NULL;
    } else {
        rc = -1;
    }

    m->cleanup_initialized = 0;
    
    ret = ReleaseMutex((HANDLE)m->cleanup);
    assert(!ret);

    ret = CloseHandle((HANDLE)m->cleanup);
    assert(!ret);
    m->cleanup = NULL;

    LeaveCriticalSection(&cleanup_mutex);

    return rc;
}

int Mutex_Lock( Mutex_t *m )
{
    DWORD ret;
    if( (m != NULL) && m->cleanup_initialized && (m->cleanup != NULL) ) {
        ret = WaitForSingleObject((HANDLE)m->cleanup);
        if(ret)
            return ret;

        if( m->data != NULL ) {
            ret = WinMutex_Lock(m->data);
            assert(ReleaseMutex((HANDLE)m->cleanup));
            return ret;
        }

        assert(ReleaseMutex((HANDLE)m->cleanup));
        return WSAEINVAL;
    }

    return WSAEINVAL;
}

int Mutex_Unlock( Mutex_t *m )
{
    DWORD ret;
    if( (m != NULL) && m->cleanup_initialized && (m->cleanup != NULL) ) {
        ret = WaitForSingleObject((HANDLE)m->cleanup);

        if(ret)
            return ret;

        if( m->data != NULL ) {
            ret = WinMutex_Unlock(m->data);
            assert(ReleaseMutex((HANDLE)m->cleanup));
            return ret;
        }

        assert(ReleaseMutex((HANDLE)m->cleanup));
        return WSAEINVAL;
    }

    return WSAEINVAL;
}

int Mutex_Trylock( Mutex_t *m )
{
    DWORD ret;
    if( (m != NULL) && m->cleanup_initialized && (m->cleanup != NULL) ) {
        ret = WaitForSingleObject((HANDLE)m->cleanup);

        if(ret)
            return ret;

        if( m->data != NULL ) {
            ret = WinMutex_Trylock(m->data);
            assert(ReleaseMutex((HANDLE)m->cleanup));
            return ret;
        }

        assert(ReleaseMutex((HANDLE)m->cleanup));
        return WSAEINVAL;
    }

    return WSAEINVAL;
}
