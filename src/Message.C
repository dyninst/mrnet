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
    _packet_count_header = pdr_sizeof( (pdrproc_t)( pdr_uint32 ), &num_packets );
    _packet_vector_sizes_size = (10 * sizeof(uint64_t)) + 1;
    _packet_count_buf = (char*) malloc(_packet_count_header);
    _packet_vector_sizes_buf = (char*)malloc(10 * sizeof(uint64_t) + 1);
    _ncbuf_count = 10;
    _packet_sync.RegisterCondition( MRN_QUEUE_NONEMPTY );
}

Message::~Message()
{
    if (_packet_count_buf != NULL)
        free(_packet_count_buf);
    if (_packet_vector_sizes_buf != NULL)
        free (_packet_vector_sizes_buf);
}

int Message::recv( XPlat::XPSOCKET sock_fd, std::list< PacketPtr > &packets_in,
                   Rank iinlet_rank )
{
    Timer t1;
    t1.start();

    unsigned int i, j;
    int32_t buf_len;
    uint32_t num_packets = 0, num_buffers;
    uint64_t *packet_sizes;
    char *buf = NULL;
    PDR pdrs;
    enum pdr_op op = PDR_DECODE;
    ssize_t retval;
    XPlat::NCBuf* ncbufs;
    uint64_t len;
    int total_bytes = 0;

    if( (retval = ::read(sock_fd, _packet_count_buf, _packet_count_header)) != _packet_count_header ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr, "MRN::read() %d of %d bytes received\n", 
                               retval, _packet_count_header));
        free( buf );
        return -1;
    }

    pdrmem_create( &pdrs, _packet_count_buf, _packet_count_header, op );

    if( ! pdr_uint32(&pdrs, &num_packets) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_uint32() failed\n") );
        return -1;
    }
    
    num_buffers = num_packets * 2;

    //
    // packet size vector
    //

    //buf_len's value is hardcode, breaking pdr encapsulation barrier :(
    buf_len = (sizeof(uint64_t) * num_buffers) + 1;  // 1 byte pdr overhead
           
    if (num_buffers < _ncbuf_count)
    {
        buf = _packet_vector_sizes_buf;
        packet_sizes = _packet_sizes;
        ncbufs = _ncbuf;
    }
    else
    {
        packet_sizes = (uint64_t*) malloc( sizeof(uint64_t) * num_buffers );
        ncbufs = new XPlat::NCBuf[num_buffers];
        buf = (char*) malloc( buf_len );
    }

    int readRet = ::read( sock_fd, buf, buf_len );
    if( readRet != buf_len ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr, "MRN::read() %d of %d bytes received\n", 
                               readRet, buf_len ));
        if (buf_len > _packet_vector_sizes_size)
            free( buf );

        return -1;
    }

    pdrmem_create( &pdrs, buf, buf_len, op );

    if( ! pdr_vector( &pdrs, (char*)packet_sizes, num_buffers,
                      sizeof(uint64_t), (pdrproc_t)pdr_uint64 ) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_vector() failed\n" ));
        if (buf_len > _packet_vector_sizes_size)
            free( buf );
        if (num_buffers > _ncbuf_count)
            free( packet_sizes );
        return -1;
    }

    for( i = 0; i < num_buffers; i++ ) {
        len = packet_sizes[i];
        ncbufs[i].buf = (char*) malloc( len );
        ncbufs[i].len = len;
        total_bytes += len;
    }

    retval = XPlat::NCRecv( sock_fd, ncbufs, num_buffers );
    if( retval != total_bytes ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "NCRecv %d of %d bytes received\n", 
                               retval, total_bytes) );

        for( i = 0; i < num_buffers; i++ )
            free( (void*)(ncbufs[i].buf) );
        if (_ncbuf_count < num_buffers)
        {
            delete[] ncbufs;
            free( packet_sizes );
        }
        return -1;
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
            for( unsigned u = 0; u < num_buffers; u++ )
                free( (void*)(ncbufs[u].buf) );
            if (num_buffers > _ncbuf_count)
            {
                delete[] ncbufs;
                free( packet_sizes );
            }
            return -1;
        }
        packets_in.push_back( new_packet );
    }

    if (num_buffers > _ncbuf_count)
    {
        delete[] ncbufs;
        free( packet_sizes );
    }
 
    if (buf_len > _packet_vector_sizes_size)
        free( buf );
    mrn_dbg_func_end();
    return 0;
}

