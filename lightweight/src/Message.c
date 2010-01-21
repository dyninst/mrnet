/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "mrnet/MRNet.h"
#include "Message.h"
#include "Utils.h"
#include "vector.h"

#include "xplat/NCIO.h"

#ifdef os_windows
#include <winsock2.h>
const int NCBlockingRecvFlag = 0;
#else
const int NCBlockingRecvFlag = MSG_WAITALL;
#endif



Message_t* new_Message_t()
{
    Message_t* new_message = (Message_t*)malloc(sizeof(Message_t));
    assert(new_message);
    new_message->packet = (Packet_t*)malloc(sizeof(Packet_t));
    assert(new_message->packet);

    return new_message;
}

int Message_recv(Network_t* net, int sock_fd, vector_t* packets_in, Rank iinlet_rank)
{
  unsigned int i;
  int32_t buf_len;
  uint32_t no_packets=0;
  uint32_t *packet_sizes;
  char *buf = NULL;
  PDR pdrs;
  enum pdr_op op = PDR_DECODE;
  int retval;
  int readRet;
  NCBuf_t** ncbufs;
  int total_bytes = 0;
  Packet_t* new_packet;
    
  mrn_dbg_func_begin();
 
  mrn_dbg(5, mrn_printf(FLF, stderr, "called with sock_fd = %d\n", sock_fd));

  // packet count

  /* find out how many packets are coming */
  mrn_dbg(5, mrn_printf(FLF, stderr, "Calling sizeof ...\n"));
  buf_len = pdr_sizeof( (pdrproc_t) (pdr_uint32), &no_packets);
  assert(buf_len);
  buf = (char*)malloc(buf_len);
  assert(buf);

  mrn_dbg(3, mrn_printf(FLF, stderr, "read(%d, %p, %d) ...\n", sock_fd, buf, buf_len));

  if ((retval = MRN_read(net, sock_fd, buf, buf_len)) != buf_len) {
    if (retval == -1)
      error(ERR_SYSTEM, iinlet_rank, "MRN_read() %s", strerror(errno));
    mrn_dbg(3, mrn_printf(FLF, stderr, "MRN_read() %d of %d bytes received\n", retval, buf_len));
    free(buf);
    return -1;
  }

  // pdrmem initialize
  mrn_dbg(3, mrn_printf(FLF, stderr, "Calling memcreate ...\n"));
  pdrmem_create(&pdrs, buf, buf_len, op);
  mrn_dbg(3, mrn_printf(FLF, stderr, "Calling uint32 ...\n"));
  if (!pdr_uint32(&pdrs, &no_packets)) {
    error(ERR_PACKING, iinlet_rank, "pdr_uint32() failed\n");
    free(buf);
    return -1;
  } 
  mrn_dbg(3, mrn_printf(FLF, stderr, "Calling free ...\n"));
  free(buf);
  mrn_dbg(3, mrn_printf(FLF, stderr, "pdr_uint32() succeeded. Received %d packets\n", no_packets));

  assert(no_packets<2000);

  if (no_packets == 0) {
    mrn_dbg(2, mrn_printf(FLF, stderr, "warning: Receiving %d packets\n", no_packets));
  }

  // packet size vector

  /* recv a vector of packet sizes */
  //buf_len's value is hardcode, breaking pdr encapsulation barrier
  mrn_dbg(3, mrn_printf(FLF, stderr, "Calling malloc ...\n"));
  buf_len = (sizeof(uint32_t)*no_packets)+1; // 1 byte pdr overhead
  buf = (char*)malloc(buf_len);
  assert(buf);

  mrn_dbg(3, mrn_printf(FLF, stderr, "Calling malloc ...\n"));
  packet_sizes = (uint32_t*)malloc(sizeof(uint32_t)*no_packets);
  if (packet_sizes == NULL) {
    mrn_dbg(1, mrn_printf(FLF, stderr,
                          "recv: packet_size malloc is NULL for %d packets\n", no_packets));
  } 
  assert(packet_sizes);

  mrn_dbg(3, mrn_printf(FLF, stderr, 
              "Calling read(%d, %p, %d) for %d buffer lengths.\n",
              sock_fd, buf, buf_len, no_packets));
  readRet = MRN_read(net, sock_fd, buf, buf_len);
  if (readRet != buf_len) {
    if (readRet == -1)
      error(ERR_SYSTEM, iinlet_rank, "MRN_read() %s", strerror(errno));
    mrn_dbg(3, mrn_printf(FLF, stderr, "MRN_read() %d of %d bytes received\n", readRet, buf_len));
    free(buf);
    //free(packet_sizes);
    return -1;
  }

  // packets

  mrn_dbg(3, mrn_printf(FLF, stderr, "Calling pdrmem_create ...\n"));
  pdrmem_create(&pdrs, buf, buf_len, op);
  if (!pdr_vector(&pdrs, (char*)(packet_sizes), no_packets,
                  sizeof(uint32_t), (pdrproc_t)pdr_uint32)) {
    mrn_dbg(1, mrn_printf(FLF, stderr, "pdr_vector() failed\n"));
    error(ERR_PACKING, iinlet_rank, "pdr_uint32() failed\n");
    free(buf);
    free(packet_sizes);
    return -1;
  }
  free(buf);

  /* recv packet buffers */
  //NCBuf_t* ncbufs[no_packets];
  ncbufs = (NCBuf_t**)malloc(sizeof(NCBuf_t)*no_packets);

  mrn_dbg(3, mrn_printf(FLF, stderr, "Reading %d packets of size: [", no_packets));

  for (i = 0; i < no_packets; i++) {
    ncbufs[i] = new_NCBuf_t();
    ncbufs[i]->buf = (char*)malloc(packet_sizes[i]);
    assert(ncbufs[i]->buf);
    ncbufs[i]->len = packet_sizes[i];
    total_bytes += packet_sizes[i];
    mrn_dbg(3, mrn_printf(0,0,0, stderr, "%d, ", packet_sizes[i]));
  }
  mrn_dbg(3, mrn_printf(0,0,0, stderr, "]\n" ));

  mrn_dbg(3, mrn_printf(FLF, stderr, "Calling NCRecv with no_packets=%d\n", no_packets));
  retval = NCRecv(sock_fd, ncbufs, no_packets);
  if (retval != total_bytes) {
    if (retval == -1){
      error(ERR_SYSTEM, iinlet_rank, "NCRecv() %s", strerror(errno));
      perror("NCRecv()");
    }
    mrn_dbg(3, mrn_printf(FLF, stderr, "NCRecv %d of %d received\n", retval, total_bytes));

    for (i = 0; i < no_packets; i++)
      free((void*)(ncbufs[i]->buf));
    free(ncbufs);
    free(packet_sizes);
    return -1;

  }

  // post-processing

  mrn_dbg(3, mrn_printf(FLF, stderr, "Creating Packets ...\n"));
  for (i = 0; i < no_packets; i++) {
    mrn_dbg(3, mrn_printf(FLF, stderr, "Creating packet[%d] ...\n",i));

    new_packet = new_Packet_t_3(ncbufs[i]->len, ncbufs[i]->buf, iinlet_rank);

    if (new_packet == NULL) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "packet creation failed\n"));

        for (i = 0; i < no_packets; i++)
                free((void*)(ncbufs[i]->buf));
        free (ncbufs);
        free(packet_sizes);
        return -1;
    }   

    pushBackElement(packets_in, new_packet);
  }

  // release dynamically allocated memory
  //free(packet_sizes);
  mrn_dbg(3, mrn_printf(FLF, stderr, "Msg(_recv() succeeded\n"));

  return 0;

}

