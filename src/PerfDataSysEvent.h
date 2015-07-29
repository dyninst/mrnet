/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined( __performance_data_sys_events_h )
#define __performance_data_sys_events_h 1

#include "utils.h"

namespace MRN {

class PerfDataSysMgr {

 public:
 static int get_ThreadTime(long &user, long &sys);
 static int get_MemUsage (double &vsize, double &psize);

};

} /* namespace MRN */

#endif /* __performance_data_sys_events_h */
