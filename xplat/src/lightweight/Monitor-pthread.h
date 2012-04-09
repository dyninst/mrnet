/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Monitor-pthread.h,v 1.5 2007/01/24 19:34:01 darnold Exp $
#ifndef XPLAT_PTHREADMONITOR_H
#define XPLAT_PTHREADMONITOR_H

#ifndef __USE_UNIX98
#define __USE_UNIX98 1 // to get PTHREAD_MUTEX_RECURSIVE
#endif
#include <pthread.h>
#include "xplat_lightweight/Monitor.h"
#include "xplat_lightweight/map.h"

struct PthreadMonitorData
{
    pthread_mutex_t mutex;
    mrn_map_t *cvmap;
    int initialized;
};
typedef struct PthreadMonitorData PthreadMonitorData_t;

PthreadMonitorData_t* PthreadMonitorData_construct(void);
int PthreadMonitorData_destruct(PthreadMonitorData_t* pmond);

int PthreadMonitor_Lock(PthreadMonitorData_t* pmond);
int PthreadMonitor_Unlock(PthreadMonitorData_t* pmond);
int PthreadMonitor_Trylock( PthreadMonitorData_t* pmond );

int PthreadMonitor_RegisterCondition(PthreadMonitorData_t* pmond, int cvid);
int PthreadMonitor_WaitOnCondition(PthreadMonitorData_t* pmond, int cvid);
int PthreadMonitor_SignalCondition(PthreadMonitorData_t* pmond, int cvid);
int PthreadMonitor_BroadcastCondition(PthreadMonitorData_t* pmond, int cvid);

#endif // XPLAT_PTHREADMONITOR_H
