/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

/*=======================================================*/
/*            Stream CLASS METHOD DEFINITIONS            */
/*=======================================================*/

#include <stdarg.h>
#include <set>
#include <sstream>

#include "mrnet/MRNet.h"
#include "PeerNode.h"
#include "mrnet/Stream.h"
#include "FrontEndNode.h"
#include "BackEndNode.h"
#include "Filter.h"
#include "Router.h"
#include "PerfDataEvent.h"
#include "PerfDataSysEvent.h"
#include "Protocol.h"
#include "utils.h"

using namespace std;

namespace MRN
{

#define STREAM_BASE_ID (1 << 30)

// a purely logical stream id
const unsigned int CTL_STRM_ID = STREAM_BASE_ID;

// some internally created streams
const unsigned int INTERNAL_STRM_BASE_ID = CTL_STRM_ID + 1;
const unsigned int TOPOL_STRM_ID = INTERNAL_STRM_BASE_ID;
const unsigned int PORT_STRM_ID  = INTERNAL_STRM_BASE_ID + 1;

// base stream id for all "real" stream objects
const unsigned int USER_STRM_BASE_ID = STREAM_BASE_ID + 10;

Stream::Stream( Network * inetwork, unsigned int iid,
                Rank *ibackends, unsigned int inum_backends,
                unsigned short ius_filter_id,
                unsigned short isync_filter_id,
                unsigned short ids_filter_id )
  : _perf_data( new PerfDataMgr() ),
     _network( inetwork ),
    _id( iid ),
    _sync_filter_id( isync_filter_id ),
    _us_filter_id( ius_filter_id ),
    _ds_filter_id( ids_filter_id ),
    _evt_pipe(NULL),
    _was_closed(false),
    _num_sending(0)
{

    set< PeerNodePtr > node_set;
    mrn_dbg( 3, mrn_printf(FLF, stderr,
                           "id:%u, us_filter:%hu, sync_id:%hu, ds_filter:%hu\n",
                           _id, _us_filter_id , _sync_filter_id, _ds_filter_id));

    _incoming_packet_buffer_sync.RegisterCondition( PACKET_BUFFER_NONEMPTY );
    _send_sync.RegisterCondition( STREAM_SEND_EMPTY );

    //parent nodes set up relevant downstream nodes 
    if( _network->is_LocalNodeParent() ) {

        if( ibackends != NULL ) {

            _end_points.insert( ibackends, ibackends + inum_backends );
            recompute_ChildrenNodes();
        }
        else {
            /* No endpoints: only valid for topol prop bcast streams 
             * created during network startup in backend attach.
             * Since bcast stream, add all children peers.
             */
            _peers_sync.Lock();
            _network->get_ChildPeers( _peers );
            _peers_sync.Unlock();        
        }
    }

    _sync_filter = new Filter(_network->GetFilterInfo(), isync_filter_id, this, FILTER_SYNC );
    _us_filter = new Filter(_network->GetFilterInfo(), ius_filter_id, this, FILTER_UPSTREAM );
    _ds_filter = new Filter(_network->GetFilterInfo(), ids_filter_id, this, FILTER_DOWNSTREAM);

    mrn_dbg_func_end();
}

// A stream is considered safe to delete when:
//
// (1) No threads are allowed to send on this stream anymore.
//
// (2) There are no threads in Stream::send_aux attempting to send something
// on this stream.
//
// (3) There exists no messages to be sent on behalf of this stream on any
// send queue.
//
// (4) There are no threads out there that hold a reference to this Stream
// where it will try to do some sort of private function.
//
// (3) and (4) are not satisfied right now.  We need shared_ptr for this!
Stream::~Stream()
{
    mrn_dbg_func_begin();

    _send_sync.Lock();
    // This satisfies (2)
    while(_num_sending != 0) {
        _send_sync.WaitOnCondition( STREAM_SEND_EMPTY );
    }
    // This satisfies (1)
    close();
    _send_sync.Unlock();

    mrn_dbg( 5, mrn_printf(FLF, stderr, "Deleting stream %u\n", _id) );

    if( _network->is_LocalNodeFrontEnd() ) {
        PacketPtr packet( new Packet(CTL_STRM_ID, PROT_DEL_STREAM, "%ud", _id) );
        if( _network->get_LocalFrontEndNode()->proc_deleteStream( packet ) == -1 ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "proc_deleteStream() failed\n"));
        }
    }

    _network->delete_Stream( _id );

    if( _sync_filter != NULL )
        delete _sync_filter;
    if( _us_filter != NULL )
        delete _us_filter;
    if( _ds_filter != NULL )
        delete _ds_filter;
    if( _perf_data != NULL )
        delete _perf_data;

    mrn_dbg_func_end();
}

