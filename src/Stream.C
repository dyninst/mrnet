/****************************************************************************
 *  Copyright 2003-2008 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

/*=======================================================*/
/*            Stream CLASS METHOD DEFINITIONS            */
/*=======================================================*/

#include <stdarg.h>
#include <set>

#include "mrnet/MRNet.h"
#include "PeerNode.h"
#include "mrnet/Stream.h"
#include "FrontEndNode.h"
#include "BackEndNode.h"
#include "Filter.h"
#include "Router.h"
#include "PerfDataEvent.h"
#include "Protocol.h"
#include "utils.h"

using namespace std;

namespace MRN
{

Stream::Stream( Network * inetwork,
                int iid,
                Rank *ibackends,
                unsigned int inum_backends,
                int ius_filter_id,
                int isync_filter_id,
                int ids_filter_id )
  : _network(inetwork),
    _id( iid ),
    _sync_filter_id(isync_filter_id),
    _sync_filter( new Filter( isync_filter_id ) ),
    _us_filter_id(ius_filter_id),
    _us_filter ( new Filter(ius_filter_id) ),
    _ds_filter_id(ids_filter_id),
    _ds_filter( new Filter(ids_filter_id) ),
    _perf_data( new PerfDataMgr() ),
    _us_closed(false),
    _ds_closed(false)
{
    set< PeerNodePtr > node_set;
    mrn_dbg( 3, mrn_printf(FLF, stderr,
                           "id:%d, us_filter:%d, sync_id:%d, ds_filter:%d\n",
                           _id, _us_filter_id , _sync_filter_id, _ds_filter_id));

    _incoming_packet_buffer_sync.RegisterCondition( PACKET_BUFFER_NONEMPTY );

    //parent nodes set up relevant downstream nodes 
    if( _network->is_LocalNodeParent() ) {
        //for each backend in the stream, we add the proper forwarding node
        for( unsigned int i = 0; i < inum_backends; i++ ) {
            mrn_dbg( 3, mrn_printf(FLF, stderr, "getting outlet for backend[%d] ... ",
                                   ibackends[i] ));
            _end_points.insert( ibackends[i] );
            PeerNodePtr outlet = _network->get_OutletNode( ibackends[i] );
            if( outlet != NULL ) {
                mrn_dbg( 3, mrn_printf(FLF, stderr,
                                       "Adding Endpoint %d to stream %d\n",
                                       ibackends[i], _id ));
                _peers.insert( outlet->get_Rank() );
            }
        }
    }

    mrn_dbg_func_end();
}
    
Stream::~Stream()
{
    if( _network->is_LocalNodeFrontEnd() ) {
        PacketPtr packet( new Packet( 0, PROT_DEL_STREAM, "%d", _id ) );
        if( _network->get_LocalFrontEndNode()->proc_deleteStream( packet ) == -1 ) {
            mrn_dbg(2, mrn_printf(FLF, stderr, "proc_deleteStream() failed\n"));
        }
    }

    _network->delete_Stream( _id );
}

int Stream::send(int itag, const char *iformat_str, ...)
{
    mrn_dbg_func_begin();

    int status;
    va_list arg_list;
    va_start(arg_list, iformat_str);

    PacketPtr packet( new Packet(true, _id, itag, iformat_str, arg_list) );
    if( packet->has_Error() ){
        mrn_dbg(1, mrn_printf(FLF, stderr, "new packet() fail\n"));
        return -1;
    }
    mrn_dbg(3, mrn_printf(FLF, stderr, "packet() succeeded. Calling send_aux()\n"));

    va_end(arg_list);

    status = send_aux( itag, iformat_str, packet );

    mrn_dbg_func_end();
    return status;
}

int Stream::send(int itag, const void **idata, const char *iformat_str)
{
    mrn_dbg_func_begin();

    int status;

    PacketPtr packet( new Packet(_id, itag, idata, iformat_str) );
    if( packet->has_Error() ){
        mrn_dbg(1, mrn_printf(FLF, stderr, "new packet() fail\n"));
        return -1;
    }
    mrn_dbg(3, mrn_printf(FLF, stderr, "packet() succeeded. Calling send_aux()\n"));

    status = send_aux( itag, iformat_str, packet );

    mrn_dbg_func_end();
    return status;
}

int Stream::send_aux(int itag, char const *ifmt, PacketPtr &ipacket )
{


    mrn_dbg_func_begin();
    mrn_dbg(3, mrn_printf(FLF, stderr,
                          "stream_id: %d, tag:%d, fmt=\"%s\"\n", _id, itag, ifmt));

    Timer tagg, tsend;
    vector<PacketPtr> opackets, opackets_reverse;

    bool upstream = true;
    if( _network->is_LocalNodeFrontEnd() )
        upstream = false;

    // performance data update for STREAM_SEND
    if( _perf_data->is_Enabled( PERFDATA_MET_NUM_PKTS, PERFDATA_CTX_SEND ) ) {
        perfdata_t val = _perf_data->get_DataValue( PERFDATA_MET_NUM_PKTS, 
                                                   PERFDATA_CTX_SEND );
        val.u += 1;
        _perf_data->set_DataValue( PERFDATA_MET_NUM_PKTS, PERFDATA_CTX_SEND,
                                  val );
    }
    if( _perf_data->is_Enabled( PERFDATA_MET_NUM_BYTES, PERFDATA_CTX_SEND ) ) {
        perfdata_t val = _perf_data->get_DataValue( PERFDATA_MET_NUM_BYTES, 
                                                  PERFDATA_CTX_SEND );
        val.u += ipacket->get_BufferLen();
        _perf_data->set_DataValue( PERFDATA_MET_NUM_BYTES, PERFDATA_CTX_SEND,
                                  val );
    }

    // filter packet
    tagg.start();
    if( push_Packet( ipacket, opackets, opackets_reverse, upstream ) == -1){
        mrn_dbg(1, mrn_printf(FLF, stderr, "push_Packet() failed\n"));
        return -1;
    }
    tagg.stop();

    // send filtered result packets
    tsend.start();
    if( !opackets.empty() ) {
        if( _network->is_LocalNodeFrontEnd() ) {
            if( _network->send_PacketsToChildren( opackets ) == -1 ) {
                mrn_dbg(1, mrn_printf(FLF, stderr, "send_PacketsToChidlren() failed\n"));
                return -1;
            }
        }
        else {
            assert( _network->is_LocalNodeBackEnd() ) ;
            if( _network->send_PacketsToParent( opackets ) == -1 ) {
                mrn_dbg(1, mrn_printf(FLF, stderr, "send_PacketsToParent() failed\n"));
                return -1;
            }
        }
        opackets.clear();
    }
    if( !opackets_reverse.empty() ) {
        for( unsigned int i = 0; i < opackets_reverse.size( ); i++ ) {
            PacketPtr cur_packet( opackets_reverse[i] );

            mrn_dbg( 3, mrn_printf(FLF, stderr, "Put packet in stream %d\n",
                                   cur_packet->get_StreamId(  ) ));
            add_IncomingPacket( cur_packet );
        }
    }
    tsend.stop();

    mrn_dbg(5, mrn_printf(FLF, stderr, "agg_lat: %.5lf send_lat: %.5lf\n",
                          tagg.get_latency_msecs(), tsend.get_latency_msecs() ));
    mrn_dbg_func_end();
    return 0;
}

int Stream::flush() const
{
    int retval = 0;
    mrn_dbg_func_begin();

    if( _network->is_LocalNodeFrontEnd() ){
        set < Rank >::const_iterator iter;

        _peers_sync.Lock();
        for( iter=_peers.begin(); iter!=_peers.end(); iter++ ){
            mrn_dbg( 5, mrn_printf(FLF, stderr,
                                   "Calling children_nodes[%d].flush() ...\n",
                                   (*iter) ));
            PeerNodePtr cur_peer = _network->get_PeerNode( *iter );
            if( cur_peer != PeerNode::NullPeerNode ) {
                if( cur_peer->flush( ) == -1 ) {
                    mrn_dbg( 1, mrn_printf(FLF, stderr, "flush() failed\n" ));
                    retval = -1;
                }
                mrn_dbg( 5, mrn_printf(FLF, stderr, "flush() succeeded\n" ));
            }
        }
        _peers_sync.Unlock();
    }
    else if( _network->is_LocalNodeBackEnd() ){
        mrn_dbg( 5, mrn_printf(FLF, stderr, "calling backend flush()\n" ));
        retval = _network->get_ParentNode()->flush();
    }

    mrn_dbg_func_end();
    return retval;
}

int Stream::recv(int *otag, PacketPtr & opacket, bool iblocking)
{
    mrn_dbg_func_begin();

    PacketPtr cur_packet = get_IncomingPacket();

    if( cur_packet != Packet::NullPacket ) {
        *otag = cur_packet->get_Tag();
        opacket = cur_packet;
        return 1;
    }

    //At this point, no packets already available for this stream

    if( !iblocking ){
        return 0;
    }

    //We're gonna block till this stream gets a packet
    if( _network->is_LocalNodeThreaded() ) {
        if( block_ForIncomingPacket() == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "Stream[%u] block_ForIncomingPacket() failed.\n",
                                   _id ));
            return -1;
        }
    }
    else {
        // Not threaded, keep receiving on network till stream has packet
        while( _incoming_packet_buffer.empty() ) {
            if( _network->recv( iblocking ) == -1 ){
                mrn_dbg( 1, mrn_printf(FLF, stderr, "FrontEnd::recv() failed\n" ));
                return -1;
            }
        }
    }

    //we should have a packet now
    cur_packet = get_IncomingPacket();

    assert( cur_packet != Packet::NullPacket );
    *otag = cur_packet->get_Tag();
    opacket = cur_packet;

    mrn_dbg(5, mrn_printf(FLF, stderr, "Received packet tag: %d\n", *otag ));
    mrn_dbg_func_end();
    return 1;
}

