/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/


#include "utils_lightweight.h"
#include "Filter.h"
#include "PeerNode.h"
#include "Protocol.h"
#include "PerfDataEvent.h"
#include "PerfDataSysEvent.h"

#include "mrnet_lightweight/Stream.h"
#include "mrnet_lightweight/Network.h"
#include "mrnet_lightweight/NetworkTopology.h"
#include "xplat_lightweight/map.h"
#include "xplat_lightweight/vector.h"

#define STREAM_BASE_ID (1 << 30)

// a purely logical stream id
const unsigned int CTL_STRM_ID = STREAM_BASE_ID;

// some internally created streams
const unsigned int TOPOL_STRM_ID = STREAM_BASE_ID + 1;
const unsigned int PORT_STRM_ID  = STREAM_BASE_ID + 2;

// base stream id for all "real" stream objects
const unsigned int USER_STRM_BASE_ID = STREAM_BASE_ID + 10;

Stream_t* new_Stream_t(Network_t* net,
                       unsigned int iid, 
                       Rank * UNUSED(ibackends),
                       unsigned int UNUSED(inum_backends),
                       unsigned int ius_filter_id,
                       unsigned int isync_filter_id,
                       unsigned int ids_filter_id)
{
    Stream_t* new_stream = (Stream_t*) calloc( (size_t) 1, sizeof(Stream_t) );
    assert(new_stream);

    new_stream->network = net;
    new_stream->id = iid;

    new_stream->sync_filter_id = isync_filter_id;
    new_stream->sync_filter = new_Filter_t(isync_filter_id); 
    new_stream->us_filter_id = ius_filter_id;
    new_stream->us_filter = new_Filter_t(ius_filter_id); 
    new_stream->ds_filter_id = ids_filter_id;
    new_stream->ds_filter = new_Filter_t(ids_filter_id);

    new_stream->perf_data = new_PerfDataMgr_t();

    new_stream->incoming_packet_buffer = new_empty_vector_t();
    new_stream->_was_closed = 0;

    mrn_dbg(3, mrn_printf(FLF, stderr,
                          "id:%u, us_filter:%u, sync_id:%u, ds_filter:%u\n", 
                          new_stream->id, new_stream->us_filter_id, 
                          new_stream->sync_filter_id, new_stream->ds_filter_id));

    mrn_dbg_func_end();

    return new_stream;
}

void delete_Stream_t(Stream_t * stream)
{
    mrn_dbg_func_begin();

    /* remove the stream from Network streams map */
    Network_delete_Stream(stream->network, stream->id);

    /* clean up data structures */
    delete_vector_t( stream->incoming_packet_buffer );
    delete_PerfDataMgr_t( stream->perf_data );
    stream->incoming_packet_buffer = NULL;
    stream->perf_data = NULL;

    /* free the stream */
    free(stream);

    mrn_dbg_func_end();
}

unsigned int Stream_get_Id(Stream_t* stream)
{
    return stream->id;
}

int Stream_find_FilterAssignment(char* UNUSED(assignments), Rank UNUSED(me), int UNUSED(filter_id))
{
    // THIS IS A STUB
    int stub = 1;
    assert(!stub);
    return -1;
}

Packet_t* Stream_get_IncomingPacket(Stream_t* stream)
{
    Packet_t* cur_packet = NULL;
    perfdata_t val;

    mrn_dbg_func_begin();

    if (stream->incoming_packet_buffer->size > 0) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "incoming_packet_buffer->size=%d\n", 
                              stream->incoming_packet_buffer->size));
        cur_packet = (Packet_t*)(stream->incoming_packet_buffer->vec[0]);
        stream->incoming_packet_buffer = eraseElement(stream->incoming_packet_buffer, 
                                                      cur_packet);
        mrn_dbg(5, mrn_printf(FLF, stderr, "incoming_packet_buffer->size now=%d\n", 
                              stream->incoming_packet_buffer->size));

        //performance data update for STREAM_RECV
        if (PerfDataMgr_is_Enabled(stream->perf_data, 
                                   PERFDATA_MET_NUM_PKTS,
                                   PERFDATA_CTX_RECV)) {
            val = PerfDataMgr_get_DataValue(stream->perf_data,
                                            PERFDATA_MET_NUM_PKTS,
                                            PERFDATA_CTX_RECV);
            val.u += 1;
            PerfDataMgr_set_DataValue(stream->perf_data,
                                      PERFDATA_MET_NUM_PKTS,
                                      PERFDATA_CTX_RECV,
                                      val);
        }
        if (PerfDataMgr_is_Enabled(stream->perf_data, 
                                   PERFDATA_MET_NUM_BYTES,
                                   PERFDATA_CTX_RECV)) {
            val = PerfDataMgr_get_DataValue(stream->perf_data,
                                            PERFDATA_MET_NUM_BYTES,
                                            PERFDATA_CTX_RECV);
            val.u += cur_packet->buf_len;
            PerfDataMgr_set_DataValue(stream->perf_data,
                                      PERFDATA_MET_NUM_BYTES,
                                      PERFDATA_CTX_RECV,
                                      val);
        }
    }

    if (cur_packet == NULL) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "cur_packet==NULL\n"));
    } else {
        mrn_dbg(5, mrn_printf(FLF, stderr, "cur_packet->tag=%d\n", cur_packet->tag));
    }

    mrn_dbg_func_end();
    return cur_packet;
}

