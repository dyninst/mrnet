/****************************************************************************
 *  Copyright 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
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

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

namespace MRN
{

static XPlat::Atomic<uint64_t> MRN_bytes_send = 0;
static XPlat::Atomic<uint64_t> MRN_bytes_recv = 0;
uint64_t get_TotalBytesSend(void) { return MRN_bytes_send.Get(); }
uint64_t get_TotalBytesRecv(void) { return MRN_bytes_recv.Get(); }


int read( int fd, void *buf, int size );
int write( int fd, const void *buf, int size );

Message::Message()
{
    _packet_sync.RegisterCondition( MRN_QUEUE_NONEMPTY );
}

int Message::recv( int sock_fd, std::list < PacketPtr >&packets_in,
                   Rank iinlet_rank )
{
    mrn_dbg_func_begin();
    unsigned int i;
    int32_t buf_len;
    uint32_t no_packets = 0, *packet_sizes;
    char *buf = NULL;
    PDR pdrs;
    enum pdr_op op = PDR_DECODE;


    //
    // packet count
    //

    /* find out how many packets are coming */
    mrn_dbg( 3, mrn_printf(FLF, stderr, "Calling sizeof ...\n" ));
    buf_len = pdr_sizeof( ( pdrproc_t ) ( pdr_uint32 ), &no_packets );
    assert( buf_len );
    buf = ( char * )malloc( buf_len );
    assert( buf );

    mrn_dbg( 3, mrn_printf(FLF, stderr, "read(%d, %p, %d) ...\n", sock_fd, buf,
                buf_len ));
    int retval;
    if( ( retval = MRN::read( sock_fd, buf, buf_len ) ) != buf_len ) {

        if( retval == -1 )
            error( ERR_SYSTEM, iinlet_rank, "MRN::read() %s", strerror(errno) );
       
        mrn_dbg( 3, mrn_printf(FLF, stderr, "MRN::read() %d of %d bytes received\n", 
                               retval, buf_len ));
        free( buf );
        return -1;
    }
    MRN_bytes_recv.Add( retval );

    //
    // pdrmem initialize
    //

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Calling memcreate ...\n" ));
    pdrmem_create( &pdrs, buf, buf_len, op );
    mrn_dbg( 3, mrn_printf(FLF, stderr, "Calling uint32 ...\n" ));
    if( !pdr_uint32( &pdrs, &no_packets ) ) {
        error( ERR_PACKING, iinlet_rank, "pdr_uint32() failed\n");
        free( buf );
        return -1;
    }
    mrn_dbg( 3, mrn_printf(FLF, stderr, "Calling free ...\n" ));
    free( buf );
    mrn_dbg( 3, mrn_printf(FLF, stderr, "pdr_uint32() succeeded. Receive %d packets\n",
                           no_packets ));

    if( no_packets > 10000 )
        mrn_dbg( 1, mrn_printf(FLF, stderr, "WARNING: Receiving more than 10000 packets\n"));
    if( no_packets == 0 )
        mrn_dbg( 1, mrn_printf(FLF, stderr, "WARNING: Receiving zero packets\n"));
    

    //
    // packet size vector
    //

    /* recv an vector of packet_sizes */
    //buf_len's value is hardcode, breaking pdr encapsulation barrier :(
    mrn_dbg( 3, mrn_printf(FLF, stderr, "Calling malloc ...\n" ));
    buf_len = (sizeof( uint32_t ) * no_packets) + 1;  // 1 byte pdr overhead
    buf = ( char * )malloc( buf_len );
    assert( buf );

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Calling malloc ...\n" ));
    packet_sizes = ( uint32_t * ) malloc( sizeof( uint32_t ) * no_packets );
    if( packet_sizes == NULL ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                    "recv: packet_size malloc is NULL for %d packets\n",
                    no_packets ));
    }
    assert( packet_sizes );

    mrn_dbg( 3, mrn_printf(FLF, stderr,
                "Calling read(%d, %p, %d) for %d buffer lengths.\n",
                sock_fd, buf, buf_len, no_packets ));
    int readRet = MRN::read( sock_fd, buf, buf_len );
    if( readRet != buf_len ) {

        if( readRet == -1 )
            error( ERR_SYSTEM, iinlet_rank, "MRN::read() %s", strerror(errno) );

        mrn_dbg( 3, mrn_printf(FLF, stderr, "MRN::read() %d of %d bytes received\n", 
                               readRet, buf_len ));
        free( buf );
        free( packet_sizes );
        return -1;
    }
    MRN_bytes_recv.Add( readRet );

    //
    // packets
    //

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Calling pdrmem_create ...\n" ));
    pdrmem_create( &pdrs, buf, buf_len, op );
    if( !pdr_vector ( &pdrs, ( char * )( packet_sizes ), no_packets,
                      sizeof( uint32_t ), ( pdrproc_t ) pdr_uint32 ) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_vector() failed\n" ));
        error( ERR_PACKING, iinlet_rank, "pdr_uint32() failed\n");
        free( buf );
        free( packet_sizes );
        return -1;
    }
    free( buf );

    /* recv packet buffers */
    XPlat::NCBuf* ncbufs = new XPlat::NCBuf[no_packets];

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Reading %d packets of size: [",
                no_packets ));

    int total_bytes = 0;
    for( i = 0; i < no_packets; i++ ) {
        ncbufs[i].buf = (char*)malloc( packet_sizes[i] );
        ncbufs[i].len = packet_sizes[i];
        total_bytes += packet_sizes[i];
        mrn_dbg( 3, mrn_printf(0,0,0, stderr, "%d, ", packet_sizes[i] ));
    }
    mrn_dbg( 3, mrn_printf(0,0,0, stderr, "]\n" ));

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Calling NCRecv ...\n" ));
    retval = XPlat::NCRecv( sock_fd, ncbufs, no_packets );
    if( retval != total_bytes ) {

        if( retval == -1 )
            error( ERR_SYSTEM, iinlet_rank, "XPlat::NCRecv() %s", strerror(errno) );
        mrn_dbg( 3, mrn_printf(FLF, stderr, "NCRecv %d of %d received\n", 
                               retval, total_bytes ));

        for( i = 0; i < no_packets; i++ )
            free( (void*)(ncbufs[i].buf) );
        delete[] ncbufs;
        free( packet_sizes );

        return -1;
    }
    MRN_bytes_recv.Add( retval );

    //
    // post-processing
    //

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Creating Packets ...\n" ));
    for( i = 0; i < no_packets; i++ ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr, "Creating packet[%d] ...\n", i ));
        //unsigned short * s = (unsigned short *)(ncbufs[i].buf+1);
        //assert( 0 <= *s  && *s < 5 );
        PacketPtr new_packet( new Packet( ncbufs[i].len,
                                          ncbufs[i].buf,
                                          iinlet_rank ));
        if( new_packet->has_Error( ) ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "packet creation failed\n" ));
            for( i = 0; i < no_packets; i++ )
                free( (void*)(ncbufs[i].buf) );
            delete[] ncbufs;
            free( packet_sizes );
            return -1;
        }
        packets_in.push_back( new_packet );
    }

    // release dynamically allocated memory
    // Note: don't release the NC buffers; that memory was passed
    // off to the Packet object(s).
    delete[] ncbufs;
    free( packet_sizes );
    mrn_dbg( 3, mrn_printf(FLF, stderr, "Msg(%p)::recv() succeeded\n", this ));
    return 0;
}

