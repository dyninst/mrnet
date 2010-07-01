/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined( __event_detector_h )
#define __event_detector_h 1

#ifndef os_windows
#include <poll.h>
#endif //os_windows
#include <set>

#include "PeerNode.h"
#include "TimeKeeper.h"

#ifdef os_windows

	struct pollfd {
		int   fd;         /* file descriptor */
		short events;     /* requested events */
		short revents;    /* returned events */
	} ;
	
#define POLLIN 1

#endif

namespace MRN {

class EventDetector {
 
 public:
    static bool start( Network *inetwork );
    static bool stop( void );

 private:

    static void * main( void * iarg );

    static int init_NewChildFDConnection( Network * inetwork,
                                          PeerNodePtr iparent_node );

    static int recover_FromChildFailure( Network *inetwork, Rank ifailed_rank );
    static int recover_FromParentFailure( Network *inetwork );
    static int recover_off_FromParentFailure( Network *inetwork );
    
    static bool add_FD( int ifd );
    static bool remove_FD( int ifd );
    static int eventWait( std::set< int >& event_fds, int timeout,
                          bool use_poll/*=true*/ );

    static void handle_Timeout( TimeKeeper*, int );

    static long _thread_id;
    static struct pollfd* _pollfds;
    static unsigned int _num_pollfds, _max_pollfds;
    static int _max_fd;
};

} // namespace MRN

#endif /* __event_detector_h */
