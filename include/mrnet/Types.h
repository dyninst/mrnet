/****************************************************************************
 *  Copyright 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(Types_h)
#define Types_h

#include "xplat/Types.h"

#include <cstdio>
#include <map>
#include <vector>

#if !defined(NULL)
#define NULL (0)
#endif

#define FirstSystemTag 0
#define FirstApplicationTag 100

/* version info */
#define MRNET_VERSION_MAJOR 3
#define MRNET_VERSION_MINOR 1
#define MRNET_VERSION_REV   0

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
        PERFDATA_TYPE_UINT  = 0,
        PERFDATA_TYPE_INT   = 1,
        PERFDATA_TYPE_FLOAT = 2
    } perfdata_mettype_t;

    typedef enum PerfData_Metric {
        PERFDATA_MET_NUM_BYTES   = 0,
        PERFDATA_MET_NUM_PKTS    = 1,
        PERFDATA_MET_ELAPSED_SEC = 2,
        PERFDATA_MET_CPU_SYS_PCT = 3,
        PERFDATA_MET_CPU_USR_PCT = 4,
        PERFDATA_MET_MEM_VIRT_KB = 5,
        PERFDATA_MET_MEM_PHYS_KB = 6,
        PERFDATA_MAX_MET         = 7
    } perfdata_metric_t;

    typedef struct PerfData_MetricInfo {
        const char* name;
        const char* units;
        const char* description;
        perfdata_mettype_t type;
    } perfdata_metinfo_t;

    typedef enum PerfData_Context {
        PERFDATA_CTX_SEND     = 0,
        PERFDATA_CTX_RECV     = 1,
        PERFDATA_CTX_FILT_IN  = 2,
        PERFDATA_CTX_FILT_OUT = 3,
        PERFDATA_CTX_NONE     = 4,
        PERFDATA_PKT_RECV     = 5,
        PERFDATA_PKT_SEND     = 6,
        PERFDATA_PKT_NET_SENDCHILD = 7,
        PERFDATA_PKT_NET_SENDPAR = 8,
        PERFDATA_PKT_INT_DATAPAR = 9,
        PERFDATA_PKT_INT_DATACHILD = 10,
        PERFDATA_PKT_FILTER = 11,
        PERFDATA_PKT_RECV_TO_FILTER = 12,
        PERFDATA_PKT_FILTER_TO_SEND = 13,
        PERFDATA_MAX_CTX    = 14
    } perfdata_context_t;

    typedef enum PerfData_PacketTimers {
        PERFDATA_PKT_TIMERS_RECV     = 1,
        PERFDATA_PKT_TIMERS_SEND     = 2,
        PERFDATA_PKT_TIMERS_NET_SENDCHILD = 3,
        PERFDATA_PKT_TIMERS_NET_SENDPAR = 4,
        PERFDATA_PKT_TIMERS_INT_DATAPAR = 5,
        PERFDATA_PKT_TIMERS_INT_DATACHILD = 6,
        PERFDATA_PKT_TIMERS_FILTER = 7,
        PERFDATA_PKT_TIMERS_RECV_TO_FILTER = 8,
        PERFDATA_PKT_TIMERS_FILTER_TO_SEND = 9,
        PERFDATA_PKT_TIMERS_MAX = 10
    } perfdata_pkt_timers_t;

    typedef enum NetworkSettings {
        MRNET_DEBUG_LEVEL         = 0,
        MRNET_DEBUG_LOG_DIRECTORY = 1,
        MRNET_COMMNODE_PATH       = 2, 
        MRNET_FAILURE_RECOVERY    = 3,
        MRNET_STARTUP_TIMEOUT     = 4,
        MRNET_PORT_BASE           = 5,
        XPLAT_RSH                 = 6, 
        XPLAT_RSH_ARGS            = 7,
        XPLAT_REMCMD              = 8,
        CRAY_ALPS_APID            = 9,
        CRAY_ALPS_APRUN_PID       = 10,
        CRAY_ALPS_STAGE_FILES     = 11
    } net_settings_key_t;   

} /* namespace MRN */

#endif /* !defined(Types_h) */