void Stream::signal_BlockedReceivers( void ) const
{
    //wait for non-empty buffer condition
    _incoming_packet_buffer_sync.Lock();
    _incoming_packet_buffer_sync.SignalCondition( PACKET_BUFFER_NONEMPTY );
    _incoming_packet_buffer_sync.Unlock();
}

int Stream::block_ForIncomingPacket( void ) const
{
    //wait for non-empty buffer condition
    _incoming_packet_buffer_sync.Lock();
    while( _incoming_packet_buffer.empty() && !is_Closed() ){
        mrn_dbg( 5, mrn_printf(FLF, stderr, "Stream[%u](%p) blocking for a packet ...\n",
                               _id, this ));
        _incoming_packet_buffer_sync.WaitOnCondition( PACKET_BUFFER_NONEMPTY );
        mrn_dbg( 5, mrn_printf(FLF, stderr, "Stream[%u] signaled for a packet.\n",
                               _id ));
    }
    _incoming_packet_buffer_sync.Unlock();

    if( is_Closed() ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "Stream[%u] is closed.\n", _id));
        return -1;
    }

    return 0;
}

bool Stream::close_Peer( Rank irank )
{
    mrn_dbg_func_begin();

    _peers_sync.Lock();
    _closed_peers.insert( irank );
    _peers_sync.Unlock();

    //invoke sync filter to see if packets will now go thru
    vector<PacketPtr> opackets, opackets_reverse;
    bool going_toparent;

    if( _network->is_LocalNodeChild() &&
        _network->get_ParentNode()->get_Rank() == irank ) {
        going_toparent=true;
    }
    else{ 
        going_toparent=false;
    }

    if( push_Packet( Packet::NullPacket, opackets, opackets_reverse, going_toparent ) == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "push_Packet() failed\n" ));
        return false;
    }

    if( !opackets.empty() ) {
        if( _network->is_LocalNodeInternal() ) {
            //internal nodes must send packets up/down as appropriate
            if( going_toparent ) {
                if( _network->send_PacketsToParent( opackets ) == -1 ) {
                    mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n" ));
                    return false;
                }
            }
            else{
                if( _network->send_PacketsToChildren( opackets ) == -1 ) {
                    mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n" ));
                    return false;
                }
            }
        }
        else {
            //Root/leaf nodes simply add packets to streams for application to get
            for( unsigned int i = 0; i < opackets.size( ); i++ ) {
                mrn_dbg( 3, mrn_printf(FLF, stderr, "Put packet in stream %d\n", _id ));
                add_IncomingPacket( opackets[i] );
            }
        }
    }

    mrn_dbg_func_end();
    return true;
}

