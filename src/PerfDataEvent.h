/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined( __performance_data_events_h )
#define __performance_data_events_h 1

#include <map>
#include <string>
#include <vector>

#include "utils.h"
#include "mrnet/Packet.h"

namespace MRN {

class PerfDataMgr {

 private:
    static perfdata_metinfo_t perfdata_metric_info[PERFDATA_MAX_MET]; 
    static const char* perfdata_context_names[PERFDATA_MAX_CTX];

 public:
    PerfDataMgr(void);
    ~PerfDataMgr(void);

    void enable( perfdata_metric_t, perfdata_context_t );
    void disable( perfdata_metric_t, perfdata_context_t );
    bool is_Enabled( perfdata_metric_t, perfdata_context_t );

    static std::string get_MetricName( perfdata_metric_t );
    static std::string get_MetricUnits( perfdata_metric_t );
    static std::string get_MetricDescription( perfdata_metric_t );
    static perfdata_mettype_t get_MetricType( perfdata_metric_t );

    void collect( perfdata_metric_t, perfdata_context_t, std::vector<perfdata_t>& );
    void print( perfdata_metric_t, perfdata_context_t );

    void add_DataInstance( perfdata_metric_t, perfdata_context_t, perfdata_t );
    std::vector< perfdata_t > get_DataSeries( perfdata_metric_t, perfdata_context_t );
    
    void set_DataValue( perfdata_metric_t, perfdata_context_t, perfdata_t );
    perfdata_t get_DataValue( perfdata_metric_t, perfdata_context_t );
    void get_MemData(perfdata_metric_t);

    void add_PacketTimers ( PacketPtr pkt);
    
 private:
    // bitfield of enabled metrics per context
    char active_metrics[PERFDATA_MAX_CTX];

    // peformance data vector per metric, per context
    std::vector< std::map< perfdata_metric_t, std::vector<perfdata_t> > > the_data;
    mutable XPlat::Mutex _data_sync;
};

} /* namespace MRN */

#endif /* __performance_data_events_h */
