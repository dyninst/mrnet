/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined __perfdataevent 
#define __perfdataevent 1

#include "mrnet/Types.h"
#include "vector.h"

struct PerfDataMgr_t {
  // bitfield of enabled metrics per context
  char active_metrics[PERFDATA_MAX_CTX];
  vector_t* the_data;
} ;

typedef struct PerfDataMgr_t PerfDataMgr_t;

static perfdata_metinfo_t perfdata_metric_info[PERFDATA_MAX_MET];
static const char* perfdata_context_names[PERFDATA_MAX_CTX];

PerfDataMgr_t* new_PerfDataMgr_t();

void PerfDataMgr_enable(PerfDataMgr_t* perf_data,
                        perfdata_metric_t met,
                        perfdata_context_t ctx);

void PerfDataMgr_disable(PerfDataMgr_t* perf_data,
                        perfdata_metric_t met,
                        perfdata_context_t ctx);

int PerfDataMgr_is_Enabled(PerfDataMgr_t* perf_data, perfdata_metric_t met, perfdata_context_t ctx);

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


void PerfDataMgr_collect (PerfDataMgr_t* perf_data,
                        perfdata_metric_t met,
                        perfdata_context_t ctx,
                        vector_t* data);


perfdata_mettype_t PerfDataMgr_get_MetricType(perfdata_metric_t met);

#endif /* __perfdataevent */
