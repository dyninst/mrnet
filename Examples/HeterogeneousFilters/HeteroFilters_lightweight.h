/**************************************************************************
 * Copyright 2003-2015   Michael J. Brim, Barton P. Miller                *
 *                Detailed MRNet usage rights in "LICENSE" file.          *
 **************************************************************************/

#if !defined(__HETERO_FILTERS_LIGHTWEIGHT_H__)
#define __HETERO_FILTERS_LIGHTWEIGHT_H__

#include "mrnet_lightweight/Types.h"
#include "file_util_lightweight.h"

typedef enum {
    PROT_INIT = FirstApplicationTag,
    PROT_SEARCH,
    PROT_EXIT
} Protocol;

const char* BE_filter_scan = "HF_BE_scan";
const char* FE_filter_uniq = "HF_FE_uniq";
const char* CP_filter_sort = "HF_CP_sort";


#endif /* __HETERO_FILTERS_LIGHTWEIGHT_H__ */
