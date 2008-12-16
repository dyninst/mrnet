/****************************************************************************
 *  Copyright 2003-2008 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "mrnet/MRNet.h"
#include "Message.h"
#include "PerfDataEvent.h"
#include "PerfDataSysEvent.h"
#include "Protocol.h"
#include "utils.h"

#include <iostream>
using namespace std;

#define PERFDATA_MET_FLAG(x) ((char)(1 << x))

namespace MRN {

perfdata_metinfo_t PerfDataMgr::perfdata_metric_info[PERFDATA_MAX_MET] = 
{
    { "NumBytes", "bytes", "number of bytes", PERFDATA_TYPE_UINT },
    { "NumPackets", "packets", "number of packets", PERFDATA_TYPE_UINT },
    { "ElapsedTime", "seconds", "elapsed time", PERFDATA_TYPE_FLOAT },
    { "CPU-Sys", "%cpu", "system cpu utilization", PERFDATA_TYPE_FLOAT },
    { "CPU-User", "%cpu", "user cpu utilization", PERFDATA_TYPE_FLOAT },
    { "VirtMem", "kilobytes", "virtual memory size", PERFDATA_TYPE_FLOAT },
    { "PhysMem", "kilobytes", "resident memory size", PERFDATA_TYPE_FLOAT } 
};
 
const char* PerfDataMgr::perfdata_context_names[PERFDATA_MAX_CTX] =
{
    "Send",
    "Recv",
    "FilterIn",
    "FilterOut",
    "NoContext"
};

PerfDataMgr::PerfDataMgr( void )
    : the_data(PERFDATA_MAX_CTX)
{
    memset(active_metrics, 0, sizeof(active_metrics));
}

string PerfDataMgr::get_MetricName( perfdata_metric_t met )
{
    return perfdata_metric_info[(unsigned)met].name;
}
string PerfDataMgr::get_MetricUnits( perfdata_metric_t met )
{
    return perfdata_metric_info[(unsigned)met].units;
}
string PerfDataMgr::get_MetricDescription( perfdata_metric_t met )
{
    return perfdata_metric_info[(unsigned)met].description;
}
perfdata_mettype_t PerfDataMgr::get_MetricType( perfdata_metric_t met )
{
    return perfdata_metric_info[(unsigned)met].type;
}


void PerfDataMgr::enable( perfdata_metric_t met, perfdata_context_t ctx )
{
    int ndx = ctx;
    if( ndx < PERFDATA_MAX_CTX ) {
        if( met < PERFDATA_MAX_MET ) {
            active_metrics[ndx] |= PERFDATA_MET_FLAG(met);
        }
    }
}

void PerfDataMgr::disable( perfdata_metric_t met, perfdata_context_t ctx )
{ 
    int ndx = ctx;
    if( ndx < PERFDATA_MAX_CTX ) {
        if( met < PERFDATA_MAX_MET ) {
            active_metrics[ndx] &= ~(PERFDATA_MET_FLAG(met));
        }
    }
}

bool PerfDataMgr::is_Enabled( perfdata_metric_t met, perfdata_context_t ctx )
{
    int ndx = ctx;
    if( ndx < PERFDATA_MAX_CTX ) {
        if( active_metrics[ndx] & PERFDATA_MET_FLAG(met) ) {
            return true;
        }
    }
    return false;
}


void PerfDataMgr::collect( perfdata_metric_t met, perfdata_context_t ctx, 
                           vector<perfdata_t>& data )
{
    mrn_dbg_func_begin();
    
    _data_sync.Lock();
    map< perfdata_metric_t, vector<perfdata_t> >& ctx_map = the_data[(unsigned)ctx];
    vector<perfdata_t>& met_data = ctx_map[met];
    data = met_data;
    met_data.clear();
    _data_sync.Unlock(); 

    mrn_dbg_func_end();
}

void PerfDataMgr::print( perfdata_metric_t met, perfdata_context_t ctx )
{
    mrn_dbg_func_begin();
    
    vector<perfdata_t> data;
    vector<perfdata_t>::iterator iter;

    collect( met, ctx, data );
    
    struct timeval tv;
    gettimeofday(&tv, NULL);

    perfdata_metinfo_t* mi= perfdata_metric_info + (unsigned)met;
    char* report = NULL;
    asprintf( &report, "PERFDATA @ T=%d.%06dsec: %s %s -",
              (int)tv.tv_sec-MRN_RELEASE_DATE_SECS, (int)tv.tv_usec, mi->name, 
              perfdata_context_names[(unsigned)ctx] );

    for( iter = data.begin() ; iter != data.end(); iter++ ) {
        if( mi->type == PERFDATA_TYPE_UINT ) {
            mrn_printf(FLF, stderr, "%s %" PRIu64 " %s\n", 
                       report, (*iter).u, mi->units );
        }
        else if( mi->type == PERFDATA_TYPE_INT ) {
            mrn_printf(FLF, stderr, "%s %" PRIi64 " %s\n", 
                       report, (*iter).i, mi->units );
        }
        else if( mi->type == PERFDATA_TYPE_FLOAT ) {
            mrn_printf(FLF, stderr, "%s %lf %s\n", 
                       report, (*iter).d, mi->units );
        }
    }
    if( report != NULL )
        free(report);

    mrn_dbg_func_end();
}

void PerfDataMgr::add_DataInstance( perfdata_metric_t met, perfdata_context_t ctx, 
                                    perfdata_t data )
{
    mrn_dbg_func_begin();
    
    _data_sync.Lock();
    map< perfdata_metric_t, vector<perfdata_t> >& ctx_map = the_data[(unsigned)ctx];
    vector<perfdata_t>& met_data = ctx_map[met];
    met_data.push_back(data);
    _data_sync.Unlock(); 

    mrn_dbg_func_end();   
}

vector< perfdata_t > 
PerfDataMgr::get_DataSeries( perfdata_metric_t met, perfdata_context_t ctx )
{
    mrn_dbg_func_begin();
    
    _data_sync.Lock();
    map< perfdata_metric_t, vector<perfdata_t> >& ctx_map = the_data[(unsigned)ctx];
    vector<perfdata_t>& met_data = ctx_map[met];
    _data_sync.Unlock(); 
  
    return met_data;
}

void PerfDataMgr::set_DataValue( perfdata_metric_t met, perfdata_context_t ctx, 
                                 perfdata_t data )
{
    mrn_dbg_func_begin();
    
    _data_sync.Lock();
    map< perfdata_metric_t, vector<perfdata_t> >& ctx_map = the_data[(unsigned)ctx];
    vector<perfdata_t>& met_data = ctx_map[met];
    if( met_data.size() )
        met_data.clear();
    met_data.push_back( data );
    _data_sync.Unlock();
}

perfdata_t PerfDataMgr::get_DataValue( perfdata_metric_t met, perfdata_context_t ctx )
{
    mrn_dbg_func_begin();
    
    _data_sync.Lock();
    map< perfdata_metric_t, vector<perfdata_t> >& ctx_map = the_data[(unsigned)ctx];
    vector<perfdata_t>& met_data = ctx_map[met];
    _data_sync.Unlock();

    if( met_data.size() >= 1 )
        return met_data[0];
    else {
        perfdata_t zero;
        memset(&zero, 0, sizeof(zero));
        return zero;
    }
}

void PerfDataMgr::get_MemData(perfdata_metric_t metric)
{
    mrn_dbg_func_begin();

    double vsize=0, psize=0;

    PerfDataSysMgr::get_MemUsage (vsize, psize);
    mrn_dbg( 5, mrn_printf(FLF, stderr, "print_PerfData vsize %f psize %f \n", vsize, psize ));


    if (metric == PERFDATA_MET_MEM_VIRT_KB) {
        perfdata_t val;
        val.d = vsize;
        add_DataInstance( PERFDATA_MET_MEM_VIRT_KB, PERFDATA_CTX_NONE,
                                          val );

    } else if (metric == PERFDATA_MET_MEM_PHYS_KB) {
        perfdata_t val;
        val.d = psize;
        add_DataInstance( PERFDATA_MET_MEM_PHYS_KB, PERFDATA_CTX_NONE,
                                          val );
    }

    mrn_dbg_func_end();
}


} /* namespace MRN */
