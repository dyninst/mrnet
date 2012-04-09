/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined __netutils_h
#define __netutils_h 1

#include "xplat_lightweight/Types.h"

int XPlat_NetUtils_FindNetworkName(char* ihostname, char* ohostname);

int XPlat_NetUtils_GetHostName(char* ihostname, char* ohostname);

int XPlat_NetUtils_GetLocalHostName(char* this_host);

int XPlat_NetUtils_GetLastError();

#endif /* __netutils_h */