int Stream_push_Packet(Stream_t* stream,
                       Packet_t* ipacket,
                       vector_t *  opackets,
                       vector_t * opackets_reverse,
                       int igoing_upstream)
{

    NetworkTopology_t* topol = stream->network->network_topology;
    TopologyLocalInfo_t* topol_info = 
      new_TopologyLocalInfo_t(topol, NetworkTopology_find_Node(topol, stream->network->local_rank));

    long user_before, sys_before;
    long user_after, sys_after;

    Timer_t* tagg = new_Timer_t();
    Filter_t* trans_filter = stream->ds_filter;
    
    perfdata_t val;
    double diff;

    vector_t * ipackets = new_empty_vector_t();
    if (ipacket)
        pushBackElement(ipackets, ipacket);
    
    mrn_dbg_func_begin();

    if (igoing_upstream) {
        trans_filter = stream->us_filter;

        // performance data update for FILTER_IN
        if (PerfDataMgr_is_Enabled(stream->perf_data, 
                                   PERFDATA_MET_NUM_PKTS, 
                                   PERFDATA_CTX_FILT_IN)) {
            val.u = 1; // ipacket.size()
            PerfDataMgr_add_DataInstance(stream->perf_data,
                                         PERFDATA_MET_NUM_PKTS, 
                                         PERFDATA_CTX_FILT_IN, 
                                         val);
        }
        if (PerfDataMgr_is_Enabled(stream->perf_data, 
                                   PERFDATA_MET_CPU_USR_PCT, 
                                   PERFDATA_CTX_FILT_OUT) || 
            PerfDataMgr_is_Enabled(stream->perf_data, 
                                   PERFDATA_MET_CPU_SYS_PCT, 
                                   PERFDATA_CTX_FILT_OUT)) {
            PerfDataSysMgr_get_ThreadTime(&user_before, &sys_before);
        }
        Timer_start(tagg);
    }

    // run transformation filter
    // NOTE: Currently, we do not support filtering at lightweight
    // backend nodes. Thus, nothing happens during this process
    // except opackets[0] = ipacket;
    if (Filter_push_Packets(trans_filter, 
                            ipackets, 
                            opackets,
                            opackets_reverse, 
                            topol_info,
                            igoing_upstream) == -1) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Filter_push_packets() failed\n"));
        return -1;
    }

    if (igoing_upstream) {
        Timer_stop(tagg);
        if (PerfDataMgr_is_Enabled(stream->perf_data, 
                                   PERFDATA_MET_CPU_USR_PCT, 
                                   PERFDATA_CTX_FILT_OUT) || 
            PerfDataMgr_is_Enabled(stream->perf_data, 
                                   PERFDATA_MET_CPU_SYS_PCT, 
                                   PERFDATA_CTX_FILT_OUT)) {
            PerfDataSysMgr_get_ThreadTime(&user_after, &sys_after);
        }

        // performance data update for FILTER_OUT
        if( PerfDataMgr_is_Enabled(stream->perf_data,  
                                   PERFDATA_MET_NUM_PKTS, 
                                   PERFDATA_CTX_FILT_OUT ) ) {
            //val.u = opacket.size() + opacket_reverse.size();
            val.u = 2;
            PerfDataMgr_add_DataInstance(stream->perf_data, 
                                         PERFDATA_MET_NUM_PKTS, 
                                         PERFDATA_CTX_FILT_OUT,
                                         val);
        }
        if( PerfDataMgr_is_Enabled(stream->perf_data,  
                                   PERFDATA_MET_ELAPSED_SEC, 
                                   PERFDATA_CTX_FILT_OUT ) ) {
            val.d = Timer_get_latency_secs(tagg);
            PerfDataMgr_add_DataInstance(stream->perf_data, 
                                         PERFDATA_MET_ELAPSED_SEC, 
                                         PERFDATA_CTX_FILT_OUT,
                                         val);
        }
        if( PerfDataMgr_is_Enabled(stream->perf_data,  
                                   PERFDATA_MET_CPU_USR_PCT, 
                                   PERFDATA_CTX_FILT_OUT ) ) {
            diff = (user_after  - user_before) ;   
            val.d = ( diff / Timer_get_latency_msecs(tagg) ) * 100.0;
            PerfDataMgr_add_DataInstance(stream->perf_data, 
                                         PERFDATA_MET_CPU_USR_PCT, 
                                         PERFDATA_CTX_FILT_OUT,
                                         val);
        }
        if( PerfDataMgr_is_Enabled(stream->perf_data,  
                                   PERFDATA_MET_CPU_SYS_PCT, 
                                   PERFDATA_CTX_FILT_OUT ) ) {
            diff = (sys_after  - sys_before) ;   
            val.d = ( diff / Timer_get_latency_msecs(tagg) ) * 100.0;
            PerfDataMgr_add_DataInstance(stream->perf_data, 
                                         PERFDATA_MET_CPU_SYS_PCT, 
                                         PERFDATA_CTX_FILT_OUT,
                                         val);
        }
    }

    delete_vector_t( ipackets );
    
    if( NULL != topol_info ) free( topol_info );
    if( NULL != tagg ) free( tagg );

    mrn_dbg_func_end();
    return 0;
}

