/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "mrnet/Network.h"
#include "Router.h"

Router_t* new_Router_t(Network_t* net)
{
  Router_t* router = (Router_t*)malloc(sizeof(Router_t));
  assert(router != NULL);
  router->net = net;

  return router;
}
