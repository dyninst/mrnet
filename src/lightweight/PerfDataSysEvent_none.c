/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "PerfDataEvent.h"
#include "PerfDataSysEvent.h"

int PerfDataSysMgr_get_ThreadTime(long* user, long* sys)
{
    *user = 0;
    *sys = 0;
    return 0;
}

int PerfDataSysMgr_get_MemUsage(double* vsize, double* psize) 
{
    *vsize = 0;
    *psize = 0;
    return 0;
}
