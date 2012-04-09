/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <libgen.h>

#include "xplat_lightweight/PathUtils.h"

char* GetFilename(const char* path)
{
  // basename() may modify the path, and may return static memory
  char* pathCopy = strdup( path );
  assert(pathCopy != NULL);
  char* pathBase = basename( pathCopy );
  char* ret = strdup( pathBase );
  assert(ret != NULL);
  free( pathCopy );
  return ret;
}
