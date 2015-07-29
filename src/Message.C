/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "Message.h"
#include "PeerNode.h"
#include "pdr.h"

#include "mrnet/Packet.h"
#include "mrnet/Stream.h"
#include "xplat/Atomic.h"
#include "xplat/Error.h"
#include "xplat/NetUtils.h"
#include "xplat/SocketUtils.h"

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
    pdr_sizeof((pdrproc_t)( pdr_uint32 ), &num_packets, &_packet_count_buf_len); 
    _packet_sizes_buf_len = ((size_t)MESSAGE_PREALLOC_LEN * sizeof(uint64_t)) + 1;
    _ncbuf_len = (size_t) MESSAGE_PREALLOC_LEN;

    _packet_count_buf = (char*) malloc(_packet_count_buf_len + 1);
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

int Message::recv( XPlat_Socket sock_fd, std::list< PacketPtr > &packets_in,
                   Rank iinlet_rank )
{
    Timer t1;
    t1.start();
    ssize_t retval;
    size_t buf_len;
    uint64_t *packet_sizes;
    char *buf = NULL;
    XPlat::SocketUtils::NCBuf* ncbufs;
    uint64_t len;
    unsigned int i, j;
    int rc = 0;
    ssize_t total_bytes = 0;
    uint32_t num_packets = 0, num_buffers;
    PDR pdrs;
    enum pdr_op op = PDR_DECODE;
    bool using_prealloc = true;
    int pkt_size;
    PacketPtr pkt; 
    Stream * strm;
    std::list< PacketPtr >::iterator piter;

    retval = MRN_recv( sock_fd, _packet_count_buf, size_t(_packet_count_buf_len + 1));
    if( retval != (ssize_t)_packet_count_buf_len + 1 ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr, "MRN_recv() %" PRIsszt" of %" PRIszt" bytes received\n", 
                               retval, _packet_count_buf_len));
        return -1;
    }

    pdrmem_create( &pdrs, &(_packet_count_buf[1]), _packet_count_buf_len, op, (pdr_byteorder)_packet_count_buf[0] );

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
        memset( (void *)_ncbuf, 0, sizeof(_ncbuf) );
    }
    else {
        using_prealloc = false;
        buf = (char*) malloc( buf_len );
        packet_sizes = (uint64_t*) malloc( sizeof(uint64_t) * num_buffers );
        ncbufs = new XPlat::SocketUtils::NCBuf[num_buffers];
        memset( (void*)ncbufs, 0,
                 num_buffers * sizeof(XPlat::SocketUtils::NCBuf) );
    }

    retval = MRN_recv( sock_fd, buf, buf_len );
    if( retval != (ssize_t)buf_len ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr, "MRN_recv() %" PRIsszt" of %" PRIsszt" bytes received\n", 
                               retval, buf_len ));
        rc = -1;
        goto recv_cleanup_return;
    }

    pdrmem_create( &pdrs, buf, buf_len, op, pdrmem_getbo() );

    if( ! pdr_vector(&pdrs, (char*)packet_sizes, num_buffers,
                     sizeof(uint64_t), (pdrproc_t)pdr_uint64) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_vector() failed\n" ));
        rc = -1;
        goto recv_cleanup_return;
    }


    /* NOTE: we tell users that packet header and data buffers will have similar
             alignment characteristics to malloc, so if we ever stop using malloc
             we will need to make sure that the buffers are properly aligned */
    for( i = 0; i < num_buffers; i++ ) {
        len = packet_sizes[i];
        ncbufs[i].buf = (char*) malloc( size_t(len) );
        ncbufs[i].len = size_t(len);
        total_bytes += size_t(len);
    }

    retval = XPlat::SocketUtils::Recv( sock_fd, ncbufs, num_buffers );
    if( retval != total_bytes ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "SocketUtils::Recv %" PRIsszt" of %" PRIsszt" bytes received\n", 
                               retval, total_bytes) );
        rc = -1;
        goto recv_cleanup_return;
    }

    //
    // post-processing
    //

    for( i = 0, j = 0; j < num_packets; i += 2, j++ ) {
        PacketPtr new_packet( new Packet((unsigned int)ncbufs[i].len,
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
        ncbufs[i].buf = NULL;
        ncbufs[i+1].buf = NULL;        
    }

    t1.stop();
    pkt_size = (int) packets_in.size();
    piter = packets_in.begin();
    for( ; piter != packets_in.end(); piter++ ) {
        pkt = *piter;
        strm =  _net->get_Stream( pkt->get_StreamId() );
        if( strm != NULL ) {
            // Time for packet at this point in time.
            if( strm->get_PerfData()->is_Enabled(PERFDATA_MET_ELAPSED_SEC, 
                        PERFDATA_CTX_PKT_RECV) ) {
                pkt->set_Timer(PERFDATA_PKT_TIMERS_RECV, t1);
            }
            pkt->start_Timer(PERFDATA_PKT_TIMERS_RECV_TO_FILTER);
            pkt->set_IncomingPktCount(pkt_size);
        }
    }
 recv_cleanup_return:

    if( -1 == rc ) {
        for( unsigned u = 0; u < num_buffers; u++ ) {
            if( NULL != ncbufs[u].buf )
                free( (void*)(ncbufs[u].buf) );
        }
    }

    if( ! using_prealloc ) {
        free( buf );
        free( packet_sizes );
        delete[] ncbufs;
    }

    mrn_dbg_func_end();
    return rc;
}

int Message::send( XPlat_Socket sock_fd )
{
    ssize_t sret;
    size_t buf_len, total_bytes = 0;
    Stream* strm;
    PerfDataMgr* pdm = NULL;
    uint64_t *packet_sizes = NULL;
    char *buf = NULL;
    XPlat::SocketUtils::NCBuf* ncbufs;
    Timer tmp;
    unsigned int i, j;
    int packetLength, rc = 0;
    uint32_t num_packets, num_buffers, num_ncbufs;
    PDR pdrs;
    enum pdr_op op = PDR_ENCODE;
    bool using_prealloc = true;
    bool go_away = false;
    PacketPtr pkt;
    std::list< PacketPtr > send_packets;
    std::list< PacketPtr >::iterator piter;

    _packet_sync.Lock();
    _send_sync.Lock();
    if( _packets.size() == 0 ) {   //nothing to do
        mrn_dbg( 3, mrn_printf(FLF, stderr, "Nothing to send!\n") );
        _packet_sync.Unlock();
        _send_sync.Unlock();
        return 0;
    }
    send_packets = _packets;
    _packets.clear();
    _packet_sync.Unlock();

    piter = send_packets.begin();
    for( ; piter != send_packets.end(); piter++ ) {
        pkt = *piter;
        strm = _net->get_Stream( pkt->get_StreamId() );
        if( NULL != strm ) {
            pdm = strm->get_PerfData();
            if( NULL != pdm ) {
                if( pdm->is_Enabled(PERFDATA_MET_ELAPSED_SEC, 
                                    PERFDATA_CTX_PKT_SEND) ) {
                    pkt->start_Timer(PERFDATA_PKT_TIMERS_SEND);
                    pkt->stop_Timer(PERFDATA_PKT_TIMERS_FILTER_TO_SEND);
                }
            }
        }
    }

    // Allocation (if required)
    num_packets = uint32_t(send_packets.size());
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
        ncbufs = new XPlat::SocketUtils::NCBuf[ num_ncbufs ];
        packet_sizes = (uint64_t*) malloc( sizeof(uint64_t) * num_buffers );
    }

    //
    // packets
    //
    piter = send_packets.begin();
    for( i = 0; piter != send_packets.end(); piter++, i += 2 ) {

        /* j accounts for skipping first two ncbufs that hold pkt count and sizes */
        j = i + 2;

        PacketPtr& curPacket = *piter;
        
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
        ncbufs[j+1].len = size_t(dsz);
        packet_sizes[i+1] = (uint64_t)dsz;

        total_bytes += (size_t)hsz + (size_t)dsz;
    }

    //
    // packet count
    //
    pdrmem_create( &pdrs, &(_packet_count_buf[1]), _packet_count_buf_len, op, pdrmem_getbo() );
    pdr_uint32(&pdrs, &num_packets);
    ncbufs[0].buf = _packet_count_buf;
    ncbufs[0].len = size_t(_packet_count_buf_len + 1);
    _packet_count_buf[0] = (char) pdrmem_getbo();
    //
    // packet sizes
    //
    pdrmem_create( &pdrs, buf, buf_len, op, pdrmem_getbo() );
    if( ! pdr_vector(&pdrs, (char*)packet_sizes, num_buffers, 
                     sizeof(uint64_t), (pdrproc_t)pdr_uint64) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_vector() failed\n" ));
        rc = -1;
        goto send_cleanup_return;
    }
    ncbufs[1].buf = buf;
    ncbufs[1].len = buf_len;

    // Send it all
    sret = XPlat::SocketUtils::Send( sock_fd, ncbufs, num_ncbufs );
    if( sret < (ssize_t)(total_bytes + _packet_count_buf_len + buf_len + 1) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                               "XPlat::SocketUtils::Send() returned %" PRIsszt
		               " of %" PRIszt" bytes, nbuffers = %u\n",
                               sret, total_bytes, num_buffers ));
        rc = -1;
        goto send_cleanup_return;
    }

    packetLength = (int) send_packets.size();
    piter = send_packets.begin();
    for( ; piter != send_packets.end(); piter++ ) {
        pkt = *piter;
        strm = _net->get_Stream( pkt->get_StreamId() );
        if( NULL != strm ) {
            pdm = strm->get_PerfData();
            if( NULL != pdm ) {
                pkt->set_Timer( PERFDATA_PKT_TIMERS_RECV_TO_FILTER, tmp );
                if( pdm->is_Enabled(PERFDATA_MET_ELAPSED_SEC, 
                                    PERFDATA_CTX_PKT_SEND) ) {
                    pkt->set_OutgoingPktCount( packetLength );
                    pkt->stop_Timer( PERFDATA_PKT_TIMERS_SEND );
                    pdm->add_PacketTimers( pkt );
                }   
                else if( pdm->is_Enabled(PERFDATA_MET_ELAPSED_SEC,  PERFDATA_CTX_PKT_NET_SENDCHILD) || 
                     pdm->is_Enabled(PERFDATA_MET_ELAPSED_SEC, PERFDATA_CTX_PKT_NET_SENDPAR) ||
                     pdm->is_Enabled(PERFDATA_MET_ELAPSED_SEC, PERFDATA_CTX_PKT_FILTER_TO_SEND)) 
                {
                    pdm->add_PacketTimers( pkt );
                }
            }
        }
    }

 send_cleanup_return:
    _send_sync.Unlock();

    if( ! using_prealloc ) {
        free( buf );
        free( packet_sizes );
        delete[] ncbufs;
    }

    if( go_away ) {
        // exit send thread
        mrn_dbg( 5, mrn_printf(FLF, stderr, "I'm going away now!\n" ));
        tsd_t* tsd = (tsd_t*)XPlat::XPlat_TLSKey->GetUserData();
        if( NULL != tsd ) {
            delete tsd;
            if(XPlat::XPlat_TLSKey->SetUserData(NULL) != 0) {
                mrn_dbg(1, mrn_printf(FLF, stderr, "Thread 0x%lx failed to set"
                            " thread-specific user data to NULL.\n",
                            XPlat::Thread::GetId()));
            }
            if(XPlat::XPlat_TLSKey->DestroyData() != 0) {
                mrn_dbg(1, mrn_printf(FLF, stderr, "Thread 0x%lx failed to "
                            "destroy thread-specific data.\n",
                            XPlat::Thread::GetId()));
            }
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

size_t Message::size_Packets( void )
{
    _packet_sync.Lock();
    size_t size = _packets.size( );
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

ssize_t MRN_send( XPlat_Socket fd, const char *buf, size_t count )
{    
    ssize_t rc;
    rc = XPlat::SocketUtils::send( fd, buf, count );
    if( rc < 0 )
        return -1;
    else
        return rc;
}

ssize_t MRN_recv( XPlat_Socket fd, char *buf, size_t count )
{
    ssize_t rc;
    rc = XPlat::SocketUtils::recv( fd, buf, count );
    if( rc < 0 )
        return -1;
    else
        return rc;
}

} // namespace MRN
