/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "xplat_lightweight/NetUtils.h"
#include "xplat_lightweight/Types.h"

int XPlat_NetUtils_GetLastError()
{
    return errno;
}

int XPlat_NetUtils_GetLocalHostName(char* this_host) 
{
#if defined(arch_crayxt)
    // on the XT, the node number is available in /proc/cray_xt/nid
    char host[16];
    FILE* f;
    const char* filename = "/proc/cray_xt/nid";
    unsigned nid;
    int rc = 0;
    f = fopen(filename, "r");
    if( NULL != f ) {
        if( 1 == fscanf(f, "%u", &nid) ) {
            sprintf( host, "nid%05u", nid );
            strcpy( this_host, host );
        }
        else rc = 1;
        fclose(f);
    }
    else rc = 1;
    return rc;
#else
    char host[XPLAT_MAX_HOSTNAME_LEN];
    gethostname(host, (size_t)XPLAT_MAX_HOSTNAME_LEN);
    host[XPLAT_MAX_HOSTNAME_LEN-1] = '\0';
    strncpy(this_host, host, (size_t)XPLAT_MAX_HOSTNAME_LEN);
    return 0;
#endif
}
