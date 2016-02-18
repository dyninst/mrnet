/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#ifndef __mrnet_types_h
#define __mrnet_types_h 1

#include "xplat_lightweight/Types.h"

typedef XPlat_Port Port;
typedef uint32_t Rank;

#if !defined (enum_t)
#define enum_t int32_t
#endif

#if !defined (char_t)
#define char_t char
#endif 

#if !defined (uchar_t)
#define uchar_t unsigned char
#endif

#define FirstSystemTag 0
#define FirstApplicationTag 100

/* version info */
#ifndef MRNET_VERSION_MAJOR
#define MRNET_VERSION_MAJOR 5
#define MRNET_VERSION_MINOR 0
#define MRNET_VERSION_REV   1
#endif
void get_Version(int* major,
                 int* minor,
                 int* revision);

int mrn_printf( const char *file, int line, const char* func, 
                FILE *fp, const char *format, ... );

/* --------------- Performance Data Types -------------- */
typedef union { 
    int64_t i;
    uint64_t u;
    double d;
} perfdata_t;

typedef enum PerfData_MetricType {
    PERFDATA_TYPE_UINT  = 0,
    PERFDATA_TYPE_INT   = 1,
    PERFDATA_TYPE_FLOAT = 2
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
    PERFDATA_PKT_TIMERS_MAX             /* 10 */
} perfdata_pkt_timers_t;

typedef enum update_type {
    TOPO_NEW_BE        = 0,
    TOPO_REMOVE_RANK   = 1,
    TOPO_CHANGE_PARENT = 2,
    TOPO_CHANGE_PORT   = 3,
    TOPO_NEW_CP        = 4
} update_type_t;

typedef struct {
    int type;
    uint32_t prank;
    uint32_t crank;
    char * chost;
    uint16_t cport;
} update_contents_t;

typedef enum NetworkSettings {
    MRNET_DEBUG_LEVEL         = 0,
    MRNET_DEBUG_LOG_DIRECTORY = 1,
    MRNET_COMMNODE_PATH       = 2,
    MRNET_FAILURE_RECOVERY    = 3,
    XPLAT_RSH                 = 4,
    XPLAT_RSH_ARGS            = 5,
    XPLAT_REMCMD              = 6
} net_settings_key_t;


typedef enum NodeType {
    NODE_TYPE_FRONTEND = 0,
    NODE_TYPE_BACKEND = 1,
    NODE_TYPE_INTERNALNODE = 2
} node_type_t;

#endif /* __mrnet_types_h */
