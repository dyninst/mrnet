#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "xplat/NetUtils.h"

int NetUtils_GetLastError()
{
  return WSAGetLastError();
}

int NetUtils_GetLocalHostName(char* this_host) 
{
    char host[256];
    gethostname(host,256);
    host[255] = '\0';
    strncpy(this_host, host, 256);
  
    return 0;
}
