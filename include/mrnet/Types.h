/****************************************************************************
 *  Copyright 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(Types_h)
#define Types_h


#if !defined (__STDC_LIMIT_MACROS)
#  define __STDC_LIMIT_MACROS
#endif
#if !defined (__STDC_CONSTANT_MACROS)
#  define __STDC_CONSTANT_MACROS
#endif
#if !defined (__STDC_FORMAT_MACROS)
#  define __STDC_FORMAT_MACROS
#endif

#include <sys/types.h>

#if defined (os_linux)
#include <stdint.h>
#elif defined(os_solaris)
#include <inttypes.h>
#elif defined(os_windows)
#include "xplat/Types.h"
#define sleep(x) Sleep(1000*(DWORD)x)
typedef long int ssize_t;
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif

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

#ifndef FALSE
#define FALSE   (0)
#endif

#ifndef TRUE
#define TRUE    (1)
#endif

#if !defined(NULL)
#define NULL (0)
#endif

#include <cstdio>
#include <map>
#include <vector>

#define FirstSystemTag 0
#define FirstApplicationTag 100

namespace MRN
{
    /* debug output control */
    extern const int MIN_OUTPUT_LEVEL;
    extern const int MAX_OUTPUT_LEVEL;
    extern int CUR_OUTPUT_LEVEL; 
    extern char* MRN_DEBUG_LOG_DIRECTORY;
    void set_OutputLevel(int l=1);

    int mrn_printf( const char *file, int line, const char * func, 
                    FILE * fp, const char *format, ... );

    /* pretty names for MRNet port and rank types. */
    typedef uint16_t Port;
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
        PERFDATA_MAX_CTX      = 5
    } perfdata_context_t;
    
    typedef enum NetworkSettings {
        MRNET_DEBUG_LEVEL         = 0,
	MRNET_DEBUG_LOG_DIRECTORY = 1,
	MRNET_COMMNODE_PATH       = 2, 
	MRNET_FAILURE_RECOVERY    = 3,
	XPLAT_RSH                 = 4, 
	XPLAT_RSH_ARGS            = 5,
	XPLAT_REMCMD              = 6
    } net_settings_key_t;	
    
} /* namespace MRN */

#endif /* !defined(Types_h) */
