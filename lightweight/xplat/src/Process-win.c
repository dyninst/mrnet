#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include "xplat/Process.h"

int Process_GetProcessId(void)
{
    return (int)GetCurrentProcessId();
}
