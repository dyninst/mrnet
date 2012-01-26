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

   int recv_orig( int sock_fd, std::list< PacketPtr > &packets_in,
                  Rank iinlet_rank );
   int send_orig( int sock_fd );
 private:

   Network * _net;
   enum {MRN_QUEUE_NONEMPTY};
   std::list < PacketPtr > _packets;
   XPlat::Monitor _packet_sync;
};
int MRN_send( XPlat::XPSOCKET fd, const char *buf, int count );
int MRN_recv( XPlat::XPSOCKET fd, char *buf, int count );
#if READY
int read( int fd, void *buf, int size );
int write( int fd, const void *buf, int size );
#endif // READY

}                               // namespace MRN

#endif                          /* Message_h */
