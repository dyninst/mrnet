/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Mutex-win.h,v 1.0 2012/02/02 13:00:32 samanas Exp $
#ifndef XPLAT_WINMUTEX_H
#define XPLAT_WINMUTEX_H

#include <windows.h>
#include <assert.h>
#include "xplat_lightweight/Mutex.h"

typedef struct WinMutexData {
    CRITICAL_SECTION mutex;
} WinMutexData_t;

WinMutexData_t* WinMutexData_construct( void )
{
    WinMutexData_t *wmd = NULL;
    wmd = (WinMutexData_t *)malloc(sizeof(WinMutexData_t));
    assert(wmd != NULL);

    InitializeCriticalSection( &wmd->mutex );

    return wmd;
}

int WinMutexData_destruct( WinMutexData_t *wmd )
{
    if(wmd != NULL) {
        DeleteCriticalSection( &wmd->mutex );
        return 0;
    }

    return -1;
}

int WinMutex_Lock( WinMutexData_t *wmd )
{
    EnterCriticalSection( &wmd->mutex );
    return 0;
}

int WinMutex_Unlock( WinMutexData_t *wmd )
{
    LeaveCriticalSection( &wmd->mutex );
    return 0;
}

int WinMutex_Trylock( WinMutexData_t *wmd )
{
    BOOL rc;
    rc = TryEnterCriticalSection( &wmd->mutex );

    /* Windows returns non-zero on a successful entry into the critical section.
     * This is opposite of the equivalent pthread function, so we invert */
    if(rc != 0) {
        rc = 0;
    } else {
        rc = 1;
    }
    
    return rc;
}

#endif // XPLAT_WINMUTEX_H