int Stream_recv(Stream_t * stream, int *otag, Packet_t* opacket, bool_t blocking)
{
    int rc;
    Packet_t * cur_packet;
	
    mrn_dbg_func_begin();

    cur_packet = Stream_get_IncomingPacket(stream);

    if (cur_packet != NULL) {
        *otag = Packet_get_Tag(cur_packet);
        *opacket = *cur_packet; 
        mrn_dbg(5, mrn_printf(FLF, stderr, "cur_packet tag:%d\n", opacket->tag));
        
        // cur_packet fields now owned by opacket, just free the Packet_t
        free( cur_packet );
        return 1;
    }

    // Try to get packet for this stream
    do {
        rc = Network_recv_internal(stream->network, stream, blocking);
        if( rc == -1 ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "Network_recv_internal() failed\n"));
            return -1;
        }
	    else if( (blocking == false) && (rc == 0) ) {
            mrn_dbg_func_end();
            return 0;
	    }
    } while (stream->incoming_packet_buffer->size == 0);

    // we should have a packet now
    cur_packet = Stream_get_IncomingPacket(stream);

    assert(cur_packet);
    *otag = Packet_get_Tag(cur_packet);
    *opacket = *cur_packet;

    // cur_packet fields now owned by opacket, just free the Packet_t
    free( cur_packet );
        
    mrn_dbg(5, mrn_printf(FLF, stderr, "Received packet tag: %d\n", *otag));

    mrn_dbg_func_end();
    return 1;
}

void Stream_add_IncomingPacket(Stream_t* stream, Packet_t* ipacket)
{
    mrn_dbg_func_begin();
    pushBackElement(stream->incoming_packet_buffer, ipacket);
    mrn_dbg_func_end();
}

int Stream_send(Stream_t* stream, int itag, const char *iformat_str, ... )
{
    int status;
    va_list arg_list;
    Packet_t* packet;
  
    mrn_dbg_func_begin();

    va_start(arg_list, iformat_str);

    packet = new_Packet_t(stream->network->local_rank, 
                          stream->id, itag, iformat_str, arg_list);
    if( packet == NULL ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "new packet() failed\n"));
        return -1;
    }
    mrn_dbg(3, mrn_printf(FLF, stderr, "packet() succeeded.  Calling send_aux(\n"));

    va_end(arg_list);

    status = Stream_send_aux(stream, itag, iformat_str, packet);

    mrn_dbg_func_end();

    return status;
}

int Stream_send_packet(Stream_t* stream, Packet_t* packet)
{
    int status;
  
    mrn_dbg_func_begin();

    if( packet == NULL ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "invalid arg: NULL packet\n"));
        return -1;
    }
    mrn_dbg(3, mrn_printf(FLF, stderr, "Calling send_aux(\n"));

    status = Stream_send_aux(stream, packet->tag, packet->fmt_str, packet);

    mrn_dbg_func_end();

    return status;
}

