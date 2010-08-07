/****************************************************************************
 *  Copyright 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined __perfdataevent 
#define __perfdataevent 1

#include "mrnet_lightweight/Types.h"
#include "vector.h"

struct PerfDataMgr_t {
    // bitfield of enabled metrics per context
    char active_metrics[PERFDATA_MAX_CTX];
    vector_t* the_data;
} ;

typedef struct PerfDataMgr_t PerfDataMgr_t;

static perfdata_metinfo_t perfdata_metric_info[PERFDATA_MAX_MET] = 
{
    { "NumBytes", "bytes", "number of bytes", PERFDATA_TYPE_UINT },
    { "NumPackets", "packets", "number of packets", PERFDATA_TYPE_UINT },
    { "ElapsedTime", "seconds", "elapsed time", PERFDATA_TYPE_FLOAT },
    { "CPU-Sys", "%cpu", "system cpu utilization", PERFDATA_TYPE_FLOAT },
    { "CPU-User", "%cpu", "user cpu utilization", PERFDATA_TYPE_FLOAT },
    { "VirtMem", "kilobytes", "virtual memory size", PERFDATA_TYPE_FLOAT },
    { "PhysMem", "kilobytes", "resident memory size", PERFDATA_TYPE_FLOAT }
};

static const char* perfdata_context_names[PERFDATA_MAX_CTX] = 
{
    "Send",
    "Recv",
    "FilterIn",
    "FilterOut",
    "NoContext"
};

PerfDataMgr_t* new_PerfDataMgr_t();

void PerfDataMgr_enable(PerfDataMgr_t* perf_data,
                        perfdata_metric_t met,
                        perfdata_context_t ctx);

void PerfDataMgr_disable(PerfDataMgr_t* perf_data,
                         perfdata_metric_t met,
                         perfdata_context_t ctx);

int PerfDataMgr_is_Enabled(PerfDataMgr_t* perf_data, 
                           perfdata_metric_t met, 
                           perfdata_context_t ctx);

void PerfDataMgr_add_DataInstance(PerfDataMgr_t* perf_data,
                                  perfdata_metric_t met,
                                  perfdata_context_t ctx,
                                  perfdata_t data);

perfdata_t PerfDataMgr_get_DataValue(PerfDataMgr_t* perf_data, 
                                     perfdata_metric_t met,
                                     perfdata_context_t ctx);

void PerfDataMgr_set_DataValue(PerfDataMgr_t* perf_data,
                               perfdata_metric_t met,
                               perfdata_context_t ctx,
                               perfdata_t data);

void PerfDataMgr_get_MemData(PerfDataMgr_t* perf_data,
                             perfdata_metric_t metric);

void PerfDataMgr_print(PerfDataMgr_t* perf_data,
                       perfdata_metric_t met,
                       perfdata_context_t ctx);


void PerfDataMgr_collect(PerfDataMgr_t* perf_data,
                         perfdata_metric_t met,
                         perfdata_context_t ctx,
                         vector_t* data);


char * PerfDataMgr_get_MetricName(PerfDataMgr_t* perf_data, perfdata_metric_t met);
char * PerfDataMgr_get_MetricUnits(PerfDataMgr_t* perf_data, perfdata_metric_t met);
char * PerfDataMgr_get_MetricDescription(PerfDataMgr_t* perf_data,perfdata_metric_t met);
perfdata_mettype_t PerfDataMgr_get_MetricType(PerfDataMgr_t * perf_data, perfdata_metric_t met);

#endif /* __perfdataevent */
