/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Thread-pthread.C,v 1.6 2008/10/09 19:53:54 mjbrim Exp $
#include <pthread.h>
#include "xplat_lightweight/Thread.h"

#include <signal.h>
#include <stdlib.h>
#include <errno.h>

typedef struct 
{
    Thread_Func func;
    void* data;
} sigblock_wrapper_data_t;
    
static void* 
sigblock_wrapper_func( void *idata )
{
    sigblock_wrapper_data_t *wrapper_data = (sigblock_wrapper_data_t *) idata;
    Thread_Func func = wrapper_data->func;
    void* data = wrapper_data->data;
    free(wrapper_data);
    wrapper_data = 0;
    
    // Block all signals execpt for machine faults and other synchronous exceptions
    sigset_t block_set;
    sigfillset( &block_set );
    sigdelset( &block_set, SIGTRAP );
    sigdelset( &block_set, SIGSEGV );
    sigdelset( &block_set, SIGBUS );
    sigdelset( &block_set, SIGILL );
    sigdelset( &block_set, SIGSYS );
    sigdelset( &block_set, SIGPROF );
    sigdelset( &block_set, SIGTERM );
    pthread_sigmask( SIG_BLOCK, &block_set, 0 );

    return (*func)( data );
}

int
Thread_Create( Thread_Func ifunc, void* idata, Thread_Id* id )
{
    sigblock_wrapper_data_t* args = (sigblock_wrapper_data_t *)
                                    malloc(sizeof(sigblock_wrapper_data_t));
    args->func = ifunc;
    args->data = idata;
    return pthread_create( (pthread_t*)id, NULL, sigblock_wrapper_func, (void*)args );
}


Thread_Id
Thread_GetId( void )
{
    return (Thread_Id) pthread_self();
}


int
Thread_Join( Thread_Id joinWith, void** exitValue )
{
    pthread_t jwith;

    if(joinWith == 0) {
        return EINVAL;
    }

    jwith = (pthread_t) joinWith;
    return pthread_join( jwith, exitValue );
}

int
Thread_Cancel( Thread_Id iid )
{
    pthread_t id = (pthread_t) iid;
    return pthread_cancel( id );
}

void
Thread_Exit( void* ival )
{
    pthread_exit( ival );
}