int Stream_send_aux(Stream_t* stream, int itag, const char* ifmt, Packet_t* ipacket)
{
    Timer_t* tagg = new_Timer_t();
    Timer_t* tsend = new_Timer_t();
    vector_t* opackets = new_empty_vector_t();
    vector_t* opackets_reverse = new_empty_vector_t();
    Packet_t* opacket;
    int upstream = true;
    perfdata_t val;
    unsigned int i;

    mrn_dbg_func_begin();
    mrn_dbg(3, mrn_printf(FLF, stderr, "stream_id: %d, tag:%d, fmt=\"%s\"\n", 
                          stream->id, itag, ifmt));

    if( (stream->id >= CTL_STRM_ID) && (stream->id < USER_STRM_BASE_ID) )
        Packet_set_DestroyData( ipacket, true );
  
    // performance data update for STREAM_SEND
    if (PerfDataMgr_is_Enabled(stream->perf_data, 
                               PERFDATA_MET_NUM_PKTS,
                               PERFDATA_CTX_SEND)) {
        val = PerfDataMgr_get_DataValue(stream->perf_data,
                                        PERFDATA_MET_NUM_PKTS,
                                        PERFDATA_CTX_SEND);
        val.u += 1;
        PerfDataMgr_set_DataValue(stream->perf_data,
                                  PERFDATA_MET_NUM_PKTS,
                                  PERFDATA_CTX_SEND,
                                  val);
    }
    if (PerfDataMgr_is_Enabled(stream->perf_data,
                               PERFDATA_MET_NUM_BYTES,
                               PERFDATA_CTX_SEND)) {
        val = PerfDataMgr_get_DataValue(stream->perf_data,
                                        PERFDATA_MET_NUM_BYTES,
                                        PERFDATA_CTX_SEND);
        val.u += ipacket->buf_len;
        PerfDataMgr_set_DataValue(stream->perf_data,
                                  PERFDATA_MET_NUM_BYTES,
                                  PERFDATA_CTX_SEND,
                                  val);
    }

    // filter packet
    Timer_start(tagg);
    if (Stream_push_Packet(stream, ipacket, opackets, opackets_reverse, upstream) == -1) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Stream_push_Packet() failed\n"));
        return -1;
    }
    Timer_stop(tagg);

    //send filtered result packets
    Timer_start(tsend);
    for (i = 0; i < opackets->size; i++) {
        opacket = opackets->vec[i];
        if (opacket != NULL) {
            if( Network_send_PacketToParent(stream->network, opacket) == -1 ) {
                mrn_dbg(1, mrn_printf(FLF, stderr, "Network_send_PacketToParent failed\n"));
                return -1;
            }
            delete_Packet_t(opacket);
        }
    }
    Timer_stop(tsend);
    mrn_dbg(5, mrn_printf(FLF, stderr, "agg_lat: %.5lf send_lat: %.5lf\n", 
                          Timer_get_latency_msecs(tagg), 
                          Timer_get_latency_msecs(tsend)));

    delete_vector_t( opackets );
    delete_vector_t( opackets_reverse );

    mrn_dbg_func_end();
    return 0;
}

int Stream_flush(Stream_t* stream)
{
    int retval = 0;
    mrn_dbg_func_begin();

    mrn_dbg(5, mrn_printf(FLF, stderr, "calling backend flush()\n"));
    retval = PeerNode_flush(stream->network->parent);

    mrn_dbg_func_end();
    return retval;
}

void Stream_set_FilterParams(Stream_t* stream, int upstream, Packet_t* iparams)
{
    /* NOTE: Currently, we do not support filtering at lightweight
     * backend nodes, so this has no effect. */
    if( upstream ) {
        Filter_set_FilterParams(stream->us_filter, iparams);
    }
    else {
        Filter_set_FilterParams(stream->ds_filter, iparams);
    }
}

int Stream_enable_PerfData(Stream_t* stream,
                           perfdata_metric_t metric,
                           perfdata_context_t context)
{
    PerfDataMgr_enable(stream->perf_data, metric, context);
    return true;
}

int Stream_disable_PerfData(Stream_t* stream,
                            perfdata_metric_t metric,
                            perfdata_context_t context)
{
    PerfDataMgr_disable(stream->perf_data, metric, context);
    return true;
}

