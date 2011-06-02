/****************************************************************************
 *  Copyright 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "mrnet_lightweight/MRNet.h"
#include "Message.h"
#include "utils_lightweight.h"
#include "vector.h"

#include "xplat_lightweight/NCIO.h"
#include "xplat_lightweight/NetUtils.h"
#include "xplat_lightweight/Types.h"

#ifdef os_windows
#include <winsock2.h>
const int NCBlockingRecvFlag = 0;
#else
const int NCBlockingRecvFlag = MSG_WAITALL;
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

Message_t* new_Message_t()
{
    Message_t* new_message = (Message_t*)malloc(sizeof(Message_t));
    assert(new_message);
    new_message->packet = (Packet_t*)malloc(sizeof(Packet_t));
    assert(new_message->packet);

    return new_message;
}

int Message_recv(int sock_fd, vector_t* packets_in, Rank iinlet_rank)
{
    unsigned int i;
    size_t buf_len;
    uint32_t num_packets=0;
    uint32_t num_buffers=0;
    uint32_t *packet_sizes, psz;
    char *buf = NULL;
    PDR pdrs;
    enum pdr_op op = PDR_DECODE;
    int retval;
    int readRet;
    NCBuf_t* ncbufs;
    int total_bytes = 0;
    Packet_t* new_packet;
    
    mrn_dbg_func_begin();
 
    mrn_dbg(5, mrn_printf(FLF, stderr, "sock_fd = %d\n", sock_fd));

    //
    // packet count
    //

    mrn_dbg(5, mrn_printf(FLF, stderr, "Calling sizeof ...\n"));
    buf_len = pdr_sizeof( (pdrproc_t) (pdr_uint32), &num_packets);
    assert(buf_len);
    buf = (char*)malloc(buf_len);
    assert(buf);

    mrn_dbg(3, mrn_printf(FLF, stderr, "Reading packet count\n"));

    retval = MRN_read(sock_fd, buf, buf_len);
    if( (size_t)retval != buf_len ) {
        mrn_dbg(3, mrn_printf(FLF, stderr, "MRN_read() of packet count failed\n"));
        free(buf);
        return -1;
    }

    mrn_dbg(5, mrn_printf(FLF, stderr, "Calling memcreate ...\n"));
    pdrmem_create(&pdrs, buf, (uint32_t)buf_len, op);
    mrn_dbg(5, mrn_printf(FLF, stderr, "Calling uint32 ...\n"));
    if( ! pdr_uint32(&pdrs, &num_packets) ) {
        error(ERR_PACKING, iinlet_rank, "pdr_uint32() failed\n");
        free(buf);
        return -1;
    } 
    free(buf);
    mrn_dbg(3, mrn_printf(FLF, stderr, "Will receive %u packets\n", num_packets));

    if( num_packets > 10000 )
        mrn_dbg(1, mrn_printf(FLF, stderr, "WARNING: Receiving more than 10000 packets\n"));
    if( num_packets == 0 )
        mrn_dbg(1, mrn_printf(FLF, stderr, "WARNING: Receiving zero packets\n"));

    num_buffers = num_packets * 2;

    //
    // packet size vector
    //
    
    //buf_len's value is hardcode, breaking pdr encapsulation barrier
    buf_len = (sizeof(uint32_t) * num_buffers) + 1; // 1 byte pdr overhead

    //mrn_dbg(5, mrn_printf(FLF, stderr, "Calling malloc(%u) ...\n", buf_len));
    buf = (char*) malloc( buf_len );
    if( buf == NULL ) {
        mrn_dbg(1, mrn_printf(FLF, stderr,
                              "malloc failed for buf\n"));
        return -1;
    }

    //mrn_dbg(5, mrn_printf(FLF, stderr, "Calling malloc ...\n"));
    packet_sizes = (uint32_t*) malloc( sizeof(uint32_t) * (size_t)num_buffers );
    if( packet_sizes == NULL ) {
        mrn_dbg(1, mrn_printf(FLF, stderr,
                              "malloc failed for packet_sizes\n"));
        return -1;
    }

    mrn_dbg(5, mrn_printf(FLF, stderr, 
                          "Calling read(%p, %d) for %d buffer lengths.\n",
                          buf, buf_len, num_buffers));
    readRet = MRN_read(sock_fd, buf, buf_len);
    if( readRet != buf_len ) {
        mrn_dbg(3, mrn_printf(FLF, stderr, "MRN_read() %d of %d bytes received\n", readRet, buf_len));
        free(buf);
        //free(packet_sizes);
        return -1;
    }

    pdrmem_create(&pdrs, buf, (uint32_t)buf_len, op);
    if( ! pdr_vector(&pdrs, (char*)packet_sizes, num_buffers,
                     (uint32_t) sizeof(uint32_t), (pdrproc_t)pdr_uint32) ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "pdr_vector() failed\n"));
        error(ERR_PACKING, iinlet_rank, "pdr_uint32() failed\n");
        free(buf);
        free(packet_sizes);
        return -1;
    }
    free(buf);

    //
    // packets
    //
    
    /* recv packet buffers */
    ncbufs = (NCBuf_t*) malloc( sizeof(NCBuf_t) * (size_t)num_buffers );
    assert(ncbufs);

    mrn_dbg(5, mrn_printf(FLF, stderr, "Reading %u packets:\n", num_packets));

    for( i = 0; i < num_buffers; i++ ) {
        psz = packet_sizes[i];
        ncbufs[i].buf = (char*) malloc( (size_t)psz );
        assert(ncbufs[i].buf);
        ncbufs[i].len = psz;
        total_bytes += psz;
        mrn_dbg(5, mrn_printf(FLF, stderr, "- buffer[%u] has size %u\n", 
                              i, psz));
    }

    mrn_dbg(5, mrn_printf(FLF, stderr, 
                          "Reading num_buffers=%d\n", num_buffers));
    retval = 0;
    for(i = 0; i < num_buffers; i++ ) {
        readRet = MRN_read( sock_fd, ncbufs[i].buf, (size_t) ncbufs[i].len );
        if( readRet < 0 ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, 
                                  "MRN_read() failed for packet %u\n", i));
            break;
        }
        retval += readRet;
    }
    if( retval != total_bytes ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "%d of %d received\n", 
                              retval, total_bytes));

        for( i = 0; i < num_buffers; i++ )
            free((void*)(ncbufs[i].buf));
        free(ncbufs);
        free(packet_sizes);
        return -1;

    }

    //
    // post-processing
    //

    retval = 0;
    mrn_dbg(3, mrn_printf(FLF, stderr, "Creating Packets ...\n"));
    for( i = 0; i < num_buffers; i += 2 ) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "creating packet[%d]\n",i));

        new_packet = new_Packet_t_3( ncbufs[i].len, ncbufs[i].buf, 
                                     ncbufs[i+1].len, ncbufs[i+1].buf, 
                                     iinlet_rank );
        if( new_packet == NULL ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "packet creation failed\n"));
            retval = -1;
            break;
        }   

        pushBackElement(packets_in, new_packet);
    }

    // release dynamically allocated memory

    // Note: on success return, don't release the packet buffers; 
    //       that memory was passed to the Packet object(s).
    if( retval == -1) {
        for (i = 0; i < num_buffers; i++) {
            char * b = ncbufs[i].buf;
            free(b);
        }
    }
    free( ncbufs );
    free( packet_sizes );

    mrn_dbg_func_end();
    return retval;
}