const std::set<Rank> & Stream::get_ClosedPeers( void ) const
{
    return _closed_peers;
}

bool Stream::is_Closed( void ) const
{
    bool retval = false;
    _peers_sync.Lock();

    if( _network->is_LocalNodeParent() ) {
        if( _peers.size() )
            retval = ( _closed_peers.size() == _peers.size() );
    }

    _peers_sync.Unlock();

    return retval;
}

bool Stream::is_PeerClosed( Rank irank ) const
{
    bool retval = false;

    _peers_sync.Lock();
    if( _closed_peers.find( irank ) != _closed_peers.end() ) {
        retval = true;
    }
    _peers_sync.Unlock();

    return retval;
}

unsigned int Stream::num_ClosedPeers( void ) const 
{
    unsigned int retval;

    _peers_sync.Lock();
    retval = _closed_peers.size();
    _peers_sync.Unlock();

    return retval;
}

int Stream::close( )
{
    flush();

    assert(!"Who the hell is calling this?");
    PacketPtr packet( new Packet( 0, PROT_CLOSE_STREAM, "%d", _id ) );

    int retval;
    if( _network->is_LocalNodeFrontEnd() ) {
        _ds_closed = true;
        retval = _network->send_PacketToChildren( packet );
    }
    else {
        _us_closed = true;
        retval = _network->send_PacketToParent( packet );
    }
 
    return retval;
}

