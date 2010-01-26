/****************************************************************************
 * Copyright © 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <winsock2.h>

#include "xplat/Process.h"

int Process_GetProcessId(void)
{
    return (int)GetCurrentProcessId();
}
