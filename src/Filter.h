/****************************************************************************
 *  Copyright 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__filter_h)
#define __filter_h 1

#include <map>
#include <string>
#include <vector>

#include "mrnet/Stream.h"

#include "mrnet/Types.h"
#include "mrnet/Error.h"
#include "mrnet/NetworkTopology.h"
#include "xplat/Mutex.h"

#include "utils.h"
#include "FilterDefinitions.h"
#include "Message.h"
#include "ParadynFilterDefinitions.h"
#include "PeerNode.h"

namespace MRN
{

class Filter: public Error {
    friend class FilterCounter;

    class FilterInfo {
        friend class Filter;
     public:
        FilterInfo(void)
            : filter_func(NULL), state_func(NULL), filter_fmt(NULL_STRING)
        {
        }
         FilterInfo( void(*ifilter)(), void(*istate)(), const char* ifmt )
            : filter_func(ifilter), state_func(istate), filter_fmt(ifmt)
        {
        }
        FilterInfo( const FilterInfo &finfo )
            : filter_func(finfo.filter_func), state_func(finfo.state_func), 
              filter_fmt(finfo.filter_fmt)
        {
        }
        ~FilterInfo(void)
        {
        }
     private:
        void(*filter_func)();
        void(*state_func)();
        std::string filter_fmt;
    };

 public:
    Filter( unsigned short iid, Stream * strm );
    ~Filter( void );

    int push_Packets( std::vector < PacketPtr > &ipackets,
                      std::vector < PacketPtr > &opackets, 
                      std::vector < PacketPtr > &opackets_reverse,
                      const TopologyLocalInfo &info );

    PacketPtr get_FilterState( int istream_id );
    void set_FilterParams( PacketPtr iparams );

    static int load_FilterFunc( unsigned short iid, const char *so_file, const char *func );
    static bool register_Filter( unsigned short iid,
                                 void (*ifilter_func)( ),
                                 void (*istate_func)( ),
                                 const char * ifmt );

 private:
    static void initialize_static_stuff( );
    static void free_static_stuff( );

    static std::map< unsigned short, FilterInfo > Filters;
    unsigned short _id;
    void * _filter_state;
    XPlat::Mutex _mutex;
    std::string _fmt_str;
    void ( *_filter_func )( const std::vector < PacketPtr >&,
                            std::vector < PacketPtr >&, 
                            std::vector < PacketPtr >&, 
                            void **, PacketPtr&, const TopologyLocalInfo& );
    PacketPtr ( *_get_state_func )( void ** ifilter_state, int istream_id );

    Stream * _strm;
    PacketPtr _params;
};

inline void Filter::initialize_static_stuff( )
{
    unsigned short tfilter_start = 0;
    unsigned short sfilter_start = 50;
    
    TFILTER_NULL = tfilter_start++;
    register_Filter( TFILTER_NULL, NULL, NULL, TFILTER_NULL_FORMATSTR );

    TFILTER_SUM = tfilter_start++;
    register_Filter( TFILTER_SUM, 
                     (void(*)())tfilter_Sum, NULL, TFILTER_SUM_FORMATSTR );

    TFILTER_AVG = tfilter_start++;
    register_Filter( TFILTER_AVG, 
                     (void(*)())tfilter_Avg, NULL, TFILTER_AVG_FORMATSTR );

    TFILTER_MAX = tfilter_start++;
    register_Filter( TFILTER_MAX, 
                     (void(*)())tfilter_Max, NULL, TFILTER_MAX_FORMATSTR );

    TFILTER_MIN = tfilter_start++;
    register_Filter( TFILTER_MIN, 
                     (void(*)())tfilter_Min, NULL, TFILTER_MIN_FORMATSTR );

    TFILTER_ARRAY_CONCAT = tfilter_start++;
    register_Filter( TFILTER_ARRAY_CONCAT, 
                     (void(*)())tfilter_ArrayConcat, NULL,
                     TFILTER_ARRAY_CONCAT_FORMATSTR );

    TFILTER_TOPO_UPDATE = tfilter_start++;
    register_Filter( TFILTER_TOPO_UPDATE, (void(*)())tfilter_TopoUpdate, NULL,
                     TFILTER_TOPO_UPDATE_FORMATSTR );

    TFILTER_TOPO_UPDATE_DOWNSTREAM = tfilter_start++;
    register_Filter( TFILTER_TOPO_UPDATE_DOWNSTREAM, (void(*)())tfilter_TopoUpdate_Downstream, NULL,
                     TFILTER_TOPO_UPDATE_DOWNSTREAM_FORMATSTR );

    TFILTER_INT_EQ_CLASS = tfilter_start++;
    register_Filter( TFILTER_INT_EQ_CLASS, 
                     (void(*)())tfilter_IntEqClass, NULL,
                     TFILTER_INT_EQ_CLASS_FORMATSTR );

    TFILTER_PERFDATA = tfilter_start++;
    register_Filter( TFILTER_PERFDATA, 
                     (void(*)())tfilter_PerfData, NULL,
                     TFILTER_PERFDATA_FORMATSTR );

#ifdef _NEED_PARADYN_FILTERS_
    TFILTER_SAVE_LOCAL_CLOCK_SKEW_UPSTREAM = tfilter_start++;
    register_Filter( TFILTER_SAVE_LOCAL_CLOCK_SKEW_UPSTREAM, 
                     (void(*)())save_LocalClockSkewUpstream, NULL,
                     save_LocalClockSkewUpstream_format_string );

    TFILTER_SAVE_LOCAL_CLOCK_SKEW_DOWNSTREAM = tfilter_start++;
    register_Filter( TFILTER_SAVE_LOCAL_CLOCK_SKEW_DOWNSTREAM, 
                     (void(*)())save_LocalClockSkewDownstream, NULL,
                     save_LocalClockSkewDownstream_format_string );

    TFILTER_GET_CLOCK_SKEW = tfilter_start++;
    register_Filter( TFILTER_GET_CLOCK_SKEW, 
                     (void(*)())get_ClockSkew, NULL,
                     get_ClockSkew_format_string );

    TFILTER_PD_UINT_EQ_CLASS = tfilter_start++;
    register_Filter( TFILTER_PD_UINT_EQ_CLASS, 
                     (void(*)())tfilter_PDUIntEqClass, NULL,
                     tfilter_PDUIntEqClass_format_string );
#endif /*_NEED_PARADYN_FILTERS_*/

    SFILTER_DONTWAIT = sfilter_start++;
    register_Filter( SFILTER_DONTWAIT, 
                     (void(*)())NULL, NULL, NULL_STRING );

    SFILTER_WAITFORALL = sfilter_start++;
    register_Filter( SFILTER_WAITFORALL, 
                     (void(*)())sfilter_WaitForAll, NULL, NULL_STRING );

    SFILTER_TIMEOUT = sfilter_start++;
    register_Filter( SFILTER_TIMEOUT, 
                     (void(*)())sfilter_TimeOut, NULL, NULL_STRING );
}

inline void Filter::free_static_stuff( )
{
}

class FilterCounter {
 private:
   static int count;
  
 public:
   FilterCounter() {
      if (count++ == 0)
         Filter::initialize_static_stuff();
   }
  ~FilterCounter() {
      if (--count == 0)
         Filter::free_static_stuff();
   }
};

} // namespace MRN

#endif  /* __filter_h */
