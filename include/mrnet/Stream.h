/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__stream_h)
#define __stream_h 1

#include <list>
#include <vector>
#include <set>

#include "xplat/Monitor.h"
#include "xplat/Mutex.h"
#include "mrnet/FilterIds.h"
#include "mrnet/Packet.h"
#include "mrnet/Network.h"

namespace MRN
{

class EventPipe;
class Filter;
class FrontEndNode;
class BackEndNode;
class PerfDataMgr;

typedef enum {
    FILTER_DOWNSTREAM_TRANS,
    FILTER_UPSTREAM_TRANS,
    FILTER_UPSTREAM_SYNC
} FilterType; 

#define INVALID_STREAM_ID ((unsigned int)-1)
extern const unsigned int CTL_STRM_ID;
extern const unsigned int TOPOL_STRM_ID;
extern const unsigned int PORT_STRM_ID;
extern const unsigned int USER_STRM_BASE_ID;
extern const unsigned int INTERNAL_STRM_BASE_ID;
    
class Stream {

    friend class Network;
    friend class FrontEndNode;
    friend class BackEndNode;
    friend class InternalNode;
    friend class ParentNode;
    friend class ChildNode;
    friend class EventDetector;

 public:

    // BEGIN MRNET API

    int send( int itag, const char *iformat_str, ... );
    int send( const char *idata_fmt, va_list idata, int itag );
    int send( int itag, const void **idata, const char *iformat_str );
    int send( PacketPtr& ipacket );
    int flush(void) const;
    int recv( int *otag, PacketPtr &opacket, bool iblocking = true );

    const std::set< Rank > & get_EndPoints(void) const;
    unsigned int get_Id(void) const;
    unsigned int size(void) const;
    bool has_Data(void);

    int  get_DataNotificationFd(void);
    void clear_DataNotificationFd(void);
    void close_DataNotificationFd(void);

    int set_FilterParameters( FilterType ftype, const char *format_str, ... ) const;
    int set_FilterParameters( const char *params_fmt, va_list params, FilterType ftype ) const;

    bool enable_PerformanceData( perfdata_metric_t metric, 
                                 perfdata_context_t context );
    bool disable_PerformanceData( perfdata_metric_t metric, 
                                  perfdata_context_t context );
    bool collect_PerformanceData( rank_perfdata_map& results,
                                  perfdata_metric_t metric, 
                                  perfdata_context_t context,
                                  int aggr_filter_id = TFILTER_ARRAY_CONCAT );
    void print_PerformanceData( perfdata_metric_t metric, 
                                perfdata_context_t context );

    bool is_Closed(void) const;
     
    // END MRNET API

    Stream( Network *inetwork, unsigned int iid, 
            Rank *ibackends, unsigned int inum_backends,
            unsigned short ius_filter_id, 
            unsigned short isync_filter_id, 
            unsigned short ids_filter_id );

    ~Stream();

    int send_internal( int itag, const char *iformat_str, ... );

    void close(void);
    void get_ChildRanks( std::set< Rank >& ) const;
    void get_ChildPeers( std::set< PeerNodePtr >& ) const;
    void add_Stream_EndPoint( Rank irank );
    void add_Stream_Peer( Rank irank );

    PacketPtr collect_PerfData( perfdata_metric_t metric, 
                                perfdata_context_t context, 
                                int aggr_strm_id );

    bool is_LocalNodeInternal(void);
    //DEPRECATED -- renamed is_ShutDown to is_Closed
    bool is_ShutDown(void) const { return is_Closed(); }

    PerfDataMgr * get_PerfData(void);

 private:
    static bool find_FilterAssignment( const std::string& assignments, 
                                       Rank me, int& filter_id );

    int send_aux( PacketPtr &ipacket, bool upstream, bool internal=false );
    void send_to_children( PacketPtr &ipacket );

    void add_IncomingPacket( PacketPtr );
    PacketPtr get_IncomingPacket(void);
    int push_Packet( PacketPtr, std::vector<PacketPtr> &, std::vector<PacketPtr> &, 
                     bool upstream );

    int send_FilterStateToParent(void) const;
    PacketPtr get_FilterState(void) const;

    void set_FilterParams( FilterType, PacketPtr& ) const;

    bool enable_PerfData( perfdata_metric_t metric, perfdata_context_t context );
    bool disable_PerfData( perfdata_metric_t metric, perfdata_context_t context );
    void print_PerfData( perfdata_metric_t metric, perfdata_context_t context );

    void remove_Node( Rank irank );
    void recompute_ChildrenNodes(void);
    bool close_Peer( Rank irank );
    void signal_BlockedReceivers(void) const;
    int block_ForIncomingPacket(void) const;

    //Static Data Members
    PerfDataMgr * _perf_data;
    Network * _network;
    unsigned int _id;
    unsigned int _sync_filter_id;
    Filter * _sync_filter;
    unsigned int _us_filter_id;
    Filter * _us_filter;
    unsigned int _ds_filter_id;
    Filter * _ds_filter;
    std::set< Rank > _end_points;

    //Dynamic Data Members
    EventPipe * _evt_pipe;
    bool _was_closed;
    int _num_sending;
    std::set< PeerNodePtr > _peers; // child peers in stream
    mutable XPlat::Mutex _peers_sync;
    mutable XPlat::Monitor _send_sync;

    std::list< PacketPtr > _incoming_packet_buffer;
    mutable XPlat::Monitor _incoming_packet_buffer_sync;

    enum {PACKET_BUFFER_NONEMPTY, STREAM_SEND_EMPTY};
};


} // namespace MRN

#endif /* __stream_h */
