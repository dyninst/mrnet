/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <winsock2.h>
#include <Iphlpapi.h>
#include <string.h>

#include "xplat_lightweight/NetUtils.h"

int XPlat_NetUtils_GetLastError()
{
    return WSAGetLastError();
}

int XPlat_NetUtils_GetLocalHostName(char* this_host) 
{
    char host[256];
    gethostname(host,256);
    host[255] = '\0';
    strncpy(this_host, host, 256);

    return 0;
}
