/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
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

    LeaveCriticalSection(&init_mutex);

    return mut;
}

int Mutex_destruct( Mutex_t* m )
{
    static CRITICAL_SECTION cleanup_mutex;
    int rc = 0;

    InitializeCriticalSection(&cleanup_mutex);

    EnterCriticalSection(&cleanup_mutex);

    if((m != NULL) && (m->data != NULL)) {
        rc = WinMutexData_destruct(m->data);
        free(m->data);
        m->data = NULL;
    } else {
        rc = -1;
    }

    return rc;
}

int Mutex_Lock( Mutex_t *m )
{
    if(m->data != NULL)
        return WinMutex_Lock(m->data);

    return WSAEINVAL;
}

int Mutex_Unlock( Mutex_t *m )
{
    if(m->data != NULL)
        return WinMutex_Unlock(m->data);

    return WSAEINVAL;
}

int Mutex_Trylock( Mutex_t *m )
{
    if(m->data != NULL)
        return WinMutex_Trylock(m->data);

    return WSAEINVAL;
}
