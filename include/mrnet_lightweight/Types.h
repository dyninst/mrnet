/****************************************************************************
 *  Copyright 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__types_h)
#define __types_h 1

#include <sys/types.h>
#include <stdio.h>

/* need the fixed bit-width integer types */
#ifndef os_windows
# include <stdint.h>
#else
# include "xplat_lightweight/Types.h"
#endif

#ifndef true
#define true (1)
#endif
#ifndef false
#define false (0)
#endif

#if !defined (TRUE)
#define TRUE true
#endif

#if !defined (FALSE)
#define FALSE false
#endif

typedef uint16_t Port;
typedef uint32_t Rank;

#if !defined (bool_t)
#define bool_t int32_t
#endif

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
#define MRNET_VERSION_MAJOR 3
#define MRNET_VERSION_MINOR 1
#define MRNET_VERSION_REV   0
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

    //typedef std::map< Rank, std::vector< perfdata_t > > rank_perfdata_map;

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
        PERFDATA_MAX_CTX      = 5
    } perfdata_context_t;

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



#endif /* __types_h */