int Message_send(Message_t* msg_out, int sock_fd)
{
    uint32_t num_buffers, num_packets;
    uint32_t *packet_sizes = NULL;
    char* buf = NULL;
    size_t buf_len;
    PDR pdrs;
    enum pdr_op op = PDR_ENCODE;
    NCBuf_t* ncbufs;
    int total_bytes, err, mcwret, sret;

    mrn_dbg(3, mrn_printf(FLF, stderr, "Sending packets from message %p\n", msg_out));

    if( msg_out->packet == NULL ) { // if there is no packet to send
        mrn_dbg(3, mrn_printf(FLF, stderr, "Nothing to send!\n"));
        return 0;
    }

    //
    // pre-processing
    //

    /* lightweight send is always single packet */
    num_packets = 1;
    num_buffers = num_packets * 2;

    ncbufs = (NCBuf_t*) calloc( (size_t) num_buffers, sizeof(NCBuf_t) );
    ncbufs[0].buf = msg_out->packet->hdr;
    ncbufs[0].len = msg_out->packet->hdr_len;
    ncbufs[1].buf = msg_out->packet->buf;
    ncbufs[1].len = msg_out->packet->buf_len;
    total_bytes = ncbufs[0].len + ncbufs[1].len;

    packet_sizes = (uint32_t*) malloc( sizeof(uint32_t) * num_buffers );
    assert(packet_sizes);
    packet_sizes[0] = ncbufs[0].len;
    packet_sizes[1] = ncbufs[1].len;


    //
    // packet count
    //

    buf_len = pdr_sizeof( (pdrproc_t) (pdr_uint32), &num_packets);
    assert(buf_len);
    buf = (char*)malloc(buf_len);
    assert(buf);
    pdrmem_create(&pdrs, buf, (uint32_t)buf_len, op);

    if( ! pdr_uint32(&pdrs, &num_packets) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_uint32() failed\n") );
        free(buf);
        free(ncbufs);
        free(packet_sizes);
        return -1;
    }

    mrn_dbg(5, mrn_printf(FLF, stderr, "writing packet count\n"));
    mcwret = MRN_write(sock_fd, buf, buf_len);
    if( mcwret != buf_len ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "MRN_write() failed\n"));
        free(buf);
        free(ncbufs);
        free(packet_sizes);
        return -1;
    }
    mrn_dbg(5, mrn_printf(FLF, stderr, "MRN_write() succeeded\n"));
    free(buf);

    //
    // packet size vector
    //

    buf_len = (num_buffers * sizeof(uint32_t)) + 1; // 1 extra bytes overhead
    buf = (char*) malloc( buf_len );
    assert(buf);
    pdrmem_create(&pdrs, buf, (uint32_t)buf_len, op);

    if( ! pdr_vector(&pdrs, (char*)(packet_sizes), num_buffers, 
                     (uint32_t) sizeof(uint32_t), (pdrproc_t) pdr_uint32) ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "pdr_vector() failed\n"));
        free(buf);
        free(ncbufs);
        free(packet_sizes);
        return -1;
    }

    mrn_dbg(5, mrn_printf(FLF, stderr, "write(%p, %d) of size vector\n", 
                          buf, buf_len));
    mcwret = MRN_write(sock_fd, buf, buf_len);
    if( mcwret != buf_len ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "MRN_write failed\n"));
        free(buf);
        free(ncbufs);
        free(packet_sizes);
        return -1;
    }
    free(packet_sizes);
    free(buf);

    //
    // packets
    //

    mrn_dbg(3, mrn_printf(FLF, stderr,
                          "Sending %d packets (%d total bytes)\n", 
                          num_packets, total_bytes));

    sret = NCSend(sock_fd, ncbufs, num_buffers);
    if( sret != total_bytes ) {
        err = NetUtils_GetLastError();
        mrn_dbg(1, mrn_printf(FLF, stderr,
                "NCSend() returned %d of %d bytes, errno = %d, nbuffers = %d\n", 
                              sret, total_bytes, err, num_packets));
        free(ncbufs);
        return -1;
    }
    free(ncbufs);

    mrn_dbg_func_end();
    return 0;
}

