#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "xplat/PathUtils.h"


char* GetFilename(const char* path)
{
    return PathFindFileName(path);
}
