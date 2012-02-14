/****************************************************************************
 *  Copyright 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(Message_h)
#define Message_h

#include <list>
#include <vector>

#include "mrnet/Error.h"
#include "mrnet/Types.h"
#include "mrnet/Packet.h"
#include "xplat/Monitor.h"
#include "mrnet/Network.h"
#include "xplat/NCIO.h"

#include "PerfDataEvent.h"
#include "PerfDataSysEvent.h"




namespace MRN
{

uint64_t get_TotalBytesSend(void);
uint64_t get_TotalBytesRecv(void);

class Message: public Error{
  public:
   Message( Network * net );
   ~Message();

   int send( XPlat::XPSOCKET isock_fd );
   int recv( XPlat::XPSOCKET isock_fd, std::list < PacketPtr >&opackets, Rank iinlet_rank );

   void add_Packet( PacketPtr );
   int size_Packets( void );
   
   void waitfor_MessagesToSend( void );

 private:

   Network * _net;
   enum {MRN_QUEUE_NONEMPTY};

   std::list< PacketPtr > _packets;
   XPlat::Monitor _packet_sync;

   int _packet_count_header;
   char * _packet_count_buf;
   int _packet_vector_sizes_size;
   char * _packet_vector_sizes_buf;
   uint64_t _packet_sizes[10];
   XPlat::NCBuf _ncbuf[10];
   uint32_t _ncbuf_count;
};

ssize_t MRN_send( XPlat::XPSOCKET fd, const char *buf, size_t count );
ssize_t MRN_recv( XPlat::XPSOCKET fd, char *buf, size_t count );

}                               // namespace MRN

#endif                          /* Message_h */