int Stream::push_Packet( PacketPtr ipacket,
                         vector<PacketPtr>& opackets,
                         vector<PacketPtr>& opackets_reverse,
                         bool igoing_upstream )
{
    vector<PacketPtr> ipackets;
    
    mrn_dbg_func_begin();

    if( ipacket != Packet::NullPacket ) {
        ipackets.push_back(ipacket);
    }

    // if not back-end and going upstream, sync first
    if( !_network->is_LocalNodeBackEnd() && igoing_upstream ){
        if( _sync_filter->push_Packets(ipackets, opackets, opackets_reverse ) == -1){
            mrn_dbg(1, mrn_printf(FLF, stderr, "Sync.push_packets() failed\n"));
            return -1;
        }
        // NOTE: ipackets is cleared by Filter::push_Packets()
        if( opackets.size() != 0 ){
            ipackets = opackets;
            opackets.clear();
        }
        
    }

    if( ipackets.size() > 0 ) {

        /* NOTE: Performance data on filter CPU usage is currently not available, since
           getrusage doesn't play nicely with threads - need a better option */ 
        /* struct rusage before, after; */
        Timer tagg;
        Filter* cur_agg = _ds_filter;

        if( igoing_upstream ) {
            cur_agg = _us_filter;

            // performance data update for FILTER_IN
            if( _perf_data->is_Enabled( PERFDATA_MET_NUM_PKTS, PERFDATA_CTX_FILT_IN ) ) {
                perfdata_t val;
                val.u = ipackets.size();
                _perf_data->add_DataInstance( PERFDATA_MET_NUM_PKTS, PERFDATA_CTX_FILT_IN,
                                             val);
            }
            /* getrusage( RUSAGE_SELF, &before); */
            tagg.start();
        }

        // run transformation filter
        if( cur_agg->push_Packets(ipackets, opackets, opackets_reverse ) == -1){
            mrn_dbg(1, mrn_printf(FLF, stderr, "aggr.push_packets() failed\n"));
            return -1;
        }

        if( igoing_upstream ) {
            tagg.stop();
            /* getrusage( RUSAGE_SELF, &after); */
        
            // performance data update for FILTER_OUT
            if( _perf_data->is_Enabled( PERFDATA_MET_NUM_PKTS, PERFDATA_CTX_FILT_OUT ) ) {
                perfdata_t val;
                val.u = opackets.size() + opackets_reverse.size();
                _perf_data->add_DataInstance( PERFDATA_MET_NUM_PKTS, PERFDATA_CTX_FILT_OUT,
                                             val );
            }
            if( _perf_data->is_Enabled( PERFDATA_MET_ELAPSED_SEC, PERFDATA_CTX_FILT_OUT ) ) {
                perfdata_t val;
                val.d = tagg.get_latency_secs();
                _perf_data->add_DataInstance( PERFDATA_MET_ELAPSED_SEC, PERFDATA_CTX_FILT_OUT,
                                             val );
            }
            /* if( _perf_data->is_Enabled( PERFDATA_MET_CPU_USR_PCT, PERFDATA_CTX_FILT_OUT ) ) {
                perfdata_t val;
                val.d = 0.0;
                double diff = tv2dbl(after.ru_utime) - tv2dbl(before.ru_utime);
                if( diff < tagg.get_latency_secs )
                    val.d = ( diff / tagg.get_latency_secs() ) * 100.0;
                _perf_data->add_DataInstance( PERFDATA_MET_CPU_USR_PCT, PERFDATA_CTX_FILT_OUT,
                                             val );
            }
            if( _perf_data->is_Enabled( PERFDATA_MET_CPU_SYS_PCT, PERFDATA_CTX_FILT_OUT ) ) {
                perfdata_t val;
                double diff = tv2dbl(after.ru_stime) - tv2dbl(before.ru_time);
                if( diff < tagg.get_latency_secs )
                    val.d = ( diff / tagg.get_latency_secs() ) * 100.0;
                _perf_data->add_DataInstance( PERFDATA_MET_CPU_SYS_PCT, PERFDATA_CTX_FILT_OUT,
                                             val );
            } */
        }
    }

    mrn_dbg_func_end();
    return 0;
}

