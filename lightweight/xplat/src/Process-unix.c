#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include "xplat/Process.h"

int Process_GetProcessId(void)
{
  static int local_pid = -1;
  if (local_pid == -1)
    local_pid = getpid();
  return local_pid;
}
