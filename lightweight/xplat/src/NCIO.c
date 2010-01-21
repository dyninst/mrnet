#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "xplat/NCIO.h"

NCBuf_t* new_NCBuf_t()
{
  NCBuf_t* new_NCBuf = (NCBuf_t*)malloc(sizeof(NCBuf_t));
  assert(new_NCBuf);

  return new_NCBuf;
}