/*******************************************************************
 * Functions used to implement sending and receiving of some basic
 * data types
 ******************************************************************/

int MRN_write(int ifd, void *ibuf, size_t ibuf_len)
{
    int ret;

    // don't generate SIGPIPE
    int flags = MSG_NOSIGNAL;

    mrn_dbg(5, mrn_printf(FLF, stderr, "%d, %p, %d\n", ifd, ibuf, ibuf_len));

    ret = send(ifd, (char*)ibuf, ibuf_len, flags);

    mrn_dbg(5, mrn_printf(FLF, stderr, "send => %d\n", ret));
  
    return ret;
}

int MRN_read(int fd, void *buf, size_t count)
{
    size_t bytes_recvd = 0;
    int retval, err;

    if( count == 0 )
        return 0;

    while( bytes_recvd != count ) {

        mrn_dbg(5, mrn_printf(FLF, stderr, "About to call recv\n"));
        retval = recv( fd, ((char*)buf) + bytes_recvd,
                       count - bytes_recvd,
                       NCBlockingRecvFlag );
        mrn_dbg(5, mrn_printf(FLF, stderr, "recv returned retval=%d\n", retval));

        err = NetUtils_GetLastError();
        if( retval == -1 ) {
            if( err == EINTR ) {
                continue;
            }
            else {
                mrn_dbg(3, mrn_printf(FLF, stderr,
                                     "premature return from recv(). Got %d of %d" 
                                     " bytes. error: %s\n", 
                                      bytes_recvd, count, strerror(err)));
                return -1;
            }
        }
        else if( retval == 0 ) {
            // the remote endpoint has gone away
            return -1;
        }
        else {
            bytes_recvd += retval;
            if( bytes_recvd < count ) {
                continue;
            }
            else {
                mrn_dbg(5, mrn_printf(FLF, stderr, "returning %d\n", bytes_recvd));
                return bytes_recvd;
            }
        }
    }
    assert(0);
    return -1;
}
