/****************************************************************************
 *  Copyright 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/
#include <fcntl.h>

#include "pdr.h"
#include "utils.h"
#include "Message.h"
#include "PeerNode.h"
#include <time.h>
#include "mrnet/Types.h"
#include "mrnet/Stream.h"
#include "mrnet/Packet.h"
#include "xplat/Atomic.h"
#include "xplat/Error.h"
#include "xplat/NCIO.h"
#include "xplat/NetUtils.h"
#include "xplat/Types.h"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

namespace MRN
{

static XPlat::Atomic<uint64_t> MRN_bytes_send = 0;
static XPlat::Atomic<uint64_t> MRN_bytes_recv = 0;
uint64_t get_TotalBytesSend(void) { return MRN_bytes_send.Get(); }
uint64_t get_TotalBytesRecv(void) { return MRN_bytes_recv.Get(); }


Message::Message(Network * net):
    _net(net)
{
    uint32_t num_packets = 0;
    _packet_count_buf_len = (size_t) pdr_sizeof( (pdrproc_t)( pdr_uint32 ), &num_packets );
    _packet_sizes_buf_len = ((size_t)MESSAGE_PREALLOC_LEN * sizeof(uint64_t)) + 1;
    _ncbuf_len = (size_t) MESSAGE_PREALLOC_LEN;

    _packet_count_buf = (char*) malloc(_packet_count_buf_len);
    _packet_sizes_buf = (char*) malloc(_packet_sizes_buf_len);

    _packet_sync.RegisterCondition( MRN_QUEUE_NONEMPTY );
}

Message::~Message()
{
    if( _packet_count_buf != NULL )
        free(_packet_count_buf);
    if( _packet_sizes_buf != NULL )
        free(_packet_sizes_buf);
}

int Message::recv( XPlat::XPSOCKET sock_fd, std::list< PacketPtr > &packets_in,
                   Rank iinlet_rank )
{
    Timer t1;
    t1.start();

    ssize_t retval;
    size_t buf_len;
    uint64_t *packet_sizes;
    char *buf = NULL;
    XPlat::NCBuf* ncbufs;
    uint64_t len;
    unsigned int i, j;
    int rc = 0;
    int total_bytes = 0;
    uint32_t num_packets = 0, num_buffers;
    PDR pdrs;
    enum pdr_op op = PDR_DECODE;
    bool using_prealloc = true;

    retval = MRN_recv( sock_fd, _packet_count_buf, _packet_count_buf_len );
    if( retval != (ssize_t)_packet_count_buf_len ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr, "MRN_recv() %"PRIsszt" of %"PRIszt" bytes received\n", 
                               retval, _packet_count_buf_len));
        return -1;
    }

    pdrmem_create( &pdrs, _packet_count_buf, _packet_count_buf_len, op );

    if( ! pdr_uint32(&pdrs, &num_packets) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_uint32() failed\n") );
        return -1;
    }
    
    num_buffers = num_packets * 2;

    //
    // packet size vector
    //

    // buf_len's value is hardcoded, breaking pdr encapsulation barrier :(
    buf_len = (sizeof(uint64_t) * num_buffers) + 1;  // 1 byte pdr overhead
           
    if( num_buffers < _ncbuf_len ) {
        buf = _packet_sizes_buf;
        packet_sizes = _packet_sizes;
        ncbufs = _ncbuf;
    }
    else {
        using_prealloc = false;
        buf = (char*) malloc( buf_len );
        packet_sizes = (uint64_t*) malloc( sizeof(uint64_t) * num_buffers );
        ncbufs = new XPlat::NCBuf[num_buffers];
    }

    retval = MRN_recv( sock_fd, buf, buf_len );
    if( retval != (ssize_t)buf_len ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr, "MRN_recv() %"PRIsszt" of %"PRIsszt" bytes received\n", 
                               retval, buf_len ));
        rc = -1;
        goto recv_cleanup_return;
    }

    pdrmem_create( &pdrs, buf, buf_len, op );

    if( ! pdr_vector(&pdrs, (char*)packet_sizes, num_buffers,
                     sizeof(uint64_t), (pdrproc_t)pdr_uint64) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_vector() failed\n" ));
        rc = -1;
        goto recv_cleanup_return;
    }

    for( i = 0; i < num_buffers; i++ ) {
        len = packet_sizes[i];
        ncbufs[i].buf = (char*) malloc( len );
        ncbufs[i].len = len;
        total_bytes += len;
    }

    retval = XPlat::NCRecv( sock_fd, ncbufs, num_buffers );
    if( retval != total_bytes ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "NCRecv %"PRIsszt" of %"PRIsszt" bytes received\n", 
                               retval, total_bytes) );
        rc = -1;
        goto recv_cleanup_return;
    }

    //
    // post-processing
    //

    for( i = 0, j = 0; j < num_packets; i += 2, j++ ) {
        PacketPtr new_packet( new Packet(ncbufs[i].len,
                                         ncbufs[i].buf,
                                         ncbufs[i+1].len,
                                         ncbufs[i+1].buf,
                                         iinlet_rank) );

        if( new_packet->has_Error() ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "packet creation failed\n") );
            rc = -1;
            goto recv_cleanup_return;
        }
        packets_in.push_back( new_packet );
    }

 recv_cleanup_return:

    if( rc == -1 ) {
        for( unsigned u = 0; u < num_buffers; u++ )
            free( (void*)(ncbufs[u].buf) );
    }

    if( ! using_prealloc ) {
        free( buf );
        free( packet_sizes );
        delete[] ncbufs;
    }

    mrn_dbg_func_end();
    return rc;
}

int Message::send( XPlat::XPSOCKET sock_fd )
{
    ssize_t sret;
    size_t buf_len, total_bytes = 0;
    Stream * strm;
    uint64_t *packet_sizes = NULL;
    char *buf = NULL;
    XPlat::NCBuf* ncbufs;
    unsigned int i, j;
    int packetLength, rc = 0;
    uint32_t num_packets, num_buffers, num_ncbufs;
    PDR pdrs;
    enum pdr_op op = PDR_ENCODE;
    bool using_prealloc = true;
    bool go_away = false;

    std::list< PacketPtr > send_packets;
    std::list< PacketPtr >::iterator piter;

    _packet_sync.Lock();
    if( _packets.size() == 0 ) {   //nothing to do
        mrn_dbg( 3, mrn_printf(FLF, stderr, "Nothing to send!\n") );
        _packet_sync.Unlock();
        return 0;
    }
    send_packets = _packets;
    _packets.clear();
    _packet_sync.Unlock();

    piter = send_packets.begin();
    for( ; piter != send_packets.end(); piter++ ) {
        PacketPtr& pkt = *piter;
        strm = _net->get_Stream(pkt->get_StreamId());
        if( strm != NULL ) {
            if( strm->get_PerfData()->is_Enabled(PERFDATA_MET_ELAPSED_SEC, 
                                                 PERFDATA_PKT_SEND) ) {
                pkt->start_Timer(PERFDATA_PKT_TIMERS_SEND);
                pkt->stop_Timer(PERFDATA_PKT_TIMERS_FILTER_TO_SEND);
            }
        }
    }


    // Allocation (if required)
    num_packets = send_packets.size();
    num_buffers = num_packets * 2;
    num_ncbufs = num_buffers + 2;
    buf_len = ((size_t)num_buffers * sizeof(uint64_t)) + 1;  //1 extra bytes overhead

    if( num_ncbufs < _ncbuf_len ) {
        buf = _packet_sizes_buf;
        ncbufs = _ncbuf;
        packet_sizes = _packet_sizes;
    }
    else {
        using_prealloc = false;
        buf = (char*) malloc( buf_len );
        ncbufs = new XPlat::NCBuf[ num_ncbufs ];
        packet_sizes = (uint64_t*) malloc( sizeof(uint64_t) * num_buffers );
    }

    //
    // packets
    //
    piter = send_packets.begin();
    for( i = 0; piter != send_packets.end(); piter++, i += 2 ) {

        /* j accounts for skipping first two ncbufs that hold pkt count and sizes */
        j = i + 2;

        PacketPtr curPacket( *piter );
        
        /* check for final packet */
        int tag = curPacket->get_Tag();
        if( (tag == PROT_SHUTDOWN) || (tag == PROT_SHUTDOWN_ACK) )
            go_away = true;

        uint32_t hsz = curPacket->get_HeaderLen();
        uint64_t dsz = curPacket->get_BufferLen();

        if( hsz == 0 ) {
            /* lazy encoding of packet header */
            curPacket->encode_pdr_header();
            hsz = curPacket->get_HeaderLen();
        }

        ncbufs[j].buf = const_cast< char* >( curPacket->get_Header() );
        ncbufs[j].len = hsz;
        packet_sizes[i] = (uint64_t)hsz;

        ncbufs[j+1].buf = const_cast< char* >( curPacket->get_Buffer() );
        ncbufs[j+1].len = dsz;
        packet_sizes[i+1] = (uint64_t)dsz;

        total_bytes += (size_t)hsz + (size_t)dsz;
    }

    //
    // packet count
    //
    pdrmem_create( &pdrs, _packet_count_buf, _packet_count_buf_len, op );
    pdr_uint32(&pdrs, &num_packets);
    ncbufs[0].buf = _packet_count_buf;
    ncbufs[0].len = _packet_count_buf_len;


    //
    // packet sizes
    //
    pdrmem_create( &pdrs, buf, buf_len, op );
    if( ! pdr_vector(&pdrs, (char*)packet_sizes, num_buffers, 
                     sizeof(uint64_t), (pdrproc_t)pdr_uint64) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_vector() failed\n" ));
        rc = -1;
        goto send_cleanup_return;
    }
    ncbufs[1].buf = buf;
    ncbufs[1].len = buf_len;

    // Send it all
    sret = XPlat::NCSend( sock_fd, ncbufs, num_ncbufs );
    if( sret < (ssize_t)(total_bytes + _packet_count_buf_len + buf_len) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                    "XPlat::NCSend() returned %"PRIsszt" of %"PRIszt" bytes, nbuffers = %u\n",
                               sret, total_bytes, num_buffers ));
        rc = -1;
        goto send_cleanup_return;
    }

    packetLength = (int) send_packets.size();
    piter = send_packets.begin();
    for( ; piter != send_packets.end(); piter++ ) {
        PacketPtr& pkt = *piter;
        strm = _net->get_Stream( pkt->get_StreamId() );
        if( strm != NULL ) {
            Timer tmp;
            pkt->set_Timer(PERFDATA_PKT_TIMERS_RECV_TO_FILTER, tmp);
            if( strm->get_PerfData()->is_Enabled(PERFDATA_MET_ELAPSED_SEC, 
                                                 PERFDATA_PKT_SEND) ) {
                pkt->set_OutgoingPktCount(packetLength);
                pkt->stop_Timer(PERFDATA_PKT_TIMERS_SEND);
            }
            strm->get_PerfData()->add_PacketTimers(pkt);
        }
    }

 send_cleanup_return:

    if( ! using_prealloc ) {
        free( buf );
        free( packet_sizes );
        delete[] ncbufs;
    }

    if( go_away ) {
        // exit send thread
        mrn_dbg( 5, mrn_printf(FLF, stderr, "I'm going away now!\n" ));
        tsd_t* tsd = (tsd_t*)tsd_key.Get();
        if( tsd != NULL ) {
            tsd_key.Set( NULL );
            free( const_cast<char*>( tsd->thread_name ) );
            delete tsd;
        }
        XPlat::Thread::Exit(NULL);
    }    

    mrn_dbg_func_end();
    return rc;
}

