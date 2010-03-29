/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__types_h)
#define __types_h 1

#include <sys/types.h>

#if defined (os_linux)
#include <stdint.h>
#elif defined(os_solaris)
#include <inttypes.h>
#elif defined(os_windows)
#include "xplat/Types.h"
#define sleep(x) Sleep(1000*(DWORD)x)
#endif


#define true 1
#define false 0

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

/* --------------- Performance Data Types -------------- */
    typedef union { 
        int64_t i;
        uint64_t u;
        double d;
    } perfdata_t;

    //typedef std::map< Rank, std::vector< perfdata_t > > rank_perfdata_map;

    typedef enum PerfData_MetricType {
        PERFDATA_TYPE_UINT = 0,
        PERFDATA_TYPE_INT = 1,
        PERFDATA_TYPE_FLOAT = 2
    } perfdata_mettype_t;

    typedef enum PerfData_Metric {
        PERFDATA_MET_NUM_BYTES = 0,
        PERFDATA_MET_NUM_PKTS = 1,
        PERFDATA_MET_ELAPSED_SEC = 2,
        PERFDATA_MET_CPU_SYS_PCT = 3,
        PERFDATA_MET_CPU_USR_PCT  = 4,
        PERFDATA_MET_MEM_VIRT_KB = 5,
        PERFDATA_MET_MEM_PHYS_KB = 6,
        PERFDATA_MAX_MET = 7
    } perfdata_metric_t;

    typedef struct PerfData_MetricInfo {
        const char* name;
        const char* units;
        const char* description;
        perfdata_mettype_t type;
    } perfdata_metinfo_t;

    typedef enum PerfData_Context {
        PERFDATA_CTX_SEND = 0,
        PERFDATA_CTX_RECV = 1,
        PERFDATA_CTX_FILT_IN = 2,
        PERFDATA_CTX_FILT_OUT = 3,
	    PERFDATA_CTX_NONE = 4,
        PERFDATA_MAX_CTX = 5
    } perfdata_context_t;


#endif /* __types_h */
