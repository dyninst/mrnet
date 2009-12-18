#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <winsock2.h>

#include "xplat/Error.h"

char* Error_GetErrorString(int code)
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

int Error_ETimedOut(int code)
{
    return (code == WSAETIMEDOUT);
}

int Error_EAddrInUse(int code)
{
    return (code == WSAEADDERINUSE);
}

int Error_EConnRefused(int code)
{
    return (code == WSAECONNREFUSED);
}