void Message::add_Packet( PacketPtr packet )
{
    _packet_sync.Lock();
    if( packet != Packet::NullPacket )
        _packets.push_back( packet );
    _packet_sync.SignalCondition(MRN_QUEUE_NONEMPTY);
    _packet_sync.Unlock();
}

int Message::size_Packets( void )
{
    _packet_sync.Lock();
    int size = _packets.size( );
    _packet_sync.Unlock();

    return size;
}

void Message::waitfor_MessagesToSend( void )
{
    _packet_sync.Lock();

    while( _packets.empty() ) {
        _packet_sync.WaitOnCondition( MRN_QUEUE_NONEMPTY );
    }

    _packet_sync.Unlock();
}

/*********************************************************
 *  Functions used to implement sending and recieving of
 *  some basic data types
 *********************************************************/

ssize_t MRN_send( XPlat::XPSOCKET fd, const char *buf, size_t count )
{    
    ssize_t rc;
    rc = XPlat::NCsend( fd, buf, count );
    if( rc < 0 )
        return -1;
    else
        return rc;
}

ssize_t MRN_recv( XPlat::XPSOCKET fd, char *buf, size_t count )
{
    ssize_t rc;
    rc = XPlat::NCrecv( fd, buf, count );
    if( rc < 0 )
        return -1;
    else
        return rc;
}
} // namespace MRN
