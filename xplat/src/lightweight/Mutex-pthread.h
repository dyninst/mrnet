/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Mutex-pthread.h,v 1.4 2008/10/09 19:54:12 mjbrim Exp $
#ifndef XPLAT_PTHREADMUTEX_H
#define XPLAT_PTHREADMUTEX_H

#include "xplat_lightweight/Types.h"

#ifndef __USE_UNIX98
#define __USE_UNIX98 1 // to get PTHREAD_MUTEX_ERRORCHECK
#endif
#include <pthread.h>

#include "xplat_lightweight/Mutex.h"

struct PthreadMutexData
{
    pthread_mutex_t mutex;
    int initialized;
};
typedef struct PthreadMutexData PthreadMutexData_t;

PthreadMutexData_t* PthreadMutexData_construct(void);
int PthreadMutexData_destruct( PthreadMutexData_t* pmd );

int PthreadMutex_Lock( PthreadMutexData_t* pmd );
int PthreadMutex_Unlock( PthreadMutexData_t* pmd );
int PthreadMutex_Trylock( PthreadMutexData_t* pmd );

#endif // XPLAT_PTHREADMUTEX_H
