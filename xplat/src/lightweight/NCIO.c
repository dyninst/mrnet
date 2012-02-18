/****************************************************************************
 * Copyright © 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "xplat_lightweight/NCIO.h"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
NCBuf_t* new_NCBuf_t()
{
  NCBuf_t* new_NCBuf = (NCBuf_t*)malloc(sizeof(NCBuf_t));
  assert(new_NCBuf);
  new_NCBuf->buf = NULL;
  new_NCBuf->len = 0;
  return new_NCBuf;
}
