/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(Message_h)
#define Message_h

#include <list>
#include <vector>

#include "utils.h"
#include "PerfDataEvent.h"
#include "PerfDataSysEvent.h"

#include "mrnet/Error.h"
#include "mrnet/Network.h"
#include "mrnet/Packet.h"
#include "xplat/Monitor.h"
#include "xplat/SocketUtils.h"

#define MESSAGE_PREALLOC_LEN 10

namespace MRN
{

uint64_t get_TotalBytesSend(void);
uint64_t get_TotalBytesRecv(void);

class Message: public Error{
 public:
    Message( Network * net );
    ~Message();

    int send( XPlat_Socket isock_fd );
    int recv( XPlat_Socket isock_fd, 
              std::list < PacketPtr >&opackets, Rank iinlet_rank );

    void add_Packet( PacketPtr );
    size_t size_Packets( void );
   
    void waitfor_MessagesToSend( void );

 private:

    Network * _net;
    enum {MRN_QUEUE_NONEMPTY};

    std::list< PacketPtr > _packets;
    XPlat::Monitor _packet_sync;
    XPlat::Monitor _send_sync;

    char *_packet_count_buf, *_packet_sizes_buf;
    uint64_t _packet_count_buf_len;
    size_t _packet_sizes_buf_len, _ncbuf_len;

    uint64_t _packet_sizes[MESSAGE_PREALLOC_LEN];
    XPlat::SocketUtils::NCBuf _ncbuf[MESSAGE_PREALLOC_LEN];
};

ssize_t MRN_send( XPlat_Socket fd, const char *buf, size_t count );
ssize_t MRN_recv( XPlat_Socket fd, char *buf, size_t count );

} // namespace MRN

#endif /* Message_h */
