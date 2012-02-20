/****************************************************************************
 * Copyright © 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined __netutils_h
#define __netutils_h 1

#if !defined(os_windows)
#include <netinet/in.h>
#else
#include <winsock2.h>
#define strtok_r strtok_s
#endif

#ifndef XPLAT_MAX_HOSTNAME_LEN
#define XPLAT_MAX_HOSTNAME_LEN 256
#endif

int XPlat_NetUtils_FindNetworkName(char* ihostname, char* ohostname);

int XPlat_NetUtils_GetHostName(char* ihostname, char* ohostname);

int XPlat_NetUtils_GetLocalHostName(char* this_host);

int XPlat_NetUtils_GetLastError();

#endif /* __netutils_h */
