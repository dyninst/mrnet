/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Thread.h,v 1.5 2008/10/09 19:53:53 mjbrim Exp $
#ifndef XPLAT_THREAD_H
#define XPLAT_THREAD_H

typedef void* (*Thread_Func)( void* );
typedef long Thread_Id;

int Thread_Create( Thread_Func func, void* data, Thread_Id* id );
Thread_Id Thread_GetId( void );
int Thread_Join( Thread_Id joinWith, void** exitValue );
int Thread_Cancel( Thread_Id id );
void Thread_Exit( void* val );

#endif // XPLAT_THREAD_H
