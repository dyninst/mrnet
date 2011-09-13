/****************************************************************************
 *  Copyright 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined( __event_detector_h )
#define __event_detector_h 1

#ifndef os_windows
#include <poll.h>
#endif //os_windows

#include <map>
#include <set>

#include "PeerNode.h"
#include "TimeKeeper.h"

#include "xplat/Mutex.h"
#include "xplat/Thread.h"

#ifdef os_windows

struct pollfd {
    int   fd;         /* file descriptor */
    short events;     /* requested events */
    short revents;    /* returned events */
};
	
#define POLLIN 1

#endif

namespace MRN {

class EventDetector {
 
 public:

    EventDetector( Network* inetwork )
        : _network(inetwork), _thread_id(0), _pollfds(NULL), 
          _num_pollfds(0), _max_pollfds(0), _max_fd(-1)
    { }

    ~EventDetector(void)
    {
        _thread_id = 0;
        _network = NULL;
        if( _pollfds != NULL )
            free( _pollfds );
    }

    static void * main( void* iarg );
    static bool start( Network* inetwork );
    bool stop( void );

    XPlat::Thread::Id get_ThrId(void) const;

 private:

    int proc_NewChildFDConnection( PacketPtr ipacket, int isock );
    int init_NewChildFDConnection( PeerNodePtr iparent_node );

    int recover_FromChildFailure( Rank ifailed_rank );
    int recover_FromParentFailure( int& new_parent_sock );
    
    bool add_FD( int ifd );
    bool remove_FD( int ifd );
    int eventWait( std::set< int >& event_fds, int timeout,
                   bool use_poll/*=true*/ );

    void handle_Timeout( TimeKeeper*, int );

    void set_ThrId( XPlat::Thread::Id );

    mutable XPlat::Mutex _sync;
    Network* _network;
    XPlat::Thread::Id _thread_id;
    struct pollfd* _pollfds;
    unsigned int _num_pollfds, _max_pollfds;
    int _max_fd;
    std::map< int, Rank > childRankByEventDetectionSocket;
};

} // namespace MRN

#endif /* __event_detector_h */