void Stream::add_Stream_EndPoint( Rank irank )
{
    _peers_sync.Lock();
    _end_points.insert( irank );
    _peers_sync.Unlock();
}  

void Stream::add_Stream_Peer( Rank irank ) 
{
    PeerNodePtr outlet = _network->get_OutletNode( irank );
    if( outlet != NULL ) {
        _peers_sync.Lock();
        _peers.insert( outlet );
        _peers_sync.Unlock();
    }
}

int Stream::send( int itag, const char *iformat_str, ... )
{
    mrn_dbg_func_begin();

    va_list arg_list;
    va_start(arg_list, iformat_str);

    PacketPtr packet( new Packet(_network->get_LocalRank(), 
                                 _id, itag, iformat_str, arg_list) );
    va_end(arg_list);

    if( packet->has_Error() ){
        mrn_dbg(1, mrn_printf(FLF, stderr, "new packet() fail\n"));
        return -1;
    }

    int status = send( packet );

    mrn_dbg_func_end();
    return status;
}

int Stream::send( const char *idata_fmt, va_list idata, int itag )
{
    mrn_dbg_func_begin();

    PacketPtr packet( new Packet(_network->get_LocalRank(),
                                 _id, itag, idata_fmt, idata) );
    if( packet->has_Error() ){
        mrn_dbg(1, mrn_printf(FLF, stderr, "new packet() fail\n"));
        return -1;
    }

    int status = send( packet );

    mrn_dbg_func_end();
    return status;
}

int Stream::send( int itag, const void **idata, const char *iformat_str )
{
    mrn_dbg_func_begin();

    PacketPtr packet( new Packet(_network->get_LocalRank(),
                                 _id, itag, idata, iformat_str) );
    if( packet->has_Error() ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "new packet() fail\n"));
        return -1;
    }

    int status = send( packet );

    mrn_dbg_func_end();
    return status;
}

int Stream::send( PacketPtr& ipacket )
{
    mrn_dbg_func_begin();

    int status;

    if( ipacket->has_Error() ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "input packet has error\n"));
        return -1;
    }

    // make sure some things are set correctly, doesn't hurt if already set
    ipacket->set_SourceRank( _network->get_LocalRank() );
    ipacket->stream_id = _id;

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

    bool upstream = true;
    if( _network->is_LocalNodeFrontEnd() )
        upstream = false;

    status = send_aux( ipacket, upstream );

    mrn_dbg_func_end();
    return status;
}

int Stream::send_internal( int itag, const char *iformat_str, ... )
{
    mrn_dbg_func_begin();

    int status;
    va_list arg_list;
    va_start(arg_list, iformat_str);

    PacketPtr packet( new Packet(_network->get_LocalRank(), 
                                 _id, itag, iformat_str, arg_list) );
    va_end(arg_list);

    if( packet->has_Error() ) {
       mrn_dbg(1, mrn_printf(FLF, stderr, "new packet() fail\n"));
       return -1;
    }

    // internal sends always upstream
    bool upstream = true;
    status = send_aux( packet, upstream, true );

    mrn_dbg_func_end();
    return status;
}

bool Stream::is_LocalNodeInternal(void)
{
    return _network->is_LocalNodeInternal();
}

