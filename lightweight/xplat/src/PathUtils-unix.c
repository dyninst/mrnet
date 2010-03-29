#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "xplat_lightweight/PathUtils.h"

char* GetFilename(const char* path)
{
  // basename() may modify the path, and may return static memory
  char* pathCopy = strdup( path );
  char* pathBase = basename( pathCopy );
  char* ret = strdup( pathBase );
  assert(ret);
  free( pathCopy );
  return ret;
}