int Message::send( int sock_fd )
{
    unsigned int i;
    uint32_t no_packets;
    uint32_t *packet_sizes = NULL;
    char *buf = NULL;
    int buf_len, total_bytes = 0;
    PDR pdrs;
    enum pdr_op op = PDR_ENCODE;
    bool go_away = false;

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Sending packets from message %p\n",
                this ));

    _packet_sync.Lock();
    if( _packets.size( ) == 0 ) {   //nothing to do
        mrn_dbg( 3, mrn_printf(FLF, stderr, "Nothing to send!\n" ));
        _packet_sync.Unlock();
        return 0;
    }

    /* Process packets in list to prepare for send() */
    no_packets = _packets.size( );
    XPlat::NCBuf* ncbufs = new XPlat::NCBuf[no_packets];
    packet_sizes = ( uint32_t * ) malloc( sizeof( uint32_t ) * no_packets );
    assert( packet_sizes );

    std::list < PacketPtr >::iterator iter = _packets.begin( );
    mrn_dbg( 3, mrn_printf(FLF, stderr, "Writing %d packets of size: [ ",
                           no_packets ));
    for( i = 0; iter != _packets.end( ); iter++, i++ ) {

        PacketPtr curPacket( *iter );

        /* check for final packet */
        int tag = curPacket->get_Tag();
        if( (tag == PROT_SHUTDOWN) || (tag == PROT_SHUTDOWN_ACK) )
            go_away = true;

        uint32_t psz = curPacket->get_BufferLen();
        ncbufs[i].buf = const_cast< char * >( curPacket->get_Buffer( ) );
        ncbufs[i].len = psz;
        packet_sizes[i] = psz;
        total_bytes += psz;
        mrn_dbg( 3, mrn_printf(0,0,0, stderr, "%u, ", psz ));
    }
    mrn_dbg( 3, mrn_printf(0,0,0, stderr, "]\n" ));

    /* put how many packets are going */
    buf_len = pdr_sizeof( ( pdrproc_t )( pdr_uint32 ), &no_packets );
    assert( buf_len );
    buf = ( char * )malloc( buf_len );
    assert( buf );
    pdrmem_create( &pdrs, buf, buf_len, op );

    if( !pdr_uint32( &pdrs, &no_packets ) ) {
        error( ERR_PACKING, UnknownRank, "pdr_uint32() failed\n" );
        free( buf );
        delete[] ncbufs;
        free( packet_sizes );
        _packet_sync.Unlock();
        return -1;
    }

    if( MRN::write( sock_fd, buf, buf_len ) != buf_len ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "write() failed\n" ));
        _perror( "write()" );
        free( buf );
        delete[] ncbufs;
        free( packet_sizes );
        _packet_sync.Unlock();
        return -1;
    }
    MRN_bytes_send.Add( buf_len );

    mrn_dbg( 3, mrn_printf(FLF, stderr, "write() succeeded\n" ));
    free( buf );

    /* send a vector of packet_sizes */
    buf_len = (no_packets * sizeof( uint32_t )) + 1;  //1 extra bytes overhead
    buf = ( char * )malloc( buf_len );
    assert( buf );
    pdrmem_create( &pdrs, buf, buf_len, op );

    if( !pdr_vector
        ( &pdrs, ( char * )( packet_sizes ), no_packets, sizeof( uint32_t ),
          ( pdrproc_t ) pdr_uint32 ) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_vector() failed\n" ));
        error( ERR_PACKING, UnknownRank, "pdr_vector() failed\n" );
        free( buf );
        delete[] ncbufs;
        free( packet_sizes );
        _packet_sync.Unlock();
        return -1;
    }

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Calling write(%d, %p, %d)\n", sock_fd,
                buf, buf_len ));
    int mcwret = MRN::write( sock_fd, buf, buf_len );
    if( mcwret != buf_len ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "write failed\n" ));
        _perror( "write()" );
        free( buf );
        delete[] ncbufs;
        free( packet_sizes );
        _packet_sync.Unlock();
        return -1;
    }
    MRN_bytes_send.Add( mcwret );
    free( packet_sizes );
    free( buf );


    // send the packets
    mrn_dbg( 3, mrn_printf(FLF, stderr,
                "Calling XPlat::NCSend(%d buffers, %d total bytes)\n",
                no_packets, total_bytes ));

    int sret = XPlat::NCSend( sock_fd, ncbufs, no_packets );
    if( sret != total_bytes ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr,
                    "XPlat::NCSend() returned %d of %d bytes, errno = %d, nbuffers = %d\n",
                    sret, total_bytes, errno, no_packets ));
        for( i = 0; i < no_packets; i++ ) {
            mrn_dbg( 3, mrn_printf(FLF, stderr, "buffer[%d].size = %d\n",
                        i, ncbufs[i].len ));
        }
        _perror( "XPlat::NCSend()" );
        error( ERR_SYSTEM, UnknownRank, "XPlat::NCSend() returned %d of %d bytes: %s\n",
               sret, total_bytes, strerror(errno) );
        delete[] ncbufs;
        _packet_sync.Unlock( );
        return -1;
    }
    MRN_bytes_send.Add( sret );

    _packets.clear( );
    _packet_sync.Unlock( );

    delete[] ncbufs;
    mrn_dbg( 3, mrn_printf(FLF, stderr, "msg(%p)::send() succeeded\n", this ));

    if( go_away ) {
        // exit send thread
        mrn_dbg( 5, mrn_printf(FLF, stderr, "I'm going away now!\n" ));
        XPlat::Thread::Exit(NULL);
    }

    return 0;
}