PacketPtr Stream::get_IncomingPacket( )
{
    PacketPtr cur_packet( Packet::NullPacket );

    _incoming_packet_buffer_sync.Lock();
    if( !_incoming_packet_buffer.empty() ){
        cur_packet = *( _incoming_packet_buffer.begin() );
        _incoming_packet_buffer.pop_front();

        // performance data update for STREAM_RECV
        if( _perf_data->is_Enabled( PERFDATA_MET_NUM_PKTS, PERFDATA_CTX_RECV ) ) {
            perfdata_t val = _perf_data->get_DataValue( PERFDATA_MET_NUM_PKTS, 
                                                       PERFDATA_CTX_RECV );
            val.u += 1;
            _perf_data->set_DataValue( PERFDATA_MET_NUM_PKTS, PERFDATA_CTX_RECV,
                                      val );
        }
        if( _perf_data->is_Enabled( PERFDATA_MET_NUM_BYTES, PERFDATA_CTX_RECV ) ) {
            perfdata_t val = _perf_data->get_DataValue( PERFDATA_MET_NUM_BYTES, 
                                                       PERFDATA_CTX_RECV );
            val.u += cur_packet->get_BufferLen();
            _perf_data->set_DataValue( PERFDATA_MET_NUM_BYTES, PERFDATA_CTX_RECV,
                                      val );
        }
    }
    _incoming_packet_buffer_sync.Unlock();

    return cur_packet;
}

void Stream::add_IncomingPacket( PacketPtr ipacket )
{
    _incoming_packet_buffer_sync.Lock();
    _incoming_packet_buffer.push_back( ipacket );
    mrn_dbg(5, mrn_printf(FLF, stderr, "Stream[%d](%p): signaling \"got packet\"\n",
                          _id, this));
    _incoming_packet_buffer_sync.SignalCondition( PACKET_BUFFER_NONEMPTY );
    _network->signal_NonEmptyStream();
    _incoming_packet_buffer_sync.Unlock();
}

unsigned int Stream::size( void ) const
{
    return _end_points.size();
}

unsigned int Stream::get_Id( void ) const 
{
    return _id;
}

const set<Rank> & Stream::get_EndPoints( void ) const
{
    return _end_points;
}

bool Stream::has_Data( void )
{
    _incoming_packet_buffer_sync.Unlock();
    bool empty = _incoming_packet_buffer.empty();
    _incoming_packet_buffer_sync.Unlock();

    return !empty;
}

int Stream::set_FilterParameters( const char *iparams_fmt, va_list iparams, bool iupstream ) const
{
    int tag = PROT_SET_FILTERPARAMS_DOWNSTREAM;
    if( iupstream )
       tag = PROT_SET_FILTERPARAMS_UPSTREAM;
    
    if( _network->is_LocalNodeFrontEnd() ) {

        PacketPtr packet( new Packet(true, _id, tag, iparams_fmt, iparams) );
        if( packet->has_Error() ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "new packet() fail\n"));
            return -1;
        }

        ParentNode *parent = _network->get_LocalParentNode();
        if( parent == NULL ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "local parent is NULL\n"));
            return -1;
        }
        if( iupstream ) {
            if( parent->proc_UpstreamFilterParams( packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "FrontEnd::recv() failed\n" ));
                return -1;
            }
        }
        else {
            if( parent->proc_DownstreamFilterParams( packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "FrontEnd::recv() failed\n" ));
                return -1;
            }
        }
    }
    return 0;
}

int Stream::set_FilterParameters( bool iupstream, const char *iformat_str, ... ) const
{
    int rc;

    mrn_dbg_func_begin();
    
    va_list arg_list;
    va_start(arg_list, iformat_str);
    
    rc = set_FilterParameters( iformat_str, arg_list, iupstream );

    va_end(arg_list);

    mrn_dbg_func_end();

    return rc;
}

void Stream::set_FilterParams( bool upstream, PacketPtr &iparams ) const
{
    if( upstream ) {
        _us_filter->set_FilterParams(iparams);
    }
    else {
        _ds_filter->set_FilterParams(iparams);
    }
}

