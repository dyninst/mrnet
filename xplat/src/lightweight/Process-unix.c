/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include "xplat_lightweight/Process.h"

int Process_GetProcessId(void)
{
  static int local_pid = -1;
  if (local_pid == -1)
    local_pid = getpid();
  return local_pid;
}
