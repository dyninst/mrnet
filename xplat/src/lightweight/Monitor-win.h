/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Monitor-win.h,v 1.4 2007/01/24 19:34:03 darnold Exp $
//
// Declaration of the WinMonitor class.
// A WinMonitor is a Win32-based Monitor.
// 
// Note: the implementation for condition variables on Win32 used by
// this class is based mainly on an approach detailed in "Strategies for 
// Implementing POSIX Condition Variables on Win32" by Douglas C. Schmidt 
// and Irfan Pyarali, C++ Report, SIGS, Vol 10., No. 5, June 1998.
//
#ifndef XPLAT_WINMONITOR_H
#define XPLAT_WINMONITOR_H

#include <windows.h>
#include "xplat_lightweight/map.h"
#include "xplat_lightweight/Monitor.h"

typedef struct WinConditionVariable {
    HANDLE hMutex;                      // owning monitor's mutex
    HANDLE hWaitSemaphore;              // semaphore on which threads wait
    HANDLE hAllWaitersReleasedEvent;    // event on which broadcaster waits
                                        // till all waiters have been 
                                        // released and run
    int waitReleasedByBroadcast;        // indicates whether signal or
                                        // broadcast was used to 
                                        // release waiters

    unsigned int nWaiters;              // waiter count
    CRITICAL_SECTION nWaitersMutex;     // protects access to waiter count
} WinConditionVariable_t;

typedef struct WinMonitorData {
    HANDLE hMutex;              // handle to mutex object controlling 
                                // access to critical section
    mrn_map_t* cvmap;           // map of condition variables, indexed
                                // by user-chosen id
    int initialized;
} WinMonitorData_t;

WinMonitorData_t* WinMonitorData_construct( void );
int WinMonitorData_destruct( WinMonitorData_t *wmd );

// methods dealing with the mutex
int WinMonitor_Lock(WinMonitorData_t *wmd);
int WinMonitor_Unlock(WinMonitorData_t *wmd);
int WinMonitor_Trylock(WinMonitorData_t *wmd);

// methods dealing with condition variables
int WinMonitor_RegisterCondition( WinMonitorData_t *wmd, int cvid );
int WinMonitor_WaitOnCondition( WinMonitorData_t *wmd, int cvid );
int WinMonitor_SignalCondition( WinMonitorData_t *wmd, int cvid );
int WinMonitor_BroadcastCondition( WinMonitorData_t *wmd, int cvid );

WinConditionVariable_t* WinConditionVariable_construct( HANDLE _hMutex );
int WinConditionVariable_destruct( WinConditionVariable_t *win_cv );
int WinConditionVariable_Init( WinConditionVariable_t *win_cv );
int WinConditionVariable_Wait( WinConditionVariable_t *win_cv );
int WinConditionVariable_Signal( WinConditionVariable_t *win_cv );
int WinConditionVariable_Broadcast( WinConditionVariable_t *win_cv );

#endif // XPLAT_WINMONITOR_H
