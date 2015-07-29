
/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "PerfDataSysEvent.h"

namespace MRN {

int PerfDataSysMgr::get_ThreadTime(long &user, long &sys)
{
    user = 0;
    sys = 0;

    return 0;
}

int PerfDataSysMgr::get_MemUsage(double &vsize, double &psize)
{
    vsize = 0;
    psize = 0;

    return 0;
}

} /* namespace MRN */