int Stream::send_aux( PacketPtr &ipacket, bool upstream, 
                      bool internal /* =false */ )
{
    mrn_dbg_func_begin();
    _send_sync.Lock();
    _num_sending++;
    _send_sync.Unlock();

    if( is_Closed() ) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "send on closed stream\n"));
        return 0;
    }
    
    vector< PacketPtr > opackets, opackets_reverse;

    unsigned int strm_id = ipacket->get_StreamId();
    if( strm_id < CTL_STRM_ID ) {
        // fast-path for BE specific stream ids
        // TODO: check id less than max BE rank
        opackets.push_back( ipacket );
    }
    else {
        // filter packet
        if( push_Packet(ipacket, opackets, opackets_reverse, upstream) == -1 ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "push_Packet() failed\n"));
            return -1;
        }
    }

    // send filtered result packets
    if( ! opackets.empty() ) {
        if( upstream ) {
            if( _network->send_PacketsToParent(opackets) == -1 ) {
                mrn_dbg(1, mrn_printf(FLF, stderr, "send_PacketsToParent() failed\n"));
                return -1;
            }
        }
        else {            
            if( _network->send_PacketsToChildren(opackets) == -1 ) {
                mrn_dbg(1, mrn_printf(FLF, stderr, "send_PacketsToChildren() failed\n"));
                return -1;
            }
        }
        opackets.clear();
    }
    if( ! opackets_reverse.empty() ) {
        if( ! internal ) {
            // FE or BE, return to sender
            for( unsigned int i = 0; i < opackets_reverse.size(); i++ ) {
                PacketPtr cur_packet( opackets_reverse[i] );

                mrn_dbg( 3, mrn_printf(FLF, stderr, "Put packet in stream %u\n",
                                       cur_packet->get_StreamId()) );
                add_IncomingPacket( cur_packet );
            }
        }
        else {
            if( upstream ) {
                if( _network->send_PacketsToParent(opackets) == -1 ) {
                    mrn_dbg(1, mrn_printf(FLF, stderr, "send_PacketsToParent() failed\n"));
                    return -1;
                }
            }
            else {
                if( _network->send_PacketsToChildren(opackets_reverse) == -1 ) {
                    mrn_dbg(1, mrn_printf(FLF, stderr, "send_PacketsToChildren() failed\n"));
                    return -1;
                }
            }
        }
    }

    _send_sync.Lock();
    _num_sending--;
    if(_num_sending == 0) {
        _send_sync.SignalCondition( STREAM_SEND_EMPTY );
    }
    _send_sync.Unlock();

    mrn_dbg_func_end();
    return 0;
}

void Stream::send_to_children( PacketPtr &ipacket )
{
    _peers_sync.Lock();

    std::set< PeerNodePtr >::const_iterator iter = _peers.begin(),
                                            iend = _peers.end();
    for( ; iter != iend; iter++ ) {

        PeerNodePtr cur_node = *iter;

        //Never send packet back to inlet
        if( cur_node->get_Rank() == ipacket->get_InletNodeRank() )
            continue;
                
        cur_node->send( ipacket );
    }
    
    _peers_sync.Unlock();
}

int Stream::flush() const
{
    int retval = 0;
    mrn_dbg_func_begin();

    if( is_Closed() )
        return -1;
    
    if( _network->is_LocalNodeFrontEnd() ) {

        _peers_sync.Lock();

        set< PeerNodePtr >::const_iterator iter = _peers.begin(),
                                           iend = _peers.end();
        for( ; iter != iend; iter++ ) {
            mrn_dbg( 5, mrn_printf(FLF, stderr,
                                   "child[%d].flush()\n",
                                   (*iter)->get_Rank()) );
            if( (*iter)->flush() == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "flush() failed\n") );
                retval = -1;
            }
        }
        _peers_sync.Unlock();
    }
    else if( _network->is_LocalNodeBackEnd() ){
        mrn_dbg( 5, mrn_printf(FLF, stderr, "backend flush()\n") );
        retval = _network->get_ParentNode()->flush();
    }

    mrn_dbg_func_end();
    return retval;
}

