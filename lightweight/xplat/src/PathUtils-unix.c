#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "xplat/PathUtils.h"

char* GetFilename(const char* path)
{
  //basename modifies the path
  int len = strlen(path)+1;
  char pathCopy[len];
  strncpy(pathCopy, path, len);

  char* ret = (char*)malloc(sizeof(char)*256);
  assert(ret);
  strncpy(ret, basename(pathCopy), 255);

  return ret;
}