PacketPtr Stream::get_FilterState( void ) const
{
    mrn_dbg_func_begin();
    PacketPtr packet ( _us_filter->get_FilterState( _id ) );
    mrn_dbg_func_end();
    return packet;
}

int Stream::send_FilterStateToParent( void ) const
{
    mrn_dbg_func_begin();
    PacketPtr packet = get_FilterState( );

    if( packet != Packet::NullPacket ){
        _network->get_ParentNode()->send( packet );
    }

    mrn_dbg_func_end();
    return 0;
}

bool Stream::remove_Node( Rank irank )
{
    _peers_sync.Lock();
    _peers.erase( irank );
    _peers_sync.Unlock();

    return true;;
}

bool Stream::recompute_ChildrenNodes( void )
{
    set < Rank >::const_iterator iter;

    _peers_sync.Lock();
    _peers.clear();
    for( iter=_end_points.begin(); iter!=_end_points.end(); iter++ ){
        Rank cur_rank=*iter;

        mrn_dbg( 3, mrn_printf(FLF, stderr, "Resetting outlet for backend[%d] ...\n",
                               cur_rank ));

        PeerNodePtr outlet = _network->get_OutletNode( cur_rank );
        if( outlet != NULL ) {
            mrn_dbg( 3, mrn_printf(FLF, stderr,
                                   "Adding outlet[%d] for node[%d] to stream[%d].\n",
                                   outlet->get_Rank(), cur_rank, _id ));
            _peers.insert( outlet->get_Rank() );
        }
    }
    _peers_sync.Unlock();

    return true;
}

set < Rank > Stream::get_ChildPeers() const
{
    return _peers;
}


bool Stream::enable_PerfData( perfdata_metric_t metric, 
                              perfdata_context_t context )
{
    _perf_data->enable( metric, context );
    return true;
}

bool Stream::enable_PerformanceData( perfdata_metric_t metric, 
                                     perfdata_context_t context )
{
    bool rc = false;

    mrn_dbg_func_begin();

    if( metric >= PERFDATA_MAX_MET )
        return false;
    if( context >= PERFDATA_MAX_CTX )
        return false;

    // send enable request
    if( _network->is_LocalNodeFrontEnd() ) {

        int tag = PROT_ENABLE_PERFDATA;
        PacketPtr packet( new Packet( _id, tag, "%d %d", (int)metric, (int)context) );
        if( packet->has_Error() ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "new packet() fail\n"));
            return false;
        }

        if( _network->is_LocalNodeParent() ) {
            if( _network->send_PacketToChildren( packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n" ));
                return false;
            }
        }

        // local enable
        rc = enable_PerfData( metric, context );
    }
    else {
        mrn_dbg( 1, mrn_printf(FLF, stderr, 
                 "enable_PerformanceData() can only be used by Network front-end\n" )); 
        return false;
    }

    mrn_dbg_func_end();
    return rc;
}

bool Stream::disable_PerfData( perfdata_metric_t metric, 
                               perfdata_context_t context )
{
    _perf_data->disable( metric, context );
    return true;
}

bool Stream::disable_PerformanceData( perfdata_metric_t metric, 
                                      perfdata_context_t context )
{
    bool rc = false;

    mrn_dbg_func_begin();
    
    if( metric >= PERFDATA_MAX_MET )
        return false;
    if( context >= PERFDATA_MAX_CTX )
        return false;

    // send disable request
    if( _network->is_LocalNodeFrontEnd() ) {

        int tag = PROT_DISABLE_PERFDATA;
        PacketPtr packet( new Packet( _id, tag, "%d %d", (int)metric, (int)context) );
        if( packet->has_Error() ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "new packet() fail\n"));
            return false;
        }

        if( _network->is_LocalNodeParent() ) {
            if( _network->send_PacketToChildren( packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n" ));
                return false;
            }
        }

        // local disable
        rc = disable_PerfData( metric, context );
    }
    else {
        mrn_dbg( 1, mrn_printf(FLF, stderr, 
                 "disable_PerformanceData() can only be used by Network front-end\n" )); 
        return false;
    }

    mrn_dbg_func_end();
    return rc;
}