int Stream::recv( int *otag, PacketPtr & opacket, bool iblocking )
{
    mrn_dbg_func_begin();

    PacketPtr cur_packet = get_IncomingPacket();

    if( cur_packet != Packet::NullPacket ) {
        *otag = cur_packet->get_Tag();
        opacket = cur_packet;
	if( _evt_pipe != NULL )
	    _evt_pipe->clear();
        return 1;
    }

    if( is_Closed() )
        return 0;

    // at this point, no packets already available for this stream
    if( ! iblocking ) {
        return 0;
    }

    // block until this stream gets a packet
    if( _network->is_LocalNodeThreaded() ) {
        if( block_ForIncomingPacket() == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "stream[%u] block_ForIncomingPacket() failed.\n",
                                   _id ));
            return -1;
        }
    }
    else {
        // not threaded, keep receiving on network till stream has packet
        while( _incoming_packet_buffer.empty() ) {
            if( _network->recv( iblocking ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "FrontEnd::recv() failed\n" ));
                return -1;
            }
        }
    }

    // should have a packet now
    cur_packet = get_IncomingPacket();

    assert( cur_packet != Packet::NullPacket );
    *otag = cur_packet->get_Tag();
    opacket = cur_packet;
    if( _evt_pipe != NULL )
        _evt_pipe->clear();

    mrn_dbg(5, mrn_printf(FLF, stderr, "Received packet tag: %d\n", *otag ));
    mrn_dbg_func_end();
    return 1;
}

void Stream::signal_BlockedReceivers(void) const
{
    mrn_dbg( 5, mrn_printf(FLF, stderr, 
                           "stream[%u]: signaling \"got packet\"\n", _id) );

    // signal non-empty buffer condition
    _incoming_packet_buffer_sync.Lock();
    if( _evt_pipe != NULL )
        _evt_pipe->signal();
    _incoming_packet_buffer_sync.SignalCondition( PACKET_BUFFER_NONEMPTY );
    _incoming_packet_buffer_sync.Unlock();
}

int Stream::block_ForIncomingPacket(void) const
{
    // wait for non-empty buffer condition
    _incoming_packet_buffer_sync.Lock();
    while( _incoming_packet_buffer.empty() && ! is_Closed() ) {
        mrn_dbg( 5, mrn_printf(FLF, stderr, "stream[%u] blocking for a packet\n",
                               _id) );
        _incoming_packet_buffer_sync.WaitOnCondition( PACKET_BUFFER_NONEMPTY );
    }
    _incoming_packet_buffer_sync.Unlock();

    if( is_Closed() ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr, "stream[%u] is closed.\n", _id) );
        return -1;
    }
    else
        mrn_dbg( 5, mrn_printf(FLF, stderr, "stream[%u] has packet.\n", _id) );
    return 0;
}

bool Stream::close_Peer( Rank )
{
    mrn_dbg_func_begin();

    assert(!"DEPRECATED -- THIS SHOULD NEVER BE CALLED");

    return true;
}

bool Stream::is_Closed(void) const
{
    bool retval;

    _incoming_packet_buffer_sync.Lock();
    retval = _was_closed;
    _incoming_packet_buffer_sync.Unlock();

    return retval;
}

void Stream::close(void)
{
    _incoming_packet_buffer_sync.Lock();
    _was_closed = true;
    _incoming_packet_buffer_sync.Unlock();

    signal_BlockedReceivers();
}
                        

