/****************************************************************************
 * Copyright © 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined( microbench_lightweight_h )
#define microbench_lightweight_h 1

#include "mrnet/Types.h"
#include "mrnet/MRNet.h"

typedef enum {
    MB_EXIT=FirstApplicationTag,
    MB_ROUNDTRIP_LATENCY,
    MB_RED_THROUGHPUT
} Protocol;

#endif /* microbench_lightweight_h */
