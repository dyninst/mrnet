/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Thread-win.c,v 1.4 2012/02/01 16:33:10 samanas Exp $
#include <windows.h>
#include <assert.h>
#include "xplat_lightweight/Thread.h"

int Thread_Create( Thread_Func func, void* data, Thread_Id* id )
{
    DWORD ret_thread_id;
    HANDLE hThread;

    assert( id != NULL );

    // create the thread
    hThread = CreateThread( NULL,   // security attributes
                                    0,      // default stack size
                                    (LPTHREAD_START_ROUTINE)func,   // start routine
                                    data,   // arg to start routine
                                    0,      // creation flags
                                    &ret_thread_id ); // loc to store thread id (unused)
    if( &ret_thread_id != NULL )
    {
        *id = (Thread_Id)ret_thread_id;
    }
    return (hThread == NULL) ? -1 : 0;
}


Thread_Id Thread_GetId( void )
{
    return (Thread_Id)(GetCurrentThreadId());
}


int Thread_Join( Thread_Id joinWith, void** exitValue )
{
    int ret = -1;
    HANDLE cur_handle;

    if(joinWith == 0) {
        return ret;
    }

    // Get the thread handle from the given Id
    cur_handle = OpenThread(SYNCHRONIZE | THREAD_QUERY_INFORMATION,
                                   FALSE,
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

int Thread_Cancel( Thread_Id id )
{
    int ret;
    HANDLE cur_handle;

    // Get the thread handle from the give Id
    cur_handle = OpenThread(THREAD_TERMINATE, FALSE, (DWORD)id);

    if( TerminateThread( cur_handle, (DWORD)0) )
	    ret = 0;
    else {
	    return (int) GetLastError();
    }

    if(!CloseHandle(cur_handle)) {
        ret = (int) GetLastError();
    }

    return ret;
}


void Thread_Exit( void* val )
{
    ExitThread( (DWORD)val );
}