int Stream::push_Packet( PacketPtr ipacket,
                         vector< PacketPtr >& opackets,
                         vector< PacketPtr >& opackets_reverse,
                         bool igoing_upstream )
{
    vector< PacketPtr > ipackets;
    Timer tagg;

    mrn_dbg_func_begin();

    NetworkTopology* topol = _network->get_NetworkTopology();
    TopologyLocalInfo topol_info( topol,
                                  topol->find_Node(_network->get_LocalRank()) );

    if( ipacket != Packet::NullPacket ) {
        ipackets.push_back(ipacket);
    }

    // if going upstream, sync first
    if( igoing_upstream ) {

        tagg.start();
        if( _sync_filter->push_Packets(ipackets, opackets, opackets_reverse, topol_info ) == -1 ){
            mrn_dbg(1, mrn_printf(FLF, stderr, "SyncFilt.push_packets() failed\n"));
            return -1;
        }
        tagg.stop();

        // NOTE: ipackets is cleared by Filter::push_Packets()
        if( opackets.size() != 0 ){
            ipackets = opackets;
            opackets.clear();
        }

        // performance data update for FILTER_OUT
        if( _perf_data->is_Enabled(PERFDATA_MET_ELAPSED_SEC, PERFDATA_CTX_SYNCFILT_OUT) ) {
            perfdata_t val;
            val.d = tagg.get_latency_secs();
            _perf_data->add_DataInstance( PERFDATA_MET_ELAPSED_SEC, 
                                          PERFDATA_CTX_SYNCFILT_OUT,
                                          val );
        }
            
    }

    if( ipackets.size() > 0 ) {

	long user_before, sys_before;
	long user_after, sys_after;

        Filter* trans_filter = _ds_filter;

        if( igoing_upstream ) {
            trans_filter = _us_filter;

            // performance data update for FILTER_IN
            if( _perf_data->is_Enabled(PERFDATA_MET_NUM_PKTS, PERFDATA_CTX_FILT_IN) ) {
                perfdata_t val;
                val.u = ipackets.size();
                _perf_data->add_DataInstance( PERFDATA_MET_NUM_PKTS, 
                                              PERFDATA_CTX_FILT_IN,
                                              val );
            }

            if( _perf_data->is_Enabled(PERFDATA_MET_CPU_USR_PCT, PERFDATA_CTX_FILT_OUT)  ||
                _perf_data->is_Enabled(PERFDATA_MET_CPU_SYS_PCT, PERFDATA_CTX_FILT_OUT) ) {
                PerfDataSysMgr::get_ThreadTime(user_before,sys_before);
	    }
        }

        // run transformation filter
        tagg.start();
        if( trans_filter->push_Packets(ipackets, opackets, opackets_reverse, topol_info ) == -1 ){
            mrn_dbg(1, mrn_printf(FLF, stderr, "TransFilt.push_packets() failed\n"));
            return -1;
        }
        tagg.stop();

        if( igoing_upstream ) {

            // performance data update for FILTER_OUT
            if( _perf_data->is_Enabled(PERFDATA_MET_CPU_USR_PCT, PERFDATA_CTX_FILT_OUT)  ||
                _perf_data->is_Enabled(PERFDATA_MET_CPU_SYS_PCT, PERFDATA_CTX_FILT_OUT) ) {
                PerfDataSysMgr::get_ThreadTime(user_after,sys_after);
            }

            if( _perf_data->is_Enabled(PERFDATA_MET_NUM_PKTS, PERFDATA_CTX_FILT_OUT) ) {
                perfdata_t val;
                val.u = opackets.size() + opackets_reverse.size();
                _perf_data->add_DataInstance( PERFDATA_MET_NUM_PKTS, 
                                              PERFDATA_CTX_FILT_OUT,
                                              val );
            }
            if( _perf_data->is_Enabled(PERFDATA_MET_ELAPSED_SEC, PERFDATA_CTX_FILT_OUT) ) {
                perfdata_t val;
                val.d = tagg.get_latency_secs();
                _perf_data->add_DataInstance( PERFDATA_MET_ELAPSED_SEC, 
                                              PERFDATA_CTX_FILT_OUT,
                                              val );
            }
            if( _perf_data->is_Enabled(PERFDATA_MET_CPU_USR_PCT, PERFDATA_CTX_FILT_OUT) ) {
                perfdata_t val;
                double diff = (double)(user_after  - user_before) ;   
                val.d = ( diff / tagg.get_latency_msecs() ) * 100.0;
                _perf_data->add_DataInstance( PERFDATA_MET_CPU_USR_PCT, 
                                              PERFDATA_CTX_FILT_OUT,
                                              val );
            }
            if( _perf_data->is_Enabled(PERFDATA_MET_CPU_SYS_PCT, PERFDATA_CTX_FILT_OUT) ) {
                perfdata_t val;
                double diff = (double)(sys_after  - sys_before) ;   
                val.d = ( diff / tagg.get_latency_msecs() ) * 100.0;
                _perf_data->add_DataInstance( PERFDATA_MET_CPU_SYS_PCT, 
                                              PERFDATA_CTX_FILT_OUT,
                                              val );
            }
        }
    }

    mrn_dbg_func_end();
    return 0;
}

