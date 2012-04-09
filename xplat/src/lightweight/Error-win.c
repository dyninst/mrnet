/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "xplat_lightweight/Error.h"
#include "xplat_lightweight/Types.h"

char* XPlat_Error_GetErrorString(int code)
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

int XPlat_Error_ETimedOut(int code)
{
    return (code == WSAETIMEDOUT);
}

int XPlat_Error_EAddrInUse(int code)
{
    return (code == WSAEADDRINUSE);
}

int XPlat_Error_EConnRefused(int code)
{
    return (code == WSAECONNREFUSED);
}
