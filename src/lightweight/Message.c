/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "mrnet_lightweight/MRNet.h"
#include "Message.h"
#include "Protocol.h"
#include "utils_lightweight.h"
#include "xplat_lightweight/vector.h"

#include "xplat_lightweight/NetUtils.h"
#include "xplat_lightweight/SocketUtils.h"

Message_t* new_Message_t()
{
    Message_t* new_message = (Message_t*)malloc(sizeof(Message_t));
    assert(new_message);
    new_message->packet = (Packet_t*)malloc(sizeof(Packet_t));
    assert(new_message->packet);

    return new_message;
}

int Message_recv(XPlat_Socket sock_fd, vector_t* packets_in, Rank iinlet_rank)
{
    int retval;
    unsigned int i;
    uint32_t num_packets = 0;
    uint32_t num_buffers = 0;
    PDR pdrs;
    enum pdr_op op = PDR_DECODE;
    size_t psz, recv_total, total_bytes = 0;
    uint64_t buf_len = 0;
    ssize_t readRet;
    XPlat_NCBuf_t* ncbufs;
    Packet_t* new_packet;
    uint64_t *packet_sizes;
    char *buf = NULL;
    
    mrn_dbg_func_begin();
 
    mrn_dbg(5, mrn_printf(FLF, stderr, "sock_fd = %d\n", sock_fd));

    //
    // packet count
    //

    mrn_dbg(5, mrn_printf(FLF, stderr, "Calling sizeof ...\n"));
    pdr_sizeof((pdrproc_t)(pdr_uint32), &num_packets, &buf_len);
    assert(buf_len);
    buf = (char*) malloc(buf_len + 1);
    assert(buf);

    mrn_dbg(3, mrn_printf(FLF, stderr, "Reading packet count\n"));

    readRet = MRN_recv(sock_fd, buf, buf_len + 1);
    if( readRet != (ssize_t)buf_len + 1) {
        mrn_dbg(3, mrn_printf(FLF, stderr, "MRN_recv() of packet count failed\n"));
        free(buf);
        return -1;
    }
    
    mrn_dbg(5, mrn_printf(FLF, stderr, "Calling memcreate ...\n"));
    pdrmem_create(&pdrs, &(buf[1]), buf_len, op, (pdr_byteorder) buf[0]);
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
    buf_len = (sizeof(uint64_t) * num_buffers) + 1; // 1 byte pdr overhead

    //mrn_dbg(5, mrn_printf(FLF, stderr, "Calling malloc(%u) ...\n", buf_len));
    buf = (char*) malloc( buf_len );
    if( buf == NULL ) {
        mrn_dbg(1, mrn_printf(FLF, stderr,
                              "malloc failed for buf\n"));
        return -1;
    }

    //mrn_dbg(5, mrn_printf(FLF, stderr, "Calling malloc ...\n"));
    packet_sizes = (uint64_t*) malloc( sizeof(uint64_t) * (size_t)num_buffers );
    if( packet_sizes == NULL ) {
        mrn_dbg(1, mrn_printf(FLF, stderr,
                              "malloc failed for packet_sizes\n"));
        return -1;
    }

    mrn_dbg(5, mrn_printf(FLF, stderr, 
                          "Calling read(%p, %"PRIu64") for %d buffer lengths.\n",
                          buf, buf_len, num_buffers));
    readRet = MRN_recv(sock_fd, buf, (size_t)buf_len);
    if( readRet != (ssize_t)buf_len ) {
        mrn_dbg(3, mrn_printf(FLF, stderr, "MRN_recv() %"PRIsszt" of %"PRIsszt" bytes received\n", readRet, buf_len));
        free(buf);
        //free(packet_sizes);
        return -1;
    }

    pdrmem_create(&pdrs, buf, buf_len, op, pdrmem_getbo());
    if( ! pdr_vector(&pdrs, (char*)packet_sizes, num_buffers,
                     (uint64_t) sizeof(uint64_t), (pdrproc_t)pdr_uint64) ) {
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
    ncbufs = (XPlat_NCBuf_t*) malloc( sizeof(XPlat_NCBuf_t) * (size_t)num_buffers );
    assert(ncbufs);

    mrn_dbg(5, mrn_printf(FLF, stderr, "Reading %u packets:\n", num_packets));

    for( i = 0; i < num_buffers; i++ ) {
        psz = (size_t) packet_sizes[i];
        ncbufs[i].buf = (char*) malloc(psz);
        assert(ncbufs[i].buf);
        ncbufs[i].len = psz;
        total_bytes += psz;
        mrn_dbg(5, mrn_printf(FLF, stderr, "- buffer[%u] has size %"PRIsszt"\n", 
                              i, psz));
    }

    mrn_dbg(5, mrn_printf(FLF, stderr, 
                          "Reading num_buffers=%d\n", num_buffers));
    recv_total = 0;
    for(i = 0; i < num_buffers; i++ ) {
        readRet = MRN_recv( sock_fd, ncbufs[i].buf, ncbufs[i].len );
        if( readRet < 0 ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, 
                                  "MRN_recv() failed for packet %u\n", i));
            break;
        }
        recv_total += (int) readRet;
    }
    if( recv_total != total_bytes ) {
        mrn_dbg(1, mrn_printf(FLF, stderr,
                              "%"PRIsszt" of %"PRIsszt" received\n", 
                              recv_total, total_bytes));

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
    if( retval == -1 ) {
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

int Message_send(Message_t* msg_out, XPlat_Socket sock_fd)
{
    uint32_t num_buffers, num_packets;
    uint64_t buf_len;
    PDR pdrs;
    enum pdr_op op = PDR_ENCODE;
    int err, go_away = 0;
    size_t total_bytes;
    ssize_t sret, mcwret;
    char* buf = NULL;

    XPlat_NCBuf_t ncbufs[2];
    uint64_t packet_sizes[2];

    mrn_dbg(3, mrn_printf(FLF, stderr, "Sending packets from message %p\n", msg_out));

    if( msg_out->packet == NULL ) { // if there is no packet to send
        mrn_dbg(3, mrn_printf(FLF, stderr, "Nothing to send!\n"));
        return 0;
    }

    if( (msg_out->packet->tag == PROT_SHUTDOWN) || 
        (msg_out->packet->tag == PROT_SHUTDOWN_ACK) )
        go_away = 1;

    //
    // pre-processing
    //

    /* lightweight send is always single packet */
    num_packets = 1;
    num_buffers = num_packets * 2;

    ncbufs[0].buf = msg_out->packet->hdr;
    ncbufs[0].len = msg_out->packet->hdr_len;
    ncbufs[1].buf = msg_out->packet->buf;
    ncbufs[1].len = (size_t)msg_out->packet->buf_len;
    total_bytes = ncbufs[0].len + ncbufs[1].len;

    packet_sizes[0] = ncbufs[0].len;
    packet_sizes[1] = ncbufs[1].len;


    //
    // packet count
    //

    pdr_sizeof((pdrproc_t)(pdr_uint32), &num_packets, &buf_len);
    assert(buf_len);
    buf = (char*) malloc(buf_len + 1);
    assert(buf);
    pdrmem_create(&pdrs, &(buf[1]), buf_len, op, pdrmem_getbo());

    if( ! pdr_uint32(&pdrs, &num_packets) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_uint32() failed\n") );
        free(buf);
        return -1;
    }
    buf[0] = (char) pdrmem_getbo();
    mrn_dbg(5, mrn_printf(FLF, stderr, "writing packet count\n"));
    mcwret = MRN_send(sock_fd, buf, buf_len + 1);
    if( mcwret != (ssize_t)buf_len + 1) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "MRN_send() failed\n"));
        free(buf);
        return -1;
    }
    mrn_dbg(5, mrn_printf(FLF, stderr, "MRN_send() succeeded\n"));
    free(buf);

    //
    // packet size vector
    //

    buf_len = (num_buffers * sizeof(uint64_t)) + 1; // 1 extra bytes overhead
    buf = (char*) malloc( buf_len );
    assert(buf);
    pdrmem_create(&pdrs, buf, (uint64_t)buf_len, op, pdrmem_getbo());

    if( ! pdr_vector(&pdrs, (char*)(packet_sizes), num_buffers, 
                     (uint64_t) sizeof(uint64_t), (pdrproc_t) pdr_uint64) ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "pdr_vector() failed\n"));
        free(buf);
        return -1;
    }

    mrn_dbg(5, mrn_printf(FLF, stderr, "write(%p, %d) of size vector\n", 
                          buf, buf_len));
    mcwret = MRN_send(sock_fd, buf, (size_t)buf_len);
    if( mcwret != (ssize_t)buf_len ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "MRN_send failed\n"));
        free(buf);
        return -1;
    }
    free(buf);

    //
    // packets
    //

    mrn_dbg(3, mrn_printf(FLF, stderr,
                          "Sending %d packets (%d total bytes)\n", 
                          num_packets, total_bytes));

    sret = XPlat_SocketUtils_Send(sock_fd, ncbufs, num_buffers);
    if( sret != (ssize_t)total_bytes ) {
        err = XPlat_NetUtils_GetLastError();
        mrn_dbg(1, mrn_printf(FLF, stderr,
                              "XPlat_SocketUtils_Send() returned %d of %d bytes,"
                              " errno = %d, nbuffers = %d\n", 
                              sret, total_bytes, err, num_packets));
        return -1;
    }

    if( go_away )
        XPlat_SocketUtils_Close(sock_fd);

    mrn_dbg_func_end();
    return 0;
}

/*******************************************************************
 * Functions used to implement sending and receiving of some basic
 * data types
 ******************************************************************/

ssize_t MRN_send(XPlat_Socket sock_fd, void *buf, size_t count)
{
    ssize_t rc;
    rc = XPlat_SocketUtils_send( sock_fd, buf, count );
    if( rc < 0 ) {
        return -1;
    }
    else
        return rc;
}

ssize_t MRN_recv(XPlat_Socket sock_fd, void *buf, size_t count)
{
    ssize_t rc;
    rc = XPlat_SocketUtils_recv( sock_fd, buf, count );
    if( rc < 0 ) {
        XPlat_SocketUtils_Close( sock_fd );
    }
    return rc;
}