void Message::add_Packet( const PacketPtr packet )
{
    _packet_sync.Lock();
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

int Message::size_Bytes( void )
{
    assert( 0 );
    return 0;
}

/*********************************************************
 *  Functions used to implement sending and recieving of
 *  some basic data types
 *********************************************************/

int write( int ifd, const void *ibuf, int ibuf_len )
{
    mrn_dbg( 5, mrn_printf(FLF, stderr, "%d, %p, %d\n", ifd, ibuf, ibuf_len ));

    // don't generate SIGPIPE
    int flags = MSG_NOSIGNAL;

    int ret = ::send( ifd, (const char*)ibuf, ibuf_len, flags );
    if( ret == -1 ) {
        int err = XPlat::NetUtils::GetLastError();
        mrn_dbg( 1, mrn_printf(FLF, stderr, "send() failed with error '%s'\n", 
                               XPlat::Error::GetErrorString( err ).c_str()) );
    }
    else mrn_dbg( 5, mrn_printf(FLF, stderr, "send => %d\n", ret ));
    return ret;
}

int read( int fd, void *buf, int count )
{
    int bytes_recvd = 0, retval, err;
    while( bytes_recvd != count ) {

        retval = ::recv( fd, ( ( char * )buf ) + bytes_recvd,
                       count - bytes_recvd,
                       XPlat::NCBlockingRecvFlag );

        err = XPlat::NetUtils::GetLastError();

        if( retval == -1 ) {
            if( err == EINTR ) {
                continue;
            }
            else {
                std::string errstr = XPlat::Error::GetErrorString( err );
                mrn_dbg( 3, mrn_printf(FLF, stderr,
                                       "premature return from read(). Got %d of %d "
                                       " bytes. error '%s'\n", bytes_recvd, count,
                                       errstr.c_str()) );
                return -1;
            }
        }
        else if( ( retval == 0 ) && ( err == EINTR ) ) {
            // this situation has been seen to occur on Linux
            // when the remote endpoint has gone away
            return -1;
        }
        else {
            bytes_recvd += retval;
            if( bytes_recvd < count && err == EINTR ) {
                continue;
            }
            else {
                // bytes_recvd == count, or error other than EINTR occurred
                if( bytes_recvd != count ) {
                    std::string errstr = XPlat::Error::GetErrorString( err );
                    mrn_dbg( 3, mrn_printf(FLF, stderr,
                                "premature return from read(). %d of %d "
                                " bytes. error '%s'\n", bytes_recvd, count,
                                errstr.c_str()) );
                }
                return bytes_recvd;
            }
        }
    }
    assert( 0 );
    return -1;
}

} // namespace MRN