int Message::send( XPlat::XPSOCKET sock_fd )
{
    Stream * strm;
    std::list< PacketPtr > tmp_packets = _packets;
    std::list< PacketPtr >::iterator piter = tmp_packets.begin();
    for( ; piter != tmp_packets.end(); piter++ ) {
        PacketPtr& pkt = *piter;
        strm =  _net->get_Stream(pkt->get_StreamId());
        if (strm != NULL) {
            if(strm->get_PerfData()->is_Enabled( PERFDATA_MET_ELAPSED_SEC, PERFDATA_PKT_SEND)) {
                pkt->start_Timer(PERFDATA_PKT_TIMERS_SEND);
                pkt->stop_Timer(PERFDATA_PKT_TIMERS_FILTER_TO_SEND);
            }
        }
    }

    unsigned int i, j;
    uint32_t num_packets, num_buffers;
    uint64_t *packet_sizes = NULL;
    char *buf = NULL;
    ssize_t buf_len, total_bytes = 0;
    PDR pdrs;
    enum pdr_op op = PDR_ENCODE;
    bool go_away = false;
    std::list< PacketPtr >::iterator iter;
    std::list < PacketPtr > send_packets;
    XPlat::NCBuf* ncbufs;

    _packet_sync.Lock();
    if( _packets.size() == 0 ) {   //nothing to do
        mrn_dbg( 3, mrn_printf(FLF, stderr, "Nothing to send!\n") );
        _packet_sync.Unlock();
        return 0;
    }
    send_packets = _packets;
    _packets.clear();
    _packet_sync.Unlock();

    // Allocation of 
    num_packets = send_packets.size();
    num_buffers = num_packets * 2;
    buf_len = (num_buffers * sizeof(uint64_t)) + 1;  //1 extra bytes overhead

    if (num_buffers + 2 < _ncbuf_count)
    {
        buf = _packet_vector_sizes_buf;
        ncbufs = _ncbuf;
        packet_sizes = _packet_sizes;
    }
    else
    {
        ncbufs = new XPlat::NCBuf[num_buffers + 2];
        packet_sizes = (uint64_t*) malloc( sizeof(uint64_t) * num_buffers );
        buf = (char*) malloc( buf_len );
    }

    iter = send_packets.begin();
    for( i = 0, j = 0; iter != send_packets.end(); iter++, i += 2, j++ ) {

        PacketPtr curPacket( *iter );
        
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

        ncbufs[i + 2].buf = const_cast< char* >( curPacket->get_Header() );
        ncbufs[i + 2].len = hsz;
        packet_sizes[i] = uint64_t(hsz);

        ncbufs[i+3].buf = const_cast< char* >( curPacket->get_Buffer() );
        ncbufs[i+3].len = dsz;
        packet_sizes[i+1] = uint64_t(dsz);

        total_bytes += uint64_t(hsz) + uint64_t(dsz);
    }

    //
    // packet count
    //
    pdrmem_create( &pdrs, _packet_count_buf, _packet_count_header, op );

    pdr_uint32(&pdrs, &num_packets);

    pdrmem_create( &pdrs, buf, buf_len, op );

    if( ! pdr_vector(&pdrs, (char*)packet_sizes, num_buffers, 
                     sizeof(uint64_t), (pdrproc_t)pdr_uint64) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_vector() failed\n" ));
        if (buf_len > _packet_vector_sizes_size)
            free( buf );
        if (num_buffers > _ncbuf_count)
        {
            delete[] ncbufs;
            free( packet_sizes );
        }
        return -1;
    }
    //
    // packets
    //

    ncbufs[0].buf = _packet_count_buf;
    ncbufs[0].len = _packet_count_header;
    ncbufs[1].buf = buf;
    ncbufs[1].len = buf_len;

    // Send the data.
    ssize_t sret = XPlat::NCSend( sock_fd, ncbufs, num_buffers + 2);

    if( sret < total_bytes + _packet_count_header + buf_len) {
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                    "XPlat::NCSend() returned %d of %d bytes, nbuffers = %d\n",
                               sret, total_bytes, num_buffers ));
        if (num_buffers > _ncbuf_count)
        {
            delete[] ncbufs;
            free( packet_sizes );
        }
        if (buf_len > _packet_vector_sizes_size)
            free( buf );                
        return -1;
    }

    if (num_buffers > _ncbuf_count)
    {
        free( packet_sizes );
        delete[] ncbufs;
        free( buf );
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

    int packetLength = (int) tmp_packets.size();
    piter = tmp_packets.begin();
    for( ; piter != tmp_packets.end(); piter++ ) {
        PacketPtr& pkt = *piter;
        strm = _net->get_Stream( pkt->get_StreamId() );
        if (strm != NULL) {
            Timer tmp;
            pkt->set_OutgoingPktCount(packetLength);
            pkt->stop_Timer(PERFDATA_PKT_TIMERS_SEND);
            pkt->set_Timer (PERFDATA_PKT_TIMERS_RECV_TO_FILTER, tmp);
            strm->get_PerfData()->add_PacketTimers(pkt);
        }
    }
    return 0;
}

void Message::add_Packet( const PacketPtr packet )
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
