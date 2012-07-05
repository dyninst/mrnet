/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Thread-pthread.C,v 1.6 2008/10/09 19:53:54 mjbrim Exp $
#include <pthread.h>
#include <cerrno>
#include "xplat/Thread.h"

#include <signal.h>

namespace XPlat
{

struct sigblock_wrapper_data_t
{
    Thread::Func func;
    void* data;
    
    sigblock_wrapper_data_t( Thread::Func ifunc, void* idata )
        : func(ifunc), data(idata)
    {}
};
    
static void* 
sigblock_wrapper_func( void *idata )
{
    sigblock_wrapper_data_t *wrapper_data = (sigblock_wrapper_data_t *) idata;
    Thread::Func func = wrapper_data->func;
    void* data = wrapper_data->data;
    delete wrapper_data;
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
Thread::Create( Func ifunc, void* idata, Id* id )
{
    sigblock_wrapper_data_t* args = new sigblock_wrapper_data_t( ifunc, idata );
    return pthread_create( (pthread_t*)id, NULL, sigblock_wrapper_func, (void*)args );
}


Thread::Id
Thread::GetId( void )
{
    return (Thread::Id) pthread_self();
}


int
Thread::Join( Thread::Id joinWith, void** exitValue )
{
    if (joinWith == 0) {
        return EINVAL;
    }
    pthread_t jwith = (pthread_t) joinWith;
    return pthread_join( jwith, exitValue );
}

int
Thread::Cancel( Thread::Id iid )
{
    pthread_t id = (pthread_t) iid;
    return pthread_cancel( id );
}

void
Thread::Exit( void* ival )
{
    pthread_exit( ival );
}

} // namespace XPlat

