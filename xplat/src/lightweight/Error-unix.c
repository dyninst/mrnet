/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "xplat_lightweight/Error.h"

char* XPlat_Error_GetErrorString(int code)
{
  return strerror(code);
}

int XPlat_Error_ETimedOut(int code)
{
  return (code == ETIMEDOUT);
}

int XPlat_Error_EAddrInUse(int code)
{
  return (code == EADDRINUSE);
}

int XPlat_Error_EConnRefused(int code)
{
  return (code == ECONNREFUSED);
}
