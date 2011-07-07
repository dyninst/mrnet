/****************************************************************************
 * Copyright © 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "xplat_lightweight/NetUtils.h"

int NetUtils_GetLastError()
{
  return errno;
}

int NetUtils_GetLocalHostName(char* this_host) 
{
#if defined(arch_crayxt)
    // on the XT, the node number is available in /proc/cray_xt/nid
    const char* filename = "/proc/cray_xt/nid";
    FILE* f;
    f = fopen(filename, "r");
    uint32_t nid;
    fread(&nid, sizeof(uint32_t), 1, f);
    fclose(f);

    // move nid into string
    char* nid_str = (char*)malloc(sizeof(char)*6);
    assert(nid_str);
    sprintf(nid_str, "%05d", nid);

    char* nidStr = (char*)malloc(sizeof(char)*9);
    assert(nidStr);
    strcat(nidStr, "nid");
    strcat(nidStr, nid_str);

    strncpy(this_host, nidStr, XPLAT_MAX_HOSTNAME_LEN);
#else
    char host[XPLAT_MAX_HOSTNAME_LEN];
    gethostname(host, (size_t)XPLAT_MAX_HOSTNAME_LEN);
    host[XPLAT_MAX_HOSTNAME_LEN-1] = '\0';
    strncpy(this_host, host, (size_t)XPLAT_MAX_HOSTNAME_LEN);
#endif

  return 0;
}