int Message_send(Network_t* net, Message_t* msg_out, int sock_fd)
{
  /* send an array of packet sizes */
  unsigned int i;
  uint32_t no_packets = 1;
  uint32_t *packet_sizes = NULL;
  char* buf=NULL;
  int buf_len;
  PDR pdrs;
  enum pdr_op op = PDR_ENCODE;
  NCBuf_t* ncbuf = new_NCBuf_t();
  int total_bytes;
  int mcwret;
  int sret;

  mrn_dbg(3, mrn_printf(FLF, stderr, "Sending packets from message %p\n", msg_out));

  if (msg_out->packet == NULL) { // if there is no packet to send
    mrn_dbg(3, mrn_printf(FLF, stderr, "Nothing to send!\n"));
    //_packet_sync.Unlock();
    return 0;
  }

  packet_sizes = (uint32_t*)malloc(sizeof(uint32_t) * no_packets);
  assert(packet_sizes);

  // Process packet to prepare for send()
  //ncbuf->buf = (char*)malloc(sizeof(msg_out->packet->buf_len));
  //assert(ncbuf->buf);
  ncbuf->buf = msg_out->packet->buf;
  ncbuf->len = msg_out->packet->buf_len;
  packet_sizes[0] = ncbuf->len;
  total_bytes = ncbuf->len;

  /* put how many packets are going (always 1) */

  // we're only sending one packet at a time, so size always = 1
  buf_len = pdr_sizeof( (pdrproc_t) (pdr_uint32), &no_packets);
  assert(buf_len);
  buf = (char*)malloc(buf_len);
  assert(buf);
  pdrmem_create(&pdrs, buf, buf_len, op);

  if (!pdr_uint32(&pdrs, &no_packets)) {
    error(ERR_PACKING, UnknownRank, "pdr_uint32() failed\n");
    free(buf);
    free(ncbuf);
    free(packet_sizes);
    return -1;
  }
  
  mrn_dbg(3, mrn_printf(FLF, stderr, "Calling write(%d, %p, %d to tell how many packets are going\n", sock_fd, buf, buf_len));
  if (MRN_write(net, sock_fd, buf, buf_len) != buf_len) {
    mrn_dbg(1, mrn_printf(FLF, stderr, "write() failed\n"));
    perror("write()");
    free(buf);
    free(ncbuf);
    free(packet_sizes);
    return -1;
  }

  mrn_dbg(3, mrn_printf(FLF, stderr, "write() succeeded\n"));
  free(buf);

  /* send a vector of packet_sizes */
  buf_len = (no_packets * sizeof(uint32_t)) + 1; // 1 extra bytes overhead
  buf = (char*)malloc(buf_len);
  assert(buf);
  pdrmem_create(&pdrs, buf, buf_len, op);

  if(!pdr_vector
      (&pdrs, (char*)(packet_sizes), no_packets, sizeof(uint32_t),
        (pdrproc_t) pdr_uint32))
  {
    mrn_dbg(1, mrn_printf(FLF, stderr, "pdr_vector() failed\n"));
    error(ERR_PACKING, UnknownRank, "pdr_vector() failed\n");
    free(buf);
    free(ncbuf);
    free(packet_sizes);
    return -1;
  }

  mrn_dbg(3, mrn_printf(FLF, stderr, "Calling write(%d, %p, %d)\n", sock_fd, buf, buf_len));
  mcwret = MRN_write(net, sock_fd, buf, buf_len);
  if (mcwret != buf_len) {
    mrn_dbg(1, mrn_printf(FLF, stderr, "write failed\n"));
    perror("write()");
    free(buf);
    free(ncbuf);
    free(packet_sizes);
    return -1;
  }

  free(packet_sizes);
  free(buf);

  // send the packets
  mrn_dbg(3, mrn_printf(FLF, stderr,
          "Calling NCSend(%d buffers, %d total bytes)\n", 
          no_packets, total_bytes));

  mrn_dbg(5, mrn_printf(FLF, stderr, 
          "...ncbuf->len=%d, ncbuf->buf=%s\n", ncbuf->len, ncbuf->buf));

  sret = NCSend(sock_fd, ncbuf, no_packets);
  if (sret != total_bytes) {
    mrn_dbg(3, mrn_printf(FLF, stderr,
            "NCSend() returned %d of %d bytes, errno = %d, nbuffers = %d\n", sret, total_bytes, errno, no_packets));

    mrn_dbg(5, mrn_printf(FLF, stderr, "buffer.size = %d\n", ncbuf->len));

    perror("NCSend()");
    error (ERR_SYSTEM, UnknownRank, "NCSend() returned %d of %d bytes: %s\n", 
            sret, total_bytes, strerror(errno));
    free(ncbuf);
    return -1;
  }
  
  //MRN_bytes_send.Add(sret);

  free(ncbuf);
  mrn_dbg(3, mrn_printf(FLF, stderr, "msg(%p)_send() succeeded\n", msg_out));
  return 0;

}