PacketPtr Stream::get_IncomingPacket( )
{
    PacketPtr cur_packet( Packet::NullPacket );

    _incoming_packet_buffer_sync.Lock();
    if( ! _incoming_packet_buffer.empty() ){
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
    // cache a valid Network pointer for later
    Network* net = _network;

    // push packet and notify posted Stream::recv()s
    _incoming_packet_buffer_sync.Lock();
    _incoming_packet_buffer.push_back( ipacket );
    _incoming_packet_buffer_sync.Unlock();
    signal_BlockedReceivers();

    // notify any posted Network::recv()s
    /* Note: the following should never be moved inside the Lock/Unlock 
     *       above as it can (and has) cause a circular dependency deadlock.
     *       we use the cached net because by the time we return from
     *       signaling, someone may have deleted this stream */ 
    net->signal_NonEmptyStream( this );
}

unsigned int Stream::size(void) const
{
    return (unsigned int)_end_points.size();
}

unsigned int Stream::get_Id(void) const 
{
    return _id;
}

const set<Rank> & Stream::get_EndPoints(void) const
{
    return _end_points;
}

bool Stream::has_Data(void)
{
    _incoming_packet_buffer_sync.Lock();
    bool empty = _incoming_packet_buffer.empty();
    _incoming_packet_buffer_sync.Unlock();

    return !empty;
}

int Stream::get_DataNotificationFd(void)
{
    if( _evt_pipe == NULL ) {
	_evt_pipe = new EventPipe;
    }
    return _evt_pipe->get_ReadFd();
}

void Stream::clear_DataNotificationFd(void)
{
    if( _evt_pipe != NULL ) {
	_evt_pipe->clear();
    }
}

void Stream::close_DataNotificationFd(void)
{
    if( _evt_pipe != NULL ) {
	delete _evt_pipe;
	_evt_pipe = NULL;
    }
}

int Stream::set_FilterParameters( const char *iparams_fmt, va_list iparams, 
				  FilterType iftype ) const
{
    int tag;
    switch( iftype ) {
     case FILTER_DOWNSTREAM_TRANS:
	 tag = PROT_SET_FILTERPARAMS_DOWNSTREAM;
	 break;
     case FILTER_UPSTREAM_TRANS:
	 tag = PROT_SET_FILTERPARAMS_UPSTREAM_TRANS;
	 break;
     case FILTER_UPSTREAM_SYNC:
	 tag = PROT_SET_FILTERPARAMS_UPSTREAM_SYNC;
	 break;
     default:
	 assert(!"bad filter type");
    }
    
    if( _network->is_LocalNodeFrontEnd() ) {

        PacketPtr packet( new Packet(_network->get_LocalRank(), 
                                     _id, tag, iparams_fmt, iparams) );
        if( packet->has_Error() ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "new packet() fail\n"));
            return -1;
        }

        ParentNode *parent = _network->get_LocalParentNode();
        if( parent == NULL ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "local parent is NULL\n"));
            return -1;
        }
	if( parent->proc_FilterParams( iftype, packet ) == -1 ) {
	     mrn_dbg( 1, mrn_printf(FLF, stderr, "FrontEnd::recv() failed\n" ));
	     return -1;
	}
    }
    return 0;
}

int Stream::set_FilterParameters( FilterType iftype, const char *iformat_str, ... ) const
{
    int rc;

    mrn_dbg_func_begin();
    
    va_list arg_list;
    va_start(arg_list, iformat_str);
    
    rc = set_FilterParameters( iformat_str, arg_list, iftype );

    va_end(arg_list);

    mrn_dbg_func_end();

    return rc;
}

