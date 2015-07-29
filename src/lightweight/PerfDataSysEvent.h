/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__perfdatasysevent_h)
#define __perfdatasysevent_h 1

#include "utils_lightweight.h"

int PerfDataSysMgr_get_ThreadTime(long* user, long* sys);
int PerfDataSysMgr_get_MemUsage(double* vsize, double* psize);

#endif /* __perfdatasysevent_h */
