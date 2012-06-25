/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Monitor-win.c,v 1.6 2012/02/02 12:51:10 samanas Exp $
//
// Implementation of the WinMonitor class.
// A WinMonitor is a Win32-based Monitor.
// 
// Note: the implementation for condition variables on Win32 used by
// this class is based mainly on an approach detailed in "Strategies for 
// Implementing POSIX Condition Variables on Win32" by Douglas C. Schmidt 
// and Irfan Pyarali, C++ Report, SIGS, Vol 10., No. 5, June 1998.
//

#include <assert.h>
#include <limits.h>
#include "Monitor-win.h"

Monitor_t* Monitor_construct( void )
{
    static CRITICAL_SECTION init_mutex;
    Monitor_t *mon;
 
    InitializeCriticalSection(&init_mutex);

    EnterCriticalSection(&init_mutex);

    /* Construct data */
    mon = (Monitor_t *)malloc(sizeof(Monitor_t));
    assert(mon != NULL);
    mon->data = (void *)WinMonitorData_construct();
    assert(mon->data != NULL);

    LeaveCriticalSection(&init_mutex);

    return mon;
}

int Monitor_destruct( Monitor_t* m )
{
    static CRITICAL_SECTION cleanup_mutex;
    int rc = 0;

    InitializeCriticalSection(&cleanup_mutex);

    EnterCriticalSection(&cleanup_mutex);

    if((m != NULL) && (m->data != NULL)) {
        rc = WinMonitorData_destruct(m->data);
        free(m->data);
        m->data = NULL;
    } else {
        rc = -1;
    }

    return rc;
}

int Monitor_Lock( Monitor_t* m )
{
    if( (m != NULL) && (m->data != NULL) )
        return WinMonitor_Lock(m->data);

    return WSAEINVAL;
}

int Monitor_Unlock( Monitor_t* m )
{
    if( (m != NULL) && (m->data != NULL) )
        return WinMonitor_Unlock(m->data);

    return WSAEINVAL;
}

int Monitor_Trylock( Monitor_t* m )
{
    if( (m != NULL) && (m->data != NULL) )
        return WinMonitor_Trylock(m->data);

    return WSAEINVAL;
}

int Monitor_RegisterCondition( Monitor_t *m, int condid ) 
{
    if( (m != NULL) && (m->data != NULL) )
        return WinMonitor_RegisterCondition( m->data, condid );

    return WSAEINVAL;
}

int Monitor_WaitOnCondition( Monitor_t *m, int condid )
{
    if( (m != NULL) && (m->data != NULL) )
        return WinMonitor_WaitOnCondition( m->data, condid );

    return WSAEINVAL;
}

int Monitor_SignalCondition( Monitor_t *m, int condid )
{
    if( (m != NULL) && (m->data != NULL) )
        return WinMonitor_SignalCondition( m->data, condid );

    return WSAEINVAL;
}

int Monitor_BroadcastCondition( Monitor_t *m, int condid )
{
    if( (m != NULL) && (m->data != NULL) )
        return WinMonitor_BroadcastCondition( m->data, condid );

    return WSAEINVAL;
}

WinMonitorData_t* WinMonitorData_construct( void )
{
    WinMonitorData_t *wmd = (WinMonitorData_t *)malloc(sizeof(WinMonitorData_t));
    if(wmd != NULL) {
        wmd->hMutex = CreateMutex( NULL, FALSE, NULL );
        assert(wmd->hMutex != NULL);

        wmd->initialized = 1;
        wmd->cvmap = new_map_t();
    }

    return wmd;
}

int WinMonitorData_destruct( WinMonitorData_t *wmd )
{
    int i, temp_key, rc = 0;
    if((wmd != NULL) && wmd->initialized) {
        for(i = 0; i < (int)wmd->cvmap->size; i++) {
            temp_key = wmd->cvmap->keys[i];

            rc = WinConditionVariable_destruct((WinConditionVariable_t *)
                                                get_val(wmd->cvmap, temp_key));
            if(rc) {
                return rc;
            }
        }
        delete_map_t(wmd->cvmap);

        /* Destroy mutex */
        wmd->initialized = 0;
        if( (wmd->hMutex == NULL) || CloseHandle( wmd->hMutex ) == 0) {
            return -1;
        }
        return 0;
    }

    return -1;
}

int WinMonitor_Lock( WinMonitorData_t *wmd )
{
    DWORD dwRet;
    assert( (wmd != NULL) && (wmd->hMutex != NULL) );
    
    dwRet = WaitForSingleObject( wmd->hMutex, INFINITE );

    return ((dwRet == WAIT_OBJECT_0) ? 0 : -1);
}

int WinMonitor_Unlock( WinMonitorData_t *wmd )
{
    assert( (wmd != NULL) && (wmd->hMutex != NULL) );

    if(ReleaseMutex( wmd->hMutex ) == 0) {
        return -1;
    }

    return 0;

}

