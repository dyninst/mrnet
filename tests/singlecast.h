/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined( singlecast_h )
#define singlecast_h 1

#include "mrnet/MRNet.h"

typedef enum {
    SC_EXIT=FirstApplicationTag,
    SC_SINGLE,
    SC_GROUP
} Protocol;

#endif /* singlecast_h */
