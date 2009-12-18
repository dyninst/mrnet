#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "xplat/NetUtils.h"

int GetLastError()
{
  return WSAGetLastError();
}

int GetLocalHostName(char* this_host) 
{
    char host[256];
    gethostname(host,256);
    host[255] = '\0';
    strncpy(this_host, host, 256);
  
    return 0;
}