int WinMonitor_Trylock( WinMonitorData_t *wmd )
{
    int rc;
    DWORD dwRet;
    assert( (wmd != NULL) && (wmd->hMutex != NULL) );

    dwRet = WaitForSingleObject( wmd->hMutex, 0 );

    if(dwRet == WAIT_TIMEOUT) {
        rc = 1;
    } else if(dwRet == WAIT_OBJECT_0) {
        rc = 0;
    } else {
        rc = -1;
    }

    return rc;
}

int WinMonitor_RegisterCondition( WinMonitorData_t *wmd, int cvid )
{
    int ret = -1;

    assert( wmd->hMutex != NULL );

    /* check if there is already a condition associated with the chosen id */
    if( get_val(wmd->cvmap, cvid) == NULL )
    {
        /* there was no condition already associated with this condition var
           build one */
        WinConditionVariable_t *newcv = WinConditionVariable_construct( wmd->hMutex );

        /* initialize the condition variable */
        if( WinConditionVariable_Init(newcv) == 0 )
        {
            insert(wmd->cvmap, cvid, (void *)newcv);
            ret = 0;
        }
        else
        {
            /* failed to initialize - release the object */
            WinConditionVariable_destruct(newcv);
        }
    }
    return ret;
}


int WinMonitor_WaitOnCondition( WinMonitorData_t *wmd, int cvid )
{
    int ret = -1;
    WinConditionVariable_t *cv;

    assert( (wmd != NULL) && (wmd->hMutex != NULL) );

    cv = (WinConditionVariable_t *)get_val(wmd->cvmap, cvid);
    if( cv != NULL )
    {
        ret = WinConditionVariable_Wait(cv);
    }
    else
    {
        // bad cvid
        // TODO how to indicate the error?
	    assert(0);
    }
    return ret;
}


int WinMonitor_SignalCondition( WinMonitorData_t *wmd, int cvid )
{
    int ret = -1;
    WinConditionVariable_t *cv;

    assert( (wmd != NULL) && (wmd->hMutex != NULL) );
 
    cv = (WinConditionVariable_t *)get_val(wmd->cvmap, cvid);
    if( cv != NULL )
    {
        ret = WinConditionVariable_Signal(cv);
    }
    else
    {
        // bad cvid
        // TODO how to indicate the error?
    }
    return ret;
}


int WinMonitor_BroadcastCondition( WinMonitorData_t *wmd, int cvid )
{
    int ret = -1;
    WinConditionVariable_t *cv;

    assert( (wmd != NULL) && (wmd->hMutex != NULL) );
 
    cv = (WinConditionVariable_t *)get_val(wmd->cvmap, cvid);
    if( cv != NULL )
    {
        ret = WinConditionVariable_Broadcast(cv);
    }
    else
    {
        // bad cvid
        // TODO how to indicate the error?
    }
    return ret;
}


//----------------------------------------------------------------------------
// WinMonitorData::ConditionVariable methods
//----------------------------------------------------------------------------

WinConditionVariable_t* WinConditionVariable_construct(HANDLE _hMutex)
{
    WinConditionVariable_t *win_cv = (WinConditionVariable_t *)
                                        malloc(sizeof(WinConditionVariable_t));
    assert(_hMutex != NULL);
    assert(win_cv != NULL);

    win_cv->hMutex = _hMutex;
    win_cv->hWaitSemaphore = NULL;
    win_cv->hAllWaitersReleasedEvent = NULL;
    win_cv->waitReleasedByBroadcast = 0;
    win_cv->nWaiters = 0;

    return win_cv;
}

int WinConditionVariable_destruct( WinConditionVariable_t *win_cv )
{
    int rc = 0;
    assert(win_cv != NULL);

    if( win_cv->hWaitSemaphore != NULL )
    {
        /* CloseHandle returns 0 on error */
        if(CloseHandle( win_cv->hWaitSemaphore ) == 0) {
            rc += 1;
        }
    } else {
        rc += 2;
    }

    if( win_cv->hAllWaitersReleasedEvent != NULL )
    {
        if(CloseHandle( win_cv->hAllWaitersReleasedEvent ) == 0) {
            rc += 4;
        }
    } else {
        rc += 8;
    }

    DeleteCriticalSection( &win_cv->nWaitersMutex );

    free(win_cv);

    /* If rc == 0, destruction was successful */
    return rc;
}


int WinConditionVariable_Init( WinConditionVariable_t *win_cv )
{
    // We use two Win32 synchronization objects to implement condition 
    // variables, as modelled by the "Strategies..." article cited at the top
    // of this file.  We use a Semaphore on which threads can wait for the
    // condition to be signalled/broadcast.  For fairness, we also use an 
    // Event so that the waiters, once released, can indicate to the 
    // broadcaster that they have all been released. 

    assert(win_cv != NULL);
    // Semaphore on which threads wait for the condition to be signalled.
    //
    win_cv->hWaitSemaphore = CreateSemaphore( NULL, // default security attrs
                                              0,          // initial count
                                              LONG_MAX,   // max count
                                              NULL );     // no name needed


    // The all-waiters released event is used in case a thread uses a broadcast
    // to release all waiting threads.  After the broadcaster releases all
    // the waiting threads, it blocks itself on this event till the last
    // waiter is known to have been awakened.
    //
    win_cv->hAllWaitersReleasedEvent = CreateEvent( NULL,   // default security attrs
                                                    FALSE,  // make auto-reset event
                                                    FALSE,  // initially non-signalled
                                                    NULL ); // no name needed

    InitializeCriticalSection( &win_cv->nWaitersMutex );

    return (((win_cv->hWaitSemaphore != NULL) &&
                (win_cv->hAllWaitersReleasedEvent != NULL)) ? 0 : -1);
}



