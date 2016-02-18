/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#ifndef MRNET_TYPES_H
#define MRNET_TYPES_H 1

#include "xplat/Types.h"

#include <cstdio>
#include <map>
#include <vector>

#if !defined(NULL)
#define NULL (0)
#endif

#define FirstSystemTag 0
#define FirstApplicationTag 100

/* version info - our build will define these, but API users want them too */
#ifndef MRNET_VERSION_MAJOR
# define MRNET_VERSION_MAJOR 5
# define MRNET_VERSION_MINOR 0
# define MRNET_VERSION_REV   1
#endif

namespace MRN
{
    /* version info */
    void get_Version( int& major,
                      int& minor,
                      int& revision );

    /* debug output control */
    extern const int MIN_OUTPUT_LEVEL;
    extern const int MAX_OUTPUT_LEVEL;
    extern int CUR_OUTPUT_LEVEL; 
    extern char* MRN_DEBUG_LOG_DIRECTORY;
    void set_OutputLevel(int l=1);

    void mrn_printf_init( FILE* ifp );
    int mrn_printf( const char *file, int line, const char * func, 
                    FILE * fp, const char *format, ... );

    /* pretty names for MRNet port and rank types. */
    typedef XPlat_Port Port;
    typedef uint32_t Rank;
    extern const Port UnknownPort;
    extern const Rank UnknownRank;

    /* network process types */
    typedef enum {
        UNKNOWN_NODE = 0,
        FE_NODE,
        BE_NODE,
        CP_NODE
    } node_type_t;

    /* ------------- Performance Data types ------------- */
    typedef union { 
        int64_t i;
        uint64_t u;
        double d;
    } perfdata_t;

    typedef std::map< Rank, std::vector< perfdata_t > > rank_perfdata_map;

    typedef enum PerfData_MetricType {
        PERFDATA_TYPE_UINT=0,
        PERFDATA_TYPE_INT,
        PERFDATA_TYPE_FLOAT
    } perfdata_mettype_t;

    typedef struct PerfData_MetricInfo {
        const char* name;
        const char* units;
        const char* description;
        perfdata_mettype_t type;
    } perfdata_metinfo_t;

    typedef enum PerfData_Metric {
        PERFDATA_MET_NUM_BYTES=0,
        PERFDATA_MET_NUM_PKTS,
        PERFDATA_MET_ELAPSED_SEC,
        PERFDATA_MET_CPU_SYS_PCT,
        PERFDATA_MET_CPU_USR_PCT,
        PERFDATA_MET_MEM_VIRT_KB, /* 5 */
        PERFDATA_MET_MEM_PHYS_KB, 
        PERFDATA_MAX_MET          /* 7 */
        /* Note that if MAX_MET ever increases, the type of the 
           PerfDataMgr::active_metrics bitfield must also be increased 
           from current 8-bit char to a 16-bit or 32-bit int */
    } perfdata_metric_t;

    typedef enum PerfData_Context {
        PERFDATA_CTX_NONE=0,
        PERFDATA_CTX_SEND,
        PERFDATA_CTX_RECV,
        PERFDATA_CTX_FILT_IN,
        PERFDATA_CTX_FILT_OUT,
        PERFDATA_CTX_SYNCFILT_IN,        /* 5 */
        PERFDATA_CTX_SYNCFILT_OUT,
        PERFDATA_CTX_PKT_RECV,    
        PERFDATA_CTX_PKT_SEND,
        PERFDATA_CTX_PKT_NET_SENDCHILD,
        PERFDATA_CTX_PKT_NET_SENDPAR,    /* 10 */
        PERFDATA_CTX_PKT_INT_DATAPAR,
        PERFDATA_CTX_PKT_INT_DATACHILD,
        PERFDATA_CTX_PKT_FILTER,
        PERFDATA_CTX_PKT_RECV_TO_FILTER,
        PERFDATA_CTX_PKT_FILTER_TO_SEND, /* 15 */
        PERFDATA_MAX_CTX               
    } perfdata_context_t;

    typedef enum PerfData_PacketTimers {
        PERFDATA_PKT_TIMERS_RECV=0,
        PERFDATA_PKT_TIMERS_SEND,
        PERFDATA_PKT_TIMERS_NET_SENDCHILD,
        PERFDATA_PKT_TIMERS_NET_SENDPAR,
        PERFDATA_PKT_TIMERS_INT_DATAPAR,    /* 5 */
        PERFDATA_PKT_TIMERS_INT_DATACHILD,
        PERFDATA_PKT_TIMERS_FILTER,
        PERFDATA_PKT_TIMERS_RECV_TO_FILTER,
        PERFDATA_PKT_TIMERS_FILTER_TO_SEND,
        PERFDATA_PKT_TIMERS_FILTER_SYNC,    /* 10 */
        PERFDATA_PKT_TIMERS_FILTER_UPDOWN,
        PERFDATA_PKT_TIMERS_MAX            
    } perfdata_pkt_timers_t;

    typedef enum NetworkSettings {
        MRNET_DEBUG_LEVEL=0,
        MRNET_DEBUG_LOG_DIRECTORY,
        MRNET_COMMNODE_PATH,
        MRNET_FAILURE_RECOVERY,
        MRNET_STARTUP_TIMEOUT,
        MRNET_PORT_BASE,           /* 5 */
        MRNET_EVENT_WAIT_TIMEOUT_MSEC,
        MRNET_TOPOLOGY_UPDATE_TIMEOUT_MSEC,
        XPLAT_RSH,
        XPLAT_RSH_ARGS,
        XPLAT_REMCMD,             /* 10 */
        CRAY_ALPS_APID,
        CRAY_ALPS_APRUN_PID,
        CRAY_ALPS_STAGE_FILES
    } net_settings_key_t;   

} /* namespace MRN */

#endif /* MRNET_TYPES_H */
