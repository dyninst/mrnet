/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <inttypes.h>

#include "PerfDataEvent.h" 
#include "map.h"
#include "mrnet/Types.h"
#include "Utils.h"
#include "vector.h"

#define PERFDATA_MET_FLAG(x) ((char)(1 << x))

PerfDataMgr_t* new_PerfDataMgr_t()
{
  PerfDataMgr_t* newperf = (PerfDataMgr_t*)malloc(sizeof(PerfDataMgr_t));
  assert(newperf != NULL);
  newperf->the_data = new_empty_vector_t();
  int i;
  for (i = 0; i < PERFDATA_MAX_CTX; i++) {
    map_t* newmap = new_map_t();
    pushBackElement(newperf->the_data, newmap);
  }
  return newperf;
}

void PerfDataMgr_enable(PerfDataMgr_t* perf_data,
                        perfdata_metric_t met,
                        perfdata_context_t ctx)
{
    int ndx = ctx;
    if (ndx < PERFDATA_MAX_CTX) {
        if (met < PERFDATA_MAX_MET) {
            perf_data->active_metrics[ndx] |= PERFDATA_MET_FLAG(met);
        }
    }
}

void PerfDataMgr_disable(PerfDataMgr_t* perf_data,
                        perfdata_metric_t met, 
                        perfdata_context_t ctx)
{
    int ndx = ctx;
    if (ndx < PERFDATA_MAX_CTX) {
        if (met < PERFDATA_MAX_MET) {
            perf_data->active_metrics[ndx] &= ~(PERFDATA_MET_FLAG(met));
        }
    }
}

int PerfDataMgr_is_Enabled(PerfDataMgr_t* perf_data, perfdata_metric_t met, perfdata_context_t ctx)
{
  int ndx = ctx;
  if (ndx < PERFDATA_MAX_CTX) {
    if (perf_data->active_metrics[ndx] & PERFDATA_MET_FLAG(met)) {
      mrn_dbg(5, mrn_printf(FLF, stderr, "about to return true\n"));
      return true;
    } 
  }
  return false;
}

void PerfDataMgr_add_DataInstance(PerfDataMgr_t* perf_data,
                                  perfdata_metric_t met,
                                  perfdata_context_t ctx,
                                  perfdata_t data)
{
    mrn_dbg_func_begin();
    perfdata_t* newdata = (perfdata_t*)malloc(sizeof(perfdata_t));
    assert(newdata != NULL);
    *newdata = data;

    map_t* ctx_map = (map_t*)(perf_data->the_data->vec[(unsigned)ctx]);
    if (ctx_map->size > 0) {
        vector_t* met_data = (vector_t*)(get_val(ctx_map,met));
        if ((met_data!=NULL) && (met_data->size)) {
            pushBackElement(met_data, newdata);
        } else {
            met_data = new_empty_vector_t();
            pushBackElement(met_data, newdata);
        }
    }
    else {
        vector_t* met_data = new_empty_vector_t();
        pushBackElement(met_data, newdata);
        insert(ctx_map, met, met_data);
    }

    mrn_dbg_func_end();
}

perfdata_t PerfDataMgr_get_DataValue(PerfDataMgr_t* perf_data, 
                                    perfdata_metric_t met,
                                    perfdata_context_t ctx)
{
    mrn_dbg_func_begin();

    map_t* ctx_map = (map_t*)(perf_data->the_data->vec[(unsigned)ctx]);
    if(ctx_map->size > 0) {
        vector_t* met_data = (vector_t*)(get_val(ctx_map, met));
        if( (met_data!=NULL) && (met_data->size)) {
            mrn_dbg_func_end();
            return *(perfdata_t*)met_data->vec[0];
        }
    }
    // if ctx_map or met_data are empty...
    perfdata_t zero;
    memset(&zero, 0, sizeof(zero));
    mrn_dbg_func_end();
    return zero;
}

