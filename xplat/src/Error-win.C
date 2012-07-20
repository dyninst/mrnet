/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Error-win.C,v 1.5 2007/01/24 19:33:59 darnold Exp $
#include <winsock2.h>
#include "xplat/Error.h"

namespace XPlat
{

std::string
Error::GetErrorString( int code )
{
    char msgbuf[1024];
    DWORD dwErr = (DWORD)code;

    DWORD dwRet = FormatMessage( 
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dwErr,
        MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
        (LPTSTR)msgbuf,
        1024 / sizeof(TCHAR),
        NULL );

    return ((dwRet != 0) ? msgbuf : "");
}


bool
Error::ETimedOut( int code )
{
    return (code == WSAETIMEDOUT);
}


bool
Error::EAddrInUse( int code )
{
    return (code == WSAEADDRINUSE);
}

bool
Error::EConnRefused( int code )
{
    return (code == WSAECONNREFUSED);
}

bool
Error::EInProgress( int code )
{
    return (code == WSAEINPROGRESS);
}

bool
Error::EInterrupted( int code )
{
    return (code == WSAEINTR);
}

} // namespace XPlat

