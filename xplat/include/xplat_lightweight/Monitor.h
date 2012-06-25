/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Monitor.h,v 1.5 2008/10/09 19:54:06 mjbrim Exp $
#ifndef XPLAT_MONITOR_H
#define XPLAT_MONITOR_H

#if !defined(NULL)
#include <stdlib.h>
#endif // !defined(NULL)

struct Monitor_t
{
    void *data;
};
typedef struct Monitor_t Monitor_t;

Monitor_t* Monitor_construct( void );
int Monitor_destruct(Monitor_t* m );

int Monitor_Lock( Monitor_t* m );
int Monitor_Unlock( Monitor_t* m );
int Monitor_Trylock( Monitor_t* m );

int Monitor_RegisterCondition( Monitor_t* m, int condid );
int Monitor_WaitOnCondition( Monitor_t* m, int condid );
int Monitor_SignalCondition( Monitor_t* m, int condid );
int Monitor_BroadcastCondition( Monitor_t* m, int condid );

#endif // XPLAT_MONITOR_H
