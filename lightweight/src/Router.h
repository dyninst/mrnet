/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__router_h)
#define __router_h 1

#include "mrnet/Network.h"

typedef struct {
  Network_t* net;
} Router_t;

Router_t* new_Router_t(Network_t* net);

#endif /* __router_h */
