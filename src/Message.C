/****************************************************************************
 *  Copyright 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <fcntl.h>

#include "pdr.h"
#include "utils.h"
#include "Message.h"
#include "PeerNode.h"

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

Message::Message()
{
    _packet_sync.RegisterCondition( MRN_QUEUE_NONEMPTY );
}

int Message::recv( XPlat::XPSOCKET sock_fd, std::list< PacketPtr > &packets_in,
                   Rank iinlet_rank )
{
    unsigned int i, j;
    int32_t buf_len;
    uint32_t num_packets = 0, num_buffers, *packet_sizes;
    char *buf = NULL;
    PDR pdrs;
    enum pdr_op op = PDR_DECODE;

    mrn_dbg_func_begin();

    //
    // packet count
    //

    mrn_dbg( 5, mrn_printf(FLF, stderr, "Calling pdr_sizeof()\n" ));
    buf_len = pdr_sizeof( ( pdrproc_t ) ( pdr_uint32 ), &num_packets );
    assert( buf_len );
    buf = ( char * )malloc( buf_len );
    assert( buf );

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Reading packet count\n") );
    int retval;
    if( (retval = MRN_recv(sock_fd, buf, buf_len)) != buf_len ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr, "MRN_recv() %d of %d bytes received\n", 
                               retval, buf_len ));
        free( buf );
        return -1;
    }
    MRN_bytes_recv.Add( retval );

    mrn_dbg( 5, mrn_printf(FLF, stderr, "Calling pdrmem_create()\n" ));
    pdrmem_create( &pdrs, buf, buf_len, op );
    mrn_dbg( 5, mrn_printf(FLF, stderr, "Calling pdr_uint32()\n" ));
    if( ! pdr_uint32(&pdrs, &num_packets) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_uint32() failed\n") );
        free( buf );
        return -1;
    }
    free( buf );
    mrn_dbg( 5, mrn_printf(FLF, stderr, "Will receive %d packets\n",
                           num_packets) );

    if( num_packets > 10000 )
        mrn_dbg( 1, mrn_printf(FLF, stderr, "WARNING: Receiving more than 10000 packets\n"));
    if( num_packets == 0 )
        mrn_dbg( 1, mrn_printf(FLF, stderr, "WARNING: Receiving zero packets\n"));
    
    num_buffers = num_packets * 2;

    //
    // packet size vector
    //

    //buf_len's value is hardcode, breaking pdr encapsulation barrier :(
    buf_len = (sizeof(uint32_t) * num_buffers) + 1;  // 1 byte pdr overhead
    buf = (char*) malloc( buf_len );
    assert( buf );

    packet_sizes = (uint32_t*) malloc( sizeof(uint32_t) * num_buffers );
    if( packet_sizes == NULL ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                    "recv: packet_size malloc is NULL for %d packets\n",
                    num_packets ));
        return -1;
    }

    mrn_dbg( 5, mrn_printf(FLF, stderr, "Calling MRN_recv(%d, %p, %d)\n",
                           sock_fd, buf, buf_len) );
    int readRet = MRN_recv( sock_fd, buf, buf_len );
    if( readRet != buf_len ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr, "MRN_recv() %d of %d bytes received\n", 
                               readRet, buf_len ));
        free( buf );
        free( packet_sizes );
        return -1;
    }
    MRN_bytes_recv.Add( readRet );

    mrn_dbg( 5, mrn_printf(FLF, stderr, "Calling pdrmem_create\n" ));
    pdrmem_create( &pdrs, buf, buf_len, op );
    if( ! pdr_vector( &pdrs, (char*)packet_sizes, num_buffers,
                      sizeof(uint32_t), (pdrproc_t)pdr_uint32 ) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_vector() failed\n" ));
        free( buf );
        free( packet_sizes );
        return -1;
    }
    free( buf );

    //
    // packets
    //

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Reading buffers for %d packets\n",
                           num_packets) );

    XPlat::NCBuf* ncbufs = new XPlat::NCBuf[num_buffers];

    ssize_t total_bytes = 0;
    for( i = 0; i < num_buffers; i++ ) {
        uint32_t len = packet_sizes[i];
        ncbufs[i].buf = (char*) malloc( len );
        ncbufs[i].len = len;
        total_bytes += len;
        mrn_dbg( 5, mrn_printf(FLF, stderr, "buffer %u has size %d\n", 
                               i, packet_sizes[i]) );
    }

    mrn_dbg( 5, mrn_printf(FLF, stderr, "Calling NCRecv\n") );
    retval = XPlat::NCRecv( sock_fd, ncbufs, num_buffers );
    if( retval != total_bytes ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "NCRecv %d of %d bytes received\n", 
                               retval, total_bytes) );

        for( i = 0; i < num_buffers; i++ )
            free( ncbufs[i].buf );
        delete[] ncbufs;
        free( packet_sizes );

        return -1;
    }
    MRN_bytes_recv.Add( retval );

    //
    // post-processing
    //

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Creating Packets\n") );
    for( i = 0, j = 0; j < num_packets; i += 2, j++ ) {

        mrn_dbg( 5, mrn_printf(FLF, stderr, "creating packet[%d]\n", j) );

        PacketPtr new_packet( new Packet(ncbufs[i].len,
                                         ncbufs[i].buf,
                                         ncbufs[i+1].len,
                                         ncbufs[i+1].buf,
                                         iinlet_rank) );
        if( new_packet->has_Error() ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "packet creation failed\n") );
            for( unsigned u = 0; u < num_buffers; u++ )
                free( ncbufs[u].buf );
            delete[] ncbufs;
            free( packet_sizes );
            return -1;
        }
        packets_in.push_back( new_packet );
    }

    // release dynamically allocated memory
    // Note: don't release the NC buffers; that memory was passed
    //       off to the Packet object(s).
    delete[] ncbufs;
    free( packet_sizes );
 
    mrn_dbg_func_end();
    return 0;
}

int Message::send( XPlat::XPSOCKET sock_fd )
{
    unsigned int i, j;
    uint32_t num_packets, num_buffers;
    uint32_t *packet_sizes = NULL;
    char *buf = NULL;
    int buf_len;
    ssize_t total_bytes = 0;
    PDR pdrs;
    enum pdr_op op = PDR_ENCODE;
    bool go_away = false;

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Sending packets from message[%p]\n",
                           this) );

    _packet_sync.Lock();
    if( _packets.size() == 0 ) {   //nothing to do
        mrn_dbg( 3, mrn_printf(FLF, stderr, "Nothing to send!\n") );
        _packet_sync.Unlock();
        return 0;
    }

    //
    // pre-processing
    //

    num_packets = _packets.size();
    num_buffers = num_packets * 2;

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Writing %u packets\n",
                           num_packets ));

    XPlat::NCBuf* ncbufs = new XPlat::NCBuf[num_buffers];
    packet_sizes = (uint32_t*) malloc( sizeof(uint32_t) * num_buffers );
    assert( packet_sizes );

    std::list< PacketPtr >::iterator iter = _packets.begin();
    for( i = 0, j = 0; iter != _packets.end(); iter++, i += 2, j++ ) {

        PacketPtr curPacket( *iter );
        
        /* check for final packet */
        int tag = curPacket->get_Tag();
        if( (tag == PROT_SHUTDOWN) || (tag == PROT_SHUTDOWN_ACK) )
            go_away = true;

        uint32_t hsz = curPacket->get_HeaderLen();
        uint32_t dsz = curPacket->get_BufferLen();

        if( hsz == 0 ) {
            /* lazy encoding of packet header */
            curPacket->encode_pdr_header();
            hsz = curPacket->get_HeaderLen();
        }

        ncbufs[i].buf = const_cast< char* >( curPacket->get_Header() );
        ncbufs[i].len = hsz;
        packet_sizes[i] = hsz;

        ncbufs[i+1].buf = const_cast< char* >( curPacket->get_Buffer() );
        ncbufs[i+1].len = dsz;
        packet_sizes[i+1] = dsz;

        total_bytes += hsz + dsz;
        mrn_dbg( 5, mrn_printf(FLF, stderr, "packet %u has size (%u,%u)\n", 
                               j, hsz, dsz) );
    }

    //
    // packet count
    //

    buf_len = pdr_sizeof( (pdrproc_t)( pdr_uint32 ), &num_packets );
    assert( buf_len );
    buf = (char*) malloc( buf_len );
    assert( buf );
    pdrmem_create( &pdrs, buf, buf_len, op );

    if( ! pdr_uint32(&pdrs, &num_packets) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_uint32() failed\n") );
        free( buf );
        delete[] ncbufs;
        free( packet_sizes );
        _packet_sync.Unlock();
        return -1;
    }

    mrn_dbg( 5, mrn_printf(FLF, stderr, "calling MRN_send() for number of packets\n" ));
    if( MRN_send( sock_fd, buf, buf_len ) != buf_len ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "MRN_send() failed\n" ));
        free( buf );
        delete[] ncbufs;
        free( packet_sizes );
        _packet_sync.Unlock();
        return -1;
    }
    free( buf );
    MRN_bytes_send.Add( buf_len );


    //
    // packet size vector
    //

    //buf_len's value is hardcode, breaking pdr encapsulation barrier :(
    buf_len = (num_buffers * sizeof(uint32_t)) + 1;  //1 extra bytes overhead
    buf = (char*) malloc( buf_len );
    assert( buf );
    pdrmem_create( &pdrs, buf, buf_len, op );

    if( ! pdr_vector(&pdrs, (char*)packet_sizes, num_buffers, 
                     sizeof(uint32_t), (pdrproc_t)pdr_uint32) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_vector() failed\n" ));
        free( buf );
        delete[] ncbufs;
        free( packet_sizes );
        _packet_sync.Unlock();
        return -1;
    }

    mrn_dbg( 5, mrn_printf(FLF, stderr, 
                           "calling MRN_send() for packet-size vec of len %d\n", 
                           buf_len) );
    int mcwret = MRN_send( sock_fd, buf, buf_len );
    if( mcwret != buf_len ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "MRN_send() failed\n" ));
        free( buf );
        delete[] ncbufs;
        free( packet_sizes );
        _packet_sync.Unlock();
        return -1;
    }
    MRN_bytes_send.Add( mcwret );
    free( packet_sizes );
    free( buf );

    //
    // packets
    //

    /* send the packets */
    mrn_dbg( 5, mrn_printf(FLF, stderr,
                "calling XPlat::NCSend(%d buffers, %d total bytes)\n",
                           num_buffers, total_bytes ));

    ssize_t sret = XPlat::NCSend( sock_fd, ncbufs, num_buffers );
    if( sret != total_bytes ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                    "XPlat::NCSend() returned %d of %d bytes, nbuffers = %d\n",
                               sret, total_bytes, num_buffers ));
        // for( i = 0; i < num_buffers; i++ ) {
//             mrn_dbg( 5, mrn_printf(FLF, stderr, "buffer[%d].size = %d\n",
//                                    i, ncbufs[i].len ));
//         }
        delete[] ncbufs;
        _packet_sync.Unlock();
        return -1;
    }
    MRN_bytes_send.Add( sret );

    _packets.clear();
    _packet_sync.Unlock();

    delete[] ncbufs;

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

int MRN_send( XPlat::XPSOCKET fd, const char *buf, int count )
{    
    ssize_t rc;
    size_t nbytes = (size_t) count;
    rc = XPlat::NCsend( fd, buf, nbytes );
    if( rc < 0 )
        return -1;
    else
        return (int) rc;
}

int MRN_recv( XPlat::XPSOCKET fd, char *buf, int count )
{
    ssize_t rc;
    size_t nbytes = (size_t) count;
    rc = XPlat::NCrecv( fd, buf, nbytes );
    if( rc < 0 )
        return -1;
    else
        return (int) rc;
}

} // namespace MRN
