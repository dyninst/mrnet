/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Mutex.h,v 1.5 2008/10/09 19:54:07 mjbrim Exp $
#ifndef XPLAT_MUTEX_H
#define XPLAT_MUTEX_H

struct Mutex_t
{
    void* data;
};
typedef struct Mutex_t Mutex_t;

Mutex_t* Mutex_construct( void );
int Mutex_destruct( Mutex_t* mut );

int Mutex_Lock( Mutex_t* mut );
int Mutex_Unlock( Mutex_t* mut );
int Mutex_Trylock( Mutex_t* mut );

#endif // XPLAT_MUTEX_H
