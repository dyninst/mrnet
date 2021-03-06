/****************************************************************************
 * Copyright � 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: SharedObject-win.C,v 1.3 2007/01/24 19:34:24 darnold Exp $
#include <cstdlib>
#include "SharedObject-win.h"

namespace XPlat
{

WinSharedObject::WinSharedObject( const char* path )
  : SharedObject( path ),
    handle( NULL )
{
    // try to load the shared object
    handle = LoadLibrary( path );
}

SharedObject*
SharedObject::Load( const char* path )
{
    WinSharedObject* ret = new WinSharedObject( path );
    if( ret->GetHandle() == NULL )
    {
        // we failed to open the given library
        delete ret;
        ret = NULL;
    }

    return ret;
}

const char*
SharedObject::GetErrorString( void )
{
    // we use a static buffer because on UNIX dlerror() 
    // does not allocate memory for the returned string
    static char msgbuf[1024];
    DWORD dwErr = GetLastError();

    DWORD dwRet = FormatMessage( 
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dwErr,
        MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
        (LPTSTR)msgbuf,
        1024 / sizeof(TCHAR),
        NULL );

    return ((dwRet != 0) ? msgbuf : NULL);
}

} // namespace XPlat

