/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "PerfDataEvent.h" 
#include "PerfDataSysEvent.h"

#include "xplat_lightweight/map.h"
#include "xplat_lightweight/vector.h"

perfdata_metinfo_t perfdata_metric_info[PERFDATA_MAX_MET] = 
{
    { "NumBytes", "bytes", "number of bytes", PERFDATA_TYPE_UINT },
    { "NumPackets", "packets", "number of packets", PERFDATA_TYPE_UINT },
    { "ElapsedTime", "seconds", "elapsed time", PERFDATA_TYPE_FLOAT },
    { "CPU-Sys", "%cpu", "system cpu utilization", PERFDATA_TYPE_FLOAT },
    { "CPU-User", "%cpu", "user cpu utilization", PERFDATA_TYPE_FLOAT },
    { "VirtMem", "kilobytes", "virtual memory size", PERFDATA_TYPE_FLOAT },
    { "PhysMem", "kilobytes", "resident memory size", PERFDATA_TYPE_FLOAT }
};

const char* perfdata_context_names[PERFDATA_MAX_CTX] = 
{
    "NoContext",
    "Send",
    "Recv",
    "TransFilter_In",
    "TransFilter_Out",
    "SyncFilter_In",
    "SyncFilter_Out",
    "Time_Packet_Recv",
    "Time_Packet_Send",
    "Time_Packet_Network_SendChild",
    "Time_Packet_Network_SentParent",
    "Time_Packet_Internal_DataParent",
    "Time_Packet_Internal_DataChild",
    "Time_Packet_Filter",
    "Time_Packet_Recv_To_Filter",
    "Time_Packet_Filter_To_Send"
};

#define PERFDATA_MET_FLAG(x) ((char)(1 << x))

PerfDataMgr_t* new_PerfDataMgr_t()
{
    unsigned int i;
    mrn_map_t* newmap;
    PerfDataMgr_t* newmgr = (PerfDataMgr_t*) calloc(1, sizeof(PerfDataMgr_t));
    assert(newmgr != NULL);
    newmgr->the_data = new_empty_vector_t();
/*     mrn_dbg(5, mrn_printf(FLF, stderr, "new_PerfDataMgr_t() = %p, mgr->the_data = %p\n", newmgr, newmgr->the_data)); */
    memset(newmgr->active_metrics, 0, sizeof(newmgr->active_metrics));
    for (i = 0; i < PERFDATA_MAX_CTX; i++) {
        newmap = new_map_t();
        pushBackElement(newmgr->the_data, newmap);
    }
    return newmgr;
}

void delete_PerfDataMgr_t( PerfDataMgr_t* mgr )
{
    mrn_map_t* amap;
    unsigned int i;
    if( mgr != NULL ) {
/*         mrn_dbg(5, mrn_printf(FLF, stderr, "delete_PerfDataMgr_t() = %p, mgr->the_data = %p\n", mgr, mgr->the_data)); */
        if( mgr->the_data != NULL ) {
            for (i = 0; i < mgr->the_data->size; i++) {
                amap = (mrn_map_t*)(mgr->the_data->vec[i]);
                if( amap != NULL )
                    delete_map_t(amap);
            }
            delete_vector_t( mgr->the_data );
	    mgr->the_data = NULL;
        }
        free( mgr );
    }
}

const char* PerfDataMgr_get_MetricName(PerfDataMgr_t* UNUSED(perf_data),
                                       perfdata_metric_t met)
{
    return perfdata_metric_info[(unsigned)met].name;
}

const char* PerfDataMgr_get_MetricUnits(PerfDataMgr_t* UNUSED(perf_data),
                                   perfdata_metric_t met)
{
    return perfdata_metric_info[(unsigned)met].units;
}

const char* PerfDataMgr_get_MetricDescription(PerfDataMgr_t* UNUSED(perf_data),
                                         perfdata_metric_t met)
{
    return perfdata_metric_info[(unsigned)met].description;
}

