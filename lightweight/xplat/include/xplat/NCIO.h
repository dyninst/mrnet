#if !defined(__ncio_h)
#define __ncio_h 1

//#include <sys/uio.h>
//#include <sys/socket.h>
#include <winsock2.h>

#if defined(WIN32)
typedef SOCKET XPSOCKET;
#else
typedef int XPSOCKET;
#endif // defined(WIN32)

typedef struct
{
  unsigned int len;
  char* buf;
} NCBuf_t;


NCBuf_t* new_NCBuf_t();

int NCSend( XPSOCKET s, NCBuf_t* bufs, unsigned int nBufs);

int NCRecv (XPSOCKET s, NCBuf_t** bufs, unsigned int nBufs);

#endif /* __ncio_h */
