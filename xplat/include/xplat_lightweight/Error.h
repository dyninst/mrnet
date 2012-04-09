/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined __error_h
#define __error_h 1

char* XPlat_Error_GetErrorString(int code);

int XPlat_Error_ETimedOut(int code);

int XPlat_Error_EAddrInUse(int code);

int XPlat_Error_EConnRefused(int code);

#endif /* __error_h */