int WinConditionVariable_Wait( WinConditionVariable_t *win_cv )
{
    int ret = -1;
    DWORD dwret;        // used for return value from Win32 calls


    assert(win_cv != NULL);

    // We're going to be waiting, so bump the number of waiters.
    // Even though we currently have our owning Monitor's mutex locked,
    // we need to protect access to this variable because our access to 
    // this variable later in this file occur when we do *not* have it locked.
    EnterCriticalSection( &win_cv->nWaitersMutex );
    win_cv->nWaiters++;
    LeaveCriticalSection( &win_cv->nWaitersMutex );

    // Atomically release our owning object's mutex and 
    // wait on our semaphore.  ("Signalling" a Win32 Mutex releases it.)
    dwret = SignalObjectAndWait( win_cv->hMutex,            // obj to signal
                                 win_cv->hWaitSemaphore, // obj to wait on
                                 INFINITE,   // timeout
                                 FALSE );    // alertable?
    if( dwret == WAIT_OBJECT_0 )
    {
        int signalBroadcaster;
        
        // We have been released from the semaphore.
        // At this point, we do *not* have the critical section mutex.

        EnterCriticalSection( &win_cv->nWaitersMutex );
        win_cv->nWaiters--;

        // check if we were released via a broadcast and we are
        // the last thread to be released
        signalBroadcaster = (win_cv->waitReleasedByBroadcast &&
                                (win_cv->nWaiters == 0));
        LeaveCriticalSection( &win_cv->nWaitersMutex );

        if( signalBroadcaster )
        {
            // We were unblocked by a broadcast and we were the last
            // waiter to run.  We need to:
            //
            //   1. Signal the broadcaster that it should continue (and
            //      release the owning Monitor's mutex lock)
            //   2. Try to acquire the owning Monitor's mutex lock.
            //
            // We need to do both in the same atomic operation.
            dwret = SignalObjectAndWait( win_cv->hAllWaitersReleasedEvent,  // obj to signal
                                         win_cv->hMutex, // obj to wait on
                                         INFINITE,       // timeout
                                         FALSE );        // alertable?
        }
        else
        {
            // We were unblocked by a signal operation, or we were not
            // the last waiter to run.  We just need to try to reacquire the
            // owning Monitor's mutex lock.
            dwret = WaitForSingleObject( win_cv->hMutex, INFINITE );
        }

        if( dwret == WAIT_OBJECT_0 )
        {
            ret = 0;
        }
        else
        {
            // the wait or signalandwait failed
            // TODO how to indicate this to the user?
        }
    }
    else
    {
        // SignalObjectAndWait failed
        // TODO how to indicate the failure?
    }

    return ret;
}



int WinConditionVariable_Signal( WinConditionVariable_t *win_cv )
{
    int ret = 0, anyWaiters;

    assert(win_cv != NULL);

    // Check if there are any waiters to signal.
    EnterCriticalSection( &win_cv->nWaitersMutex );
    anyWaiters = (win_cv->nWaiters > 0);
    LeaveCriticalSection( &win_cv->nWaitersMutex );

    // release one thread from the Semaphore if necessary
    if( anyWaiters )
    {
        if( !ReleaseSemaphore( win_cv->hWaitSemaphore,
                               1,              // release one thread
                               NULL ) )        // don't care about prev value
        {
            // we failed to release a thread
            // TODO how to report more complete error to user?
            ret = -1;
        }
    }
    return ret;
}


int WinConditionVariable_Broadcast( WinConditionVariable_t *win_cv )
{
    int ret = 0, haveWaiters;

    // Check if there are any waiters.
    EnterCriticalSection( &win_cv->nWaitersMutex );
    haveWaiters = (win_cv->nWaiters > 0);
    if( haveWaiters )
    {
        win_cv->waitReleasedByBroadcast = 1;
        
        // release all waiters atomically
        if( !ReleaseSemaphore( win_cv->hWaitSemaphore,
                               win_cv->nWaiters,       // number to release
                               NULL ) )        // don't care about prev val
        {
            // TODO how to give more error info to user?
            ret = -1;
        }
    }
    LeaveCriticalSection( &win_cv->nWaitersMutex );

    // Wait for last waiter to tell us it has been unblocked and run
    // (if there were any waiters to be released).
    if( haveWaiters && (ret == 0) )
    {
        DWORD dwret = WaitForSingleObject( win_cv->hAllWaitersReleasedEvent,
                                           INFINITE );
        if( dwret != WAIT_OBJECT_0 )
        {
            // TODO how to give more error info to user?
            ret = -1;
        }
        win_cv->waitReleasedByBroadcast = 0;
    }

    return ret;
}