void Stream::set_FilterParams( FilterType itype, PacketPtr &iparams ) const
{
    switch( itype ) {
     case FILTER_DOWNSTREAM_TRANS:
         _ds_filter->set_FilterParams(iparams);
	 break;
     case FILTER_UPSTREAM_TRANS:
	 _us_filter->set_FilterParams(iparams);
	 break;
     case FILTER_UPSTREAM_SYNC:
	 _sync_filter->set_FilterParams(iparams);
	 break;
     default:
	 assert(!"bad filter type");
    }
}

PacketPtr Stream::get_FilterState(void) const
{
    mrn_dbg_func_begin();
    PacketPtr packet ( _us_filter->get_FilterState( _id ) );
    mrn_dbg_func_end();
    return packet;
}

int Stream::send_FilterStateToParent(void) const
{
    mrn_dbg_func_begin();
    PacketPtr packet = get_FilterState( );

    if( packet != Packet::NullPacket ){
        _network->get_ParentNode()->send( packet );
    }

    mrn_dbg_func_end();
    return 0;
}

void Stream::remove_Node( Rank irank )
{
    if( is_Closed() )
        return;

    _peers_sync.Lock();

    set< PeerNodePtr >::const_iterator iter = _peers.begin(),
                                       iend = _peers.end();
    for( ; iter != iend; iter++ ) {
        Rank cur_rank = (*iter)->get_Rank();
        if( cur_rank == irank ) {
            if( _network->is_LocalNodeFrontEnd() &&
                ! _network->recover_FromFailures() ) {
                /* if failure recovery is off, we can't expect the BEs that were in
                   the subtree of the failed peer to re-connect, so the stream is now 
                   broken, and we should tell the user by closing it */
                _peers.clear();
                close();
            }
            else {
                _peers.erase( iter );
            }
            break;
        }
    }

    _peers_sync.Unlock();
}

void Stream::recompute_ChildrenNodes(void)
{
    if( is_Closed() )
        return;

    _peers_sync.Lock();
    _peers.clear();

    set< Rank >::const_iterator iter;
    for( iter = _end_points.begin(); iter != _end_points.end(); iter++ ) {
        Rank cur_rank = *iter;
        PeerNodePtr outlet = _network->get_OutletNode( cur_rank );
        if( outlet != NULL ) {
            mrn_dbg( 5, mrn_printf(FLF, stderr,
                                   "Adding outlet[%d] for endpoint[%d] to stream[%u].\n",
                                   outlet->get_Rank(), cur_rank, _id) );
            _peers.insert( outlet );
        }
        else
            mrn_dbg( 5, mrn_printf(FLF, stderr,
                                   "No outlet found for endpoint[%d] in stream[%u].\n",
                                   cur_rank, _id) );
    }
    _peers_sync.Unlock();
}

void Stream::get_ChildRanks( set< Rank >& peers ) const
{
    _peers_sync.Lock();

    set< PeerNodePtr >::const_iterator iter = _peers.begin(),
                                       iend = _peers.end();
    for( ; iter != iend; iter++ ) {
        Rank cur_rank = (*iter)->get_Rank();
        peers.insert( cur_rank );
    }

    _peers_sync.Unlock();
}

