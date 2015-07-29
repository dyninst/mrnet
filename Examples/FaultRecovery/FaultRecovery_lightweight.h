/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(fault_recovery_lightweight_h )
#define fault_recovery_lightweight_h 1

#include "mrnet_lightweight/Types.h"

#define fr_range_max 1000
#define fr_bins 20

typedef enum { PROT_EXIT=FirstApplicationTag, 
               PROT_START,
               PROT_WAVE,
               PROT_CHECK_MIN,
               PROT_CHECK_MAX,
               PROT_CHECK_PCT
             } Protocol;

#endif /* fault_recovery_lightweight_h */