/*******************************************************************
 * Functions used to implement sending and receiving of some basic
 * data types
 ******************************************************************/

int MRN_write(Network_t* net, int ifd, /* const */ void *ibuf, int ibuf_len)
{

    int ret;

    mrn_dbg(3, mrn_printf(FLF, stderr, "%d, %p, %d\n", ifd, ibuf, ibuf_len));

    ret = send(ifd, (char*)ibuf, ibuf_len, 0);

    mrn_dbg(3, mrn_printf(FLF, stderr, "send => %d\n", ret));
  
    return ret;

}

int MRN_read(Network_t* net, int fd, void *buf, int count)
{
  int bytes_recvd = 0, retval;

  while (bytes_recvd != count) {
    mrn_dbg(5, mrn_printf(FLF, stderr, "About to call recv\n"));
    retval = recv(fd, ((char*)buf) + bytes_recvd,
                  count - bytes_recvd,
                  NCBlockingRecvFlag);
    mrn_dbg(5, mrn_printf(FLF, stderr, "recv returned retval=%d\n", retval));
    if (retval == -1) {
      if (errno == EINTR) {
        continue;
      }
      
      else {
        mrn_dbg(3,mrn_printf(FLF, stderr,
                            "premature return from read(). Got %d of %d" 
                            " bytes. error: %s\n", bytes_recvd, count, strerror(errno)));
        return -1;
      }
    }
    else if ( (retval == 0) && (errno == EINTR)) {
      // this situation has been seen to occur on Linux
      // when the remote endpoint has gone away
      return -1;
    }
    else {
      bytes_recvd += retval;
      if (bytes_recvd < count && errno == EINTR) {
        continue;
      }
      else {
        // bytes_recvd is either count, or error other than "EINTR" has occurred
        if (bytes_recvd != count) {
          mrn_dbg(3, mrn_printf(FLF, stderr, "premature return from read(). %d of %d bytes. errno: %s\n", bytes_recvd, count, strerror(errno)));
        }
        mrn_dbg(5, mrn_printf(FLF, stderr, "MRN_read returning %d\n", bytes_recvd));
        return bytes_recvd;
      }
    }
  }
  
  assert(0);
  return -1;
}