void Stream::get_ChildPeers( set< PeerNodePtr >& peers ) const
{
    _peers_sync.Lock();
    peers = _peers;
    _peers_sync.Unlock();
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

    if (metric == PERFDATA_MET_MEM_VIRT_KB || metric == PERFDATA_MET_MEM_PHYS_KB) {
    	_perf_data->get_MemData(metric);
    }

    _perf_data->collect( metric, context, data );
    iter = data.begin();

    Rank my_rank = _network->get_LocalRank();
    uint32_t num_elems = (uint32_t) data.size();
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
    aggr_strm->set_FilterParameters( FILTER_UPSTREAM_TRANS, "%d %d %ud %ud", 
                                     (int)metric, (int)context, 
                                     aggr_filter_id, _id );
    int aggr_strm_id = aggr_strm->get_Id();

    if( _network->is_LocalNodeFrontEnd() ) {

        // send collect request
        int tag = PROT_COLLECT_PERFDATA;
        PacketPtr packet( new Packet( _id, tag, "%d %d %ud", 
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
        uint32_t rank_len, nelems_len;
	int data_len;
        void* data_arr;
        const char* fmt = NULL;
        perfdata_mettype_t mettype = PerfDataMgr::get_MetricType(metric);
        switch( mettype ) {
         case PERFDATA_TYPE_UINT: {
             fmt = "%ad %ad %auld";
             uint64_t* u64_arr;
	     resp_packet->unpack( fmt, &rank_arr, &rank_len, 
                                  &nelems_arr, &nelems_len,
                                  &u64_arr, &data_len );
             data_arr = u64_arr;
             break;
         }
         case PERFDATA_TYPE_INT: {
             fmt = "%ad %ad %ald"; 
             int64_t* i64_arr;
	     resp_packet->unpack( fmt, &rank_arr, &rank_len, 
                                  &nelems_arr, &nelems_len,
                                  &i64_arr, &data_len );
             data_arr = i64_arr;
             break;
         }
         case PERFDATA_TYPE_FLOAT: {
             fmt = "%ad %ad %alf"; 
             double* dbl_arr;
	     resp_packet->unpack( fmt, &rank_arr, &rank_len, 
                                  &nelems_arr, &nelems_len,
                                  &dbl_arr, &data_len );
             data_arr = dbl_arr;
             break;
         }
         default:
             mrn_dbg(1, mrn_printf(FLF, stderr, "bad metric type\n"));  
             return false;          
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

    mrn_dbg_func_begin();

    if (metric == PERFDATA_MET_MEM_VIRT_KB || metric == PERFDATA_MET_MEM_PHYS_KB) {
    	_perf_data->get_MemData(metric);
    }	

    _perf_data->print( metric, context );

    mrn_dbg_func_end();
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

// straightforward filter assignment parser, may replace this with yacc later
// returns -1 on parse error, 0 on success
static int simple_parser(const string& s, map< int, vector<int> >& m)
{
    if( ! s.length() )
        return -1;

    vector<string> assignments;
    size_t semi, tmp, start=0;
    string arrow("=>");
    do {
        semi = s.find_first_of(';', start + arrow.length());
        if( semi != s.npos ) {

            string a = s.substr(start, semi-start);
            assignments.push_back(a);

            start = semi + 1;
            if( start >= s.length() ) break;

            tmp = s.find( arrow, start );
            if( tmp == s.npos ) break; // no more assignments

        }
        else {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "missing semicolon - parse failed\n"));
            return -1;
        }
    } while(1);

    vector<string>::iterator si = assignments.begin();
    for( ; si != assignments.end() ; si++ ) {

        int filter, rank;
        string dummy, who;
        stringstream ss(stringstream::in | stringstream::out);
        ss << *si;

        ss >> filter;
        ss >> dummy;
        ss >> who;
        if( who.empty() ) {
            // no space between arrow and target
            tmp = dummy.find( arrow );
            if( tmp == dummy.npos ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "missing arrow - parse failed\n"));
                m.clear();
                return -1;
            }
            who = dummy.substr(tmp + arrow.length());
        }
        
        if( who == "*" )
            rank = -1;
        else
            rank = atoi( who.c_str() );
  
        vector<int>& filters = m[rank];
        filters.push_back(filter);
    }
    
    return 0; // success
}

bool Stream::find_FilterAssignment(const std::string& assignments, 
                                   Rank me, int& filter_id)
{
    std::map<int, vector<int> > rank_filters;
    if( simple_parser(assignments, rank_filters) != -1 ) {

        std::map<int, vector<int> >::iterator mine = rank_filters.find((int)me);
        if( mine != rank_filters.end() )
            filter_id = mine->second.front();
        else {
            // check for '*' catch-all case, stored as -1 rank
            mine = rank_filters.find(-1);
            if( mine != rank_filters.end() )
                filter_id = mine->second.front();
            else
                return false;
        }
        return true;
    }
    return false;
}


PerfDataMgr * Stream::get_PerfData(void)
{
    return _perf_data;
}

} // namespace MRN
