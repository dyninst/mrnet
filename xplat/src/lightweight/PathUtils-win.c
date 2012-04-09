/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <windows.h>
#include <shlwapi.h>
#include "xplat_lightweight/PathUtils.h"


char* GetFilename(const char* path)
{
    return PathFindFileName(path);
}
