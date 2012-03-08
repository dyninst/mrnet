/****************************************************************************
 * Copyright © 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Mutex-win.C,v 1.5 2008/10/09 19:54:13 mjbrim Exp $
#include "Mutex-win.h"

namespace XPlat
{

Mutex::Mutex( void )
{
    static HANDLE initMutex = CreateMutex( NULL, FALSE, NULL );
    assert(initMutex != NULL);
    DWORD ret = WaitForSingleObject( initMutex, INFINITE );
    assert(ret != WAIT_FAILED);

    data = new WinMutexData;
    HANDLE c = CreateMutex( NULL, FALSE, NULL );
    assert(c != NULL);

    cleanup_initialized = true;
    cleanup = (void *)c;

    ReleaseMutex(initMutex);
}

Mutex::~Mutex( void )
{
    DWORD ret;

    ret = WaitForSingleObject((HANDLE)cleanup, INFINITE);

    // Make sure no one destroys the cleanup twice
    if(cleanup == NULL) {
        return;
    }
    assert(!ret);

    if( data != NULL ) {
        delete data;
        data = NULL;
    }

    cleanup_initialized = false;

    ret = ReleaseMutex((HANDLE)cleanup);
    assert(!ret);

    ret = CloseHandle((HANDLE)cleanup);
    assert(!ret);
    cleanup = NULL;
}

int Mutex::Lock( void )
{
    DWORD ret;
    if( cleanup_initialized && (cleanup != NULL) ) {
        ret = WaitForSingleObject((HANDLE)cleanup);
        if(ret)
            return ret;

        if( data != NULL ) {
            ret = data->Lock();
            assert(ReleaseMutex((HANDLE)cleanup));
            return ret;
        }

        assert(ReleaseMutex((HANDLE)cleanup));
        return -1;
    }
        
    return -1;
}

int Mutex::Unlock( void )
{
    DWORD ret;
    if( cleanup_initialized && (cleanup != NULL) ) {
        ret = WaitForSingleObject((HANDLE)cleanup);
        if(ret)
            return ret;

        if( data != NULL ) {
            ret = data->Unlock();
            assert(ReleaseMutex((HANDLE)cleanup));
            return ret;
        }

        assert(ReleaseMutex((HANDLE)cleanup));
        return 0;
    }
        
    return 0;
}

} // namespace XPlat