PacketPtr Stream::collect_PerfData( perfdata_metric_t metric, 
                                    perfdata_context_t context,
				    int aggr_strm_id )
{
    vector< perfdata_t > data;
    vector< perfdata_t >::iterator iter;
    _perf_data->collect( metric, context, data );
    iter = data.begin();

    Rank my_rank = _network->get_LocalRank();
    unsigned num_elems = data.size();
    void* data_arr = NULL;
    const char* fmt = NULL;

    switch( PerfDataMgr::get_MetricType(metric) ) {

     case PERFDATA_TYPE_UINT: {
         fmt = "%ad %ad %auld";
         if( num_elems ) {
             uint64_t* u64_arr = (uint64_t*)malloc(sizeof(uint64_t) * num_elems);
             if( u64_arr == NULL ) {
                 mrn_dbg(1, mrn_printf(FLF, stderr, "malloc() data array failed\n"));
                 return Packet::NullPacket;
             }
             for( unsigned u=0; iter != data.end(); u++, iter++ )
                 u64_arr[u] = (*iter).u;
             data_arr = u64_arr;
         } 
         break;
     }

     case PERFDATA_TYPE_INT: {
         fmt = "%ad %ad %ald"; 
         if( num_elems ) {
             int64_t* i64_arr = (int64_t*)malloc(sizeof(int64_t) * num_elems);
             if( i64_arr == NULL ) {
                 mrn_dbg(1, mrn_printf(FLF, stderr, "malloc() data array failed\n"));
                 return Packet::NullPacket;
             }
             for( unsigned u=0; iter != data.end(); u++, iter++ )
                 i64_arr[u] = (*iter).i;
             data_arr = i64_arr;
         } 
         break;
     }

     case PERFDATA_TYPE_FLOAT: {
         fmt = "%ad %ad %alf"; 
         if( num_elems ) {
             double* dbl_arr = (double*)malloc(sizeof(double) * num_elems);
             if( dbl_arr == NULL ) {
                 mrn_dbg(1, mrn_printf(FLF, stderr, "malloc() data array failed\n"));
                 return Packet::NullPacket;
             }
             for( unsigned u=0; iter != data.end(); u++, iter++ )
                 dbl_arr[u] = (*iter).d;
             data_arr = dbl_arr;
         }
         break;
     }
     default:
         mrn_dbg(1, mrn_printf(FLF, stderr, "bad metric type\n"));  
         return Packet::NullPacket;          
    }

    // create output packet
    int* rank_arr = (int*) malloc( sizeof(int) );
    int* nelems_arr = (int*) malloc( sizeof(int) );
    *rank_arr = my_rank;
    *nelems_arr = num_elems;
    PacketPtr packet( new Packet( aggr_strm_id, PROT_COLLECT_PERFDATA, fmt, 
                                  rank_arr, 1, nelems_arr, 1, data_arr, num_elems ) );
    if( packet->has_Error() ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "new packet() fail\n"));
        return Packet::NullPacket;
    }
    packet->set_DestroyData(true); // free arrays when Packet no longer in use
    
    return packet;
}

