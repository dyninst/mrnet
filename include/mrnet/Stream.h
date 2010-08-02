/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
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

class Filter;
class FrontEndNode;
class BackEndNode;
class PerfDataMgr;

typedef enum {
    FILTER_DOWNSTREAM_TRANS,
    FILTER_UPSTREAM_TRANS,
    FILTER_UPSTREAM_SYNC
} FilterType; 

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
    int send_internal( int itag, const char *iformat_str, ... );
    int send( const char *idata_fmt, va_list idata, int itag );
    int send( int itag, const void **idata, const char *iformat_str );
    int send( PacketPtr& ipacket );
    int flush( ) const;
    int recv( int *otag, PacketPtr &opacket, bool iblocking = true );

    const std::set< Rank > & get_EndPoints( void ) const ;
    unsigned int get_Id( void ) const ;
    unsigned int size( void ) const ;
    bool has_Data( void );

    int  get_DataNotificationFd( void );
    void clear_DataNotificationFd( void );
    void close_DataNotificationFd( void );

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

    bool is_ShutDown(void);
     
    // END MRNET API

    Stream( Network * inetwork, int iid, Rank *ibackends, unsigned int inum_backends,
            int ius_filter_id, int isync_filter_id, int ids_filter_id );

    ~Stream();


    bool is_PeerClosed( Rank irank ) const;
    unsigned int num_ClosedPeers( void ) const ;
    bool is_Closed( void ) const;
    const std::set< Rank > & get_ClosedPeers( void ) const ;
    std::set< Rank > get_ChildPeers() const;
    void add_Stream_EndPoint(Rank irank);
    void add_Stream_Peer(Rank irank) ;

    PacketPtr collect_PerfData( perfdata_metric_t metric, 
                                perfdata_context_t context, 
                                int aggr_strm_id );

 private:
    
    int send_aux( int tag, const char *format_str, PacketPtr &packet );
    int send_aux_internal( int tag, const char *format_str, PacketPtr &packet );
    void add_IncomingPacket( PacketPtr );
    PacketPtr get_IncomingPacket( void );
    int push_Packet( PacketPtr, std::vector<PacketPtr> &, std::vector<PacketPtr> &, 
                     bool going_upstream );

    int send_FilterStateToParent( void ) const;
    PacketPtr get_FilterState( ) const;

    void set_FilterParams( FilterType, PacketPtr& ) const;

    bool enable_PerfData( perfdata_metric_t metric, perfdata_context_t context );
    bool disable_PerfData( perfdata_metric_t metric, perfdata_context_t context );
    void print_PerfData( perfdata_metric_t metric, perfdata_context_t context );

    bool remove_Node( Rank irank );
    bool recompute_ChildrenNodes( void );
    bool close_Peer( Rank irank );
    void signal_BlockedReceivers( void ) const;
    int block_ForIncomingPacket( void ) const;

    static bool find_FilterAssignment(const std::string& assignments, Rank me, int& filter_id);

    //Static Data Members
    Network * _network;
    unsigned int _id;
    int _sync_filter_id;
    Filter * _sync_filter;
    int _us_filter_id;
    Filter * _us_filter;
    int _ds_filter_id;
    Filter * _ds_filter;
    std::set< Rank > _end_points;  //end-points of stream

    //Dynamic Data Members
    EventPipe * _evt_pipe;
    PerfDataMgr * _perf_data;
    bool _us_closed;
    bool _ds_closed;
    bool _was_shutdown;
    std::set< Rank > _peers; //peers in stream
    std::set< Rank > _closed_peers;
    mutable XPlat::Mutex _peers_sync;

    std::list< PacketPtr > _incoming_packet_buffer;
    mutable XPlat::Monitor _incoming_packet_buffer_sync;
    mutable XPlat::Monitor _shutdown_sync;
    enum {PACKET_BUFFER_NONEMPTY};

};


} // namespace MRN

#endif /* __stream_h */