perfdata_mettype_t PerfDataMgr_get_MetricType(PerfDataMgr_t* UNUSED(perf_data),
                                              perfdata_metric_t met)
{
    return perfdata_metric_info[(unsigned)met].type;
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

int PerfDataMgr_is_Enabled(PerfDataMgr_t* perf_data, 
                           perfdata_metric_t met, 
			   perfdata_context_t ctx)
{
    int ndx = ctx;
    if (ndx < PERFDATA_MAX_CTX) {
        if (perf_data->active_metrics[ndx] & PERFDATA_MET_FLAG(met)) {
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
    perfdata_t* newdata;
    mrn_map_t* ctx_map;
    vector_t* met_data;

    mrn_dbg_func_begin();
    newdata = (perfdata_t*) malloc(sizeof(perfdata_t));
    assert(newdata != NULL);
    *newdata = data;

    ctx_map = (mrn_map_t*)(perf_data->the_data->vec[(unsigned)ctx]);
    if (ctx_map->size > 0) {
        met_data = (vector_t*)(get_val(ctx_map,(int)met));
        if ((met_data!=NULL) && (met_data->size)) {
            pushBackElement(met_data, newdata);
        } else {
            met_data = new_empty_vector_t();
            pushBackElement(met_data, newdata);
        }
    }
    else {
        met_data = new_empty_vector_t();
        pushBackElement(met_data, newdata);
        insert(ctx_map, (int)met, met_data);
    }

    mrn_dbg_func_end();
}

perfdata_t PerfDataMgr_get_DataValue(PerfDataMgr_t* perf_data, 
                                     perfdata_metric_t met,
                                     perfdata_context_t ctx)
{
    mrn_map_t* ctx_map;
    vector_t* met_data;
    perfdata_t zero;

    mrn_dbg_func_begin();

    ctx_map = (mrn_map_t*)(perf_data->the_data->vec[(unsigned)ctx]);
    if(ctx_map->size > 0) {
        met_data = (vector_t*)(get_val(ctx_map, (int)met));
        if( (met_data!=NULL) && (met_data->size)) {
            mrn_dbg_func_end();
            return *(perfdata_t*)met_data->vec[0];
        }
    }
    // if ctx_map or met_data are empty...

    memset(&zero, 0, sizeof(zero));
    mrn_dbg_func_end();
    return zero;
}

void PerfDataMgr_set_DataValue(PerfDataMgr_t* perf_data,
                               perfdata_metric_t met,
                               perfdata_context_t ctx,
                               perfdata_t data)
{
    perfdata_t* newdata;
    mrn_map_t* ctx_map;
    vector_t* met_data;
    
    mrn_dbg_func_begin();
    newdata = (perfdata_t*)malloc(sizeof(perfdata_t));
    assert(newdata != NULL);
    *newdata = data;

    ctx_map = (mrn_map_t*)(perf_data->the_data->vec[(unsigned)ctx]);
    mrn_dbg(5, mrn_printf(FLF, stderr, "ctx_map->size=%d\n", ctx_map->size));
    if (ctx_map->root) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "ctx_map->root is not NULL\n"));
    }
    else { 
        mrn_dbg(5, mrn_printf(FLF, stderr, "ctx_map->root _is_ NULL\n"));
    }
    if (ctx_map->size > 0) {
        met_data = (vector_t*)(get_val(ctx_map, (int)met));
        if ((met_data!=NULL) && (met_data->size)) {
            clear(met_data);
        }
        if (met_data==NULL) {
            met_data = new_empty_vector_t();
        }
        pushBackElement(met_data, newdata);
    }
    else {
        met_data = new_empty_vector_t();
        pushBackElement(met_data, newdata);
        insert(ctx_map, (int)met, met_data);
    }
    mrn_dbg_func_end();
}

void PerfDataMgr_get_MemData(PerfDataMgr_t* perf_data,
                             perfdata_metric_t metric)
{
    double vsize=0, psize=0;
    perfdata_t val;
	
    mrn_dbg_func_begin();

    PerfDataSysMgr_get_MemUsage(&vsize, &psize);
    mrn_dbg(5, mrn_printf(FLF, stderr, "print_PerfData vsize %f psize %f\n", vsize, psize));

    if (metric == PERFDATA_MET_MEM_VIRT_KB) {
        val.d = vsize;
        PerfDataMgr_add_DataInstance(perf_data, 
                                    PERFDATA_MET_MEM_VIRT_KB,
                                    PERFDATA_CTX_NONE,
                                    val);
    } else if (metric == PERFDATA_MET_MEM_PHYS_KB) {
        val.d = psize;
        PerfDataMgr_add_DataInstance(perf_data,
                                    PERFDATA_MET_MEM_PHYS_KB,
                                    PERFDATA_CTX_NONE,
                                    val);   
    }
    mrn_dbg(5, mrn_printf(FLF, stderr, "Are we really segfaulting here?\n"));
    mrn_dbg_func_end();
}

void PerfDataMgr_print(PerfDataMgr_t* perf_data,
                       perfdata_metric_t met,
                       perfdata_context_t ctx) 
{
    vector_t* data = new_empty_vector_t();
    struct timeval tv;
    perfdata_metinfo_t* mi;
    int size;
    char* report;
    unsigned int k;

    mrn_dbg_func_begin();

    PerfDataMgr_collect(perf_data, met, ctx, data); 

    gettimeofday(&tv, NULL);

    mi = perfdata_metric_info + (unsigned)met;

    size = 36; // "PERFDATA @ T=" + "%d" + "%06dsec: "
    size += strlen(mi->name);
    size += strlen(perfdata_context_names[(unsigned)ctx]);
	
    report= (char*)malloc(sizeof(char)*size);
    assert(report);
    sprintf( report, "PERFDATA @ T=%d.%06dsec: %s %s -",
             (int)tv.tv_sec-MRN_RELEASE_DATE_SECS, (int)tv.tv_usec, mi->name, 
              perfdata_context_names[(unsigned)ctx] );

    for (k = 0; k < data->size; k++) {
        if (mi->type == PERFDATA_TYPE_UINT) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "%s %u %s\n",
                    report, ((perfdata_t*)data->vec[k])->u, mi->units));
        } else if (mi->type == PERFDATA_TYPE_INT) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "%s %i %s\n",
                    report, ((perfdata_t*)data->vec[k])->i, mi->units));
        } else if (mi->type == PERFDATA_TYPE_FLOAT) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "%s %lf %s\n",
                    report, ((perfdata_t*)data->vec[k])->d, mi->units));
        }
    }
    if (report != NULL)
        free(report);

    mrn_dbg_func_end();
}

void PerfDataMgr_collect(PerfDataMgr_t* perf_data,
                         perfdata_metric_t met,
                         perfdata_context_t ctx,
                         vector_t* data)
{
    mrn_map_t* ctx_map;
    vector_t* met_data;

    mrn_dbg_func_begin();

    ctx_map = (mrn_map_t*)(perf_data->the_data->vec[(unsigned)ctx]);
    if (ctx_map->size > 0) {
        //vector_t* met_data = ctx_map[met];
        met_data = (vector_t*)(get_val(ctx_map, (int)met));
        //data = met_data;
        if ((met_data!= NULL) && (met_data->size)) {
            copy_vector(met_data, data);
            clear(met_data);    
        }
    } else {
        met_data = new_empty_vector_t();
        insert(ctx_map, (int)met, met_data);
    }

    mrn_dbg_func_end();
}

