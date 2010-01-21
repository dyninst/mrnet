#include <windows.h>
#include <shlwapi.h>
#include "xplat/PathUtils.h"

char* GetFilename(const char* path)
{
    return PathFindFileName(path);
}