Packet_t* Stream_collect_PerfData(Stream_t* stream, 
                                  perfdata_metric_t metric,
                                  perfdata_context_t context,
                                  unsigned int aggr_strm_id)
{
    vector_t* data = new_empty_vector_t();
    unsigned int u, iter = 0;
    Rank my_rank;
    uint32_t num_elems;
    void* data_arr;
    const char* fmt;
    uint64_t* u64_arr;
    int64_t* i64_arr;
    double* dbl_arr;
    int* rank_arr;
    int* nelems_arr;
    Packet_t* packet;

    if (metric == PERFDATA_MET_MEM_VIRT_KB ||
        metric == PERFDATA_MET_MEM_PHYS_KB) {
        PerfDataMgr_get_MemData(stream->perf_data, metric);
    }

    PerfDataMgr_collect(stream->perf_data, metric, context, data);

    my_rank = stream->network->local_rank;
    num_elems = (uint64_t)data->size;
    data_arr = NULL;
    fmt = NULL;

    switch (PerfDataMgr_get_MetricType(stream->perf_data, metric)) {
        case PERFDATA_TYPE_UINT: {
            fmt = "%ad %ad %auld";
            if (num_elems) {
                u64_arr = (uint64_t*)malloc(sizeof(uint64_t*)*num_elems);
                if (u64_arr == NULL) {
                    mrn_dbg(1, mrn_printf(FLF, stderr, "malloc() data array failed\n"));
                    return NULL;
                }
                for (u = 0; iter != data->size; u++, iter++)
                    u64_arr[u] = ((perfdata_t*)data->vec[iter])->u;
                data_arr = u64_arr;
            }
            break;
        }

        case PERFDATA_TYPE_INT: {
            fmt = "%ad %ad %ald";
            if (num_elems) {
                i64_arr = (int64_t*)malloc(sizeof(int64_t)*num_elems);
                if (i64_arr == NULL) {
                    mrn_dbg(1, mrn_printf(FLF, stderr, "malloc() data array failed\n"));
                    return NULL;
                }
                for (u = 0; iter != data->size; u++, iter++)
                    i64_arr[u] = ((perfdata_t*)data->vec[iter])->i;

                data_arr = i64_arr;
            }
            break;
        }
        case PERFDATA_TYPE_FLOAT: {
            fmt = "%ad %ad %alf";
            if (num_elems) {
                dbl_arr = (double*)malloc(sizeof(double)*num_elems);
                if (dbl_arr == NULL) {
                    mrn_dbg(1, mrn_printf(FLF, stderr, "malloc() data array failed\n"));
                    return NULL;
                }
                for (u = 0; iter != data->size; u++, iter++)
                    dbl_arr[u] = ((perfdata_t*)data->vec[iter])->d;
                data_arr = dbl_arr;
            }
            break;
        }
        default:
            mrn_dbg(1, mrn_printf(FLF, stderr, "bad metric type\n"));
            return NULL;
    }

    // create output packet
    rank_arr = (int*)malloc(sizeof(int));
    assert(rank_arr);
    nelems_arr = (int*)malloc(sizeof(int));
    assert(nelems_arr);
    *rank_arr = my_rank;
    *nelems_arr = num_elems;
    packet = new_Packet_t_2(aggr_strm_id, PROT_COLLECT_PERFDATA,
                            fmt, rank_arr, 1, 
                            nelems_arr, 1, data_arr,
                            num_elems);
    if (packet == NULL) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "new packet() failed\n"));
        return NULL;
    }
   
    Packet_set_DestroyData( packet, 1 ); // free arrays when Packet no longer in use

    return packet;

}

void Stream_print_PerfData(Stream_t* stream,
                           perfdata_metric_t metric,
                           perfdata_context_t context)
{
    mrn_dbg_func_begin();

    if (metric == PERFDATA_MET_MEM_VIRT_KB || 
        metric == PERFDATA_MET_MEM_PHYS_KB) {
        PerfDataMgr_get_MemData(stream->perf_data, metric);
    }

    PerfDataMgr_print(stream->perf_data, metric, context);

    mrn_dbg_func_end();
}

int Stream_remove_Node(Stream_t* UNUSED(stream), Rank UNUSED(irank))
{
    // BEs don't keep track of stream peers
    return 1;
}

char Stream_is_Closed(Stream_t* stream)
{
    return stream->_was_closed;
}

//DEPRECATED -- renamed is_ShutDown to is_Closed
char Stream_is_Shutdown(Stream_t* stream)
{
    return Stream_is_Closed(stream);
}
