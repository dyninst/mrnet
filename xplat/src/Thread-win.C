/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Thread-win.C,v 1.4 2008/10/09 19:53:55 mjbrim Exp $
#include <windows.h>
#include <cassert>
#include "xplat/Thread.h"

namespace XPlat
{

int
Thread::Create( Func func, void* data, Id* id )
{
    assert( id != NULL );
    DWORD ret_thread_id;

    // create the thread
    HANDLE hThread = CreateThread( NULL,   // security attributes
                                    0,      // default stack size
                                    (LPTHREAD_START_ROUTINE)func,   // start routine
                                    data,   // arg to start routine
                                    0,      // creation flags
                                    &ret_thread_id ); // loc to store thread id (unused)
    if( &ret_thread_id != NULL )
    {
        *id = (Thread::Id)ret_thread_id;
    }
    return (hThread == NULL) ? -1 : 0;
}


Thread::Id
Thread::GetId( void )
{
    return (Id)(GetCurrentThreadId());
}


int
Thread::Join( Id joinWith, void** exitValue )
{
    int ret = -1;
    HANDLE cur_handle;

    if(joinWith == 0) {
        return ret;
    }

    // Get the thread handle from the given Id
    cur_handle = OpenThread(SYNCHRONIZE | THREAD_QUERY_INFORMATION, FALSE,
                            (DWORD)joinWith);

    // wait for the indicated thread to exit
    //
    // TODO yes, we know this doesn't handle all possible return values
    // from WaitForSingleObject.
    //
    if( WaitForSingleObject( cur_handle, INFINITE ) == WAIT_OBJECT_0 )
    {
        if( exitValue != NULL )
        {
            // extract departed thread's exit code
            GetExitCodeThread( cur_handle, (LPDWORD)exitValue );
        }
        ret = 0;
    }

    if(!CloseHandle(cur_handle)) {
        return (int) GetLastError();
    }

    return ret;
}

int Thread::Cancel( Id id )
{
    int ret;
    HANDLE cur_handle;

    // Get the thread handle from the give Id
    cur_handle = OpenThread(THREAD_TERMINATE, FALSE, (DWORD)id);

    if( TerminateThread( cur_handle, (DWORD)0) )
	    ret = 0;
    else
	    ret = (int) GetLastError();

    if(!CloseHandle(cur_handle)) {
        ret = (int) GetLastError();
    }

    return ret;
}


void
Thread::Exit( void* val )
{
    ExitThread( (DWORD)val );
}


} // namespace XPlat