bool Stream::collect_PerformanceData( rank_perfdata_map& results,
                                      perfdata_metric_t metric, 
                                      perfdata_context_t context,
                                      int aggr_filter_id /*=TFILTER_ARRAY_CONCAT*/
                                    )
{
    mrn_dbg_func_begin();
    if( metric >= PERFDATA_MAX_MET )
        return false;
    if( context >= PERFDATA_MAX_CTX )
        return false;

    // create stream to aggregate performance data
    Communicator* comm = new Communicator( _network, _end_points );
    Stream* aggr_strm = _network->new_Stream( comm, TFILTER_PERFDATA );
    aggr_strm->set_FilterParameters( true, "%d %d %d %d", 
                                     (int)metric, (int)context, 
                                     aggr_filter_id, _id );
    int aggr_strm_id = aggr_strm->get_Id();

    if( _network->is_LocalNodeFrontEnd() ) {

        // send collect request
        int tag = PROT_COLLECT_PERFDATA;
        PacketPtr packet( new Packet( _id, tag, "%d %d %d", 
                                      (int)metric, (int)context, aggr_strm_id ) );
        if( packet->has_Error() ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "new packet() fail\n"));
            return false;
        }

        if( _network->is_LocalNodeParent() ) {
            if( _network->send_PacketToChildren( packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n" ));
                return false;
            }
        }

        // receive data
        int rtag;
        PacketPtr resp_packet;
        aggr_strm->recv( &rtag, resp_packet );
        
        // unpack data
	int* rank_arr;
	int* nelems_arr;
        unsigned rank_len, nelems_len, data_len;
        unsigned data_elem_sz;
        void* data_arr;
        const char* fmt = NULL;
        perfdata_mettype_t mettype = PerfDataMgr::get_MetricType(metric);
        switch( mettype ) {
         case PERFDATA_TYPE_UINT: {
             fmt = "%ad %ad %auld";
             data_elem_sz = sizeof(uint64_t);
             uint64_t* u64_arr;
	     resp_packet->unpack( fmt, &rank_arr, &rank_len, 
                                  &nelems_arr, &nelems_len,
                                  &u64_arr, &data_len );
             data_arr = u64_arr;
             break;
         }
         case PERFDATA_TYPE_INT: {
             fmt = "%ad %ad %ald"; 
             data_elem_sz = sizeof(int64_t);
             int64_t* i64_arr;
	     resp_packet->unpack( fmt, &rank_arr, &rank_len, 
                                  &nelems_arr, &nelems_len,
                                  &i64_arr, &data_len );
             data_arr = i64_arr;
             break;
         }
         case PERFDATA_TYPE_FLOAT: {
             fmt = "%ad %ad %alf"; 
             data_elem_sz = sizeof(double);
             double* dbl_arr;
	     resp_packet->unpack( fmt, &rank_arr, &rank_len, 
                                  &nelems_arr, &nelems_len,
                                  &dbl_arr, &data_len );
             data_arr = dbl_arr;
             break;
         }
         default:
             mrn_dbg(1, mrn_printf(FLF, stderr, "bad metric type\n"));  
             return Packet::NullPacket;          
        }
        
        // add data to result map
        unsigned data_ndx = 0;
        for( unsigned u=0; u < rank_len ; u++ ) {
            unsigned nelems = nelems_arr[u];
            int rank = rank_arr[u];
            vector<perfdata_t>& rank_data = results[ rank ];
            rank_data.clear();
            rank_data.reserve( nelems );

            perfdata_t datum;
            switch( mettype ) {
             case PERFDATA_TYPE_UINT: {
                 uint64_t* u64_arr = (uint64_t*)data_arr + data_ndx;
                 for( unsigned v=0; v < nelems; v++ ) {
                     datum.u = u64_arr[v];
                     rank_data.push_back( datum );
                 }
                 break;
             }
             case PERFDATA_TYPE_INT: {
                 int64_t* i64_arr = (int64_t*)data_arr + data_ndx;
                 for( unsigned v=0; v < nelems; v++ ) {
                     datum.i = i64_arr[v];
                     rank_data.push_back( datum );
                 }
                 break;
             }
             case PERFDATA_TYPE_FLOAT: {
                 double* dbl_arr = (double*)data_arr + data_ndx;
                 for( unsigned v=0; v < nelems; v++ ) {
                     datum.d = dbl_arr[v];
                     rank_data.push_back( datum );
                 }
                 break;
             }
            }
            data_ndx += nelems;
        }
        if( rank_arr != NULL ) free( rank_arr );
        if( nelems_arr != NULL ) free( nelems_arr );
        if( data_arr != NULL ) free( data_arr );
    }
    else {
        mrn_dbg( 1, mrn_printf(FLF, stderr, 
                 "collect_PerformanceData() can only be used by Network front-end\n" )); 
        return false;
    }

    mrn_dbg_func_end();
    return true;
}

void Stream::print_PerfData( perfdata_metric_t metric, 
                             perfdata_context_t context )
{
    _perf_data->print( metric, context );
}

void Stream::print_PerformanceData( perfdata_metric_t metric, 
                                    perfdata_context_t context )
{
    mrn_dbg_func_begin();
    if( metric >= PERFDATA_MAX_MET )
        return;
    if( context >= PERFDATA_MAX_CTX )
        return;
    
    // send print request
    if( _network->is_LocalNodeFrontEnd() ) {

        int tag = PROT_PRINT_PERFDATA;
        PacketPtr packet( new Packet( _id, tag, "%d %d", (int)metric, (int)context) );
        if( packet->has_Error() ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "new packet() fail\n"));
            return;
        }

        if( _network->is_LocalNodeParent() ) {
            if( _network->send_PacketToChildren( packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n" ));
                return;
            }
        }

        // local print
        print_PerfData( metric, context );
    }
    else {
        mrn_dbg( 1, mrn_printf(FLF, stderr, 
                 "print_PerformanceData() can only be used by Network front-end\n" )); 
        return;
    }

    mrn_dbg_func_end();
}
    

} // namespace MRN