void PerfDataMgr_set_DataValue(PerfDataMgr_t* perf_data,
                                perfdata_metric_t met,
                                perfdata_context_t ctx,
                                perfdata_t data)
{
    mrn_dbg_func_begin();
    perfdata_t* newdata = (perfdata_t*)malloc(sizeof(perfdata_t));
    assert(newdata != NULL);
    *newdata = data;

    map_t* ctx_map = (map_t*)(perf_data->the_data->vec[(unsigned)ctx]);
    mrn_dbg(5, mrn_printf(FLF, stderr, "ctx_map->size=%d\n", ctx_map->size));
    if (ctx_map->root) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "ctx_map->root is not NULL\n"));
    }
    else { 
        mrn_dbg(5, mrn_printf(FLF, stderr, "ctx_map->root _is_ NULL\n"));
    }
    if (ctx_map->size > 0) {
        vector_t* met_data = (vector_t*)(get_val(ctx_map, met));
        if ((met_data!=NULL) && (met_data->size)) {
            clear(met_data);
        }
        if (met_data==NULL) {
            met_data = new_empty_vector_t();
        }
        pushBackElement(met_data, newdata);
    }
    else {
        vector_t* met_data = new_empty_vector_t();
        pushBackElement(met_data, newdata);
        insert(ctx_map, met, met_data);
    }
    mrn_dbg_func_end();
}

void PerfDataMgr_get_MemData(PerfDataMgr_t* perf_data,
                            perfdata_metric_t metric)
{
    mrn_dbg_func_begin();

    double vsize=0, psize=0;

    PerfDataSysMgr_get_MemUsage(vsize, psize);
    mrn_dbg(5, mrn_printf(FLF, stderr, "print_PerfData vsize %f psize %f\n", vsize, psize));

    if (metric == PERFDATA_MET_MEM_VIRT_KB) {
        perfdata_t val;
        val.d = vsize;
        PerfDataMgr_add_DataInstance(perf_data, 
                                    PERFDATA_MET_MEM_VIRT_KB,
                                    PERFDATA_CTX_NONE,
                                    val);
    } else if (metric == PERFDATA_MET_MEM_PHYS_KB) {
        perfdata_t val;
        val.d = psize;
        PerfDataMgr_add_DataInstance(perf_data,
                                    PERFDATA_MET_MEM_PHYS_KB,
                                    PERFDATA_CTX_NONE,
                                    val);   
    }
    mrn_dbg_func_end();
}

void PerfDataMgr_print(PerfDataMgr_t* perf_data,
                        perfdata_metric_t met,
                        perfdata_context_t ctx) 
{
    mrn_dbg_func_begin();

    vector_t* data = new_empty_vector_t();

    PerfDataMgr_collect(perf_data, met, ctx, data); 

    struct timeval tv;
    gettimeofday(&tv, NULL);

    perfdata_metinfo_t* mi = perfdata_metric_info + (unsigned)met;
    char* report = NULL;
    asprintf(&report, "PERFDATA @ T=%d.%06dsec: %s %s -",
            (int)tv.tv_sec-MRN_RELEASE_DATE_SECS,
            (int)tv.tv_usec, mi->name,
            perfdata_context_names[(unsigned)ctx]);

    int k;
    for (k = 0; k < data->size; k++) {
        if (mi->type == PERFDATA_TYPE_UINT) {
            mrn_printf(FLF, stderr, "%s %" PRIu64 " %s\n",
                    report, ((perfdata_t*)data->vec[k])->u, mi->units);
        } else if (mi->type == PERFDATA_TYPE_INT) {
            mrn_printf(FLF, stderr, "%s %" PRIi64 "%s\n",
                    report, ((perfdata_t*)data->vec[k])->i, mi->units);
        } else if (mi->type == PERFDATA_TYPE_FLOAT) {
            mrn_printf(FLF, stderr, "%s %lf %s\n",
                    report, ((perfdata_t*)data->vec[k])->d, mi->units);
        }
    }
    if (report != NULL)
        free(report);

    mrn_dbg_func_end();
}

void PerfDataMgr_collect (PerfDataMgr_t* perf_data,
                        perfdata_metric_t met,
                        perfdata_context_t ctx,
                        vector_t* data)
{
    mrn_dbg_func_begin();

    map_t* ctx_map = (map_t*)(perf_data->the_data->vec[(unsigned)ctx]);
    if (ctx_map->size > 0) {
        //vector_t* met_data = ctx_map[met];
        vector_t* met_data = (vector_t*)(get_val(ctx_map, met));
        //data = met_data;
        if ((met_data!= NULL) && (met_data->size)) {
            *data = *met_data;
            clear(met_data);    
        }
    } else {
        vector_t* met_data = new_empty_vector_t();
        insert(ctx_map, met, met_data);
    }

    mrn_dbg_func_end();
}

perfdata_mettype_t PerfDataMgr_get_MetricType(perfdata_metric_t met)
{
    return perfdata_metric_info[(unsigned)met].type;
}
