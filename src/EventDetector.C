/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "BackEndNode.h"
#include "ChildNode.h"
#include "EventDetector.h"
#include "FrontEndNode.h"
#include "InternalNode.h"
#include "ParentNode.h"
#include "ParsedGraph.h"
#include "PeerNode.h"
#include "Router.h"
#include "utils.h"

#include "mrnet/MRNet.h"
#include "mrnet/NetworkTopology.h"
#include "xplat/NetUtils.h"
#include "xplat/SocketUtils.h"

#include <sstream>
#include <cxxabi.h>
using namespace std;

namespace MRN {

#if 0
bool EventDetector::stop( )
{
    int rc;

    mrn_dbg_func_begin();

    if( 0 == _thread_id )
        return true;

    if( _network->_edt != NULL ) {

        if( _network->is_LocalNodeParent() ) {
            // send KILL_SELF message to EDT on listening port
            Port edt_port;
            XPlat_Socket sock_fd=XPlat::SocketUtils::InvalidSocket;
            string edt_host;
            Message msg(_network);
            PacketPtr packet( new Packet(CTL_STRM_ID, PROT_KILL_SELF, NULL) );
            edt_host = _network->get_LocalHostName();
            edt_port = _network->get_LocalPort();
            mrn_dbg(3, mrn_printf( FLF, stderr, "Telling EDT(%s:%d) to go away\n",
                                   edt_host.c_str(), edt_port )); 
            if( connectHost( &sock_fd, edt_host, edt_port ) == -1 ) {
                mrn_dbg(1, mrn_printf(FLF, stderr, "connectHost(%s:%d) failed\n",
                                      edt_host.c_str(), edt_port ));
                return false;
            }
            msg.add_Packet( packet );
            if( msg.send( sock_fd ) == -1 ) {
                mrn_dbg(1, mrn_printf(FLF, stderr, "Message.sendDirectly() failed\n" ));
                return false;
            }
            XPlat::SocketUtils::Close( sock_fd );    
        }
        else {
            // backends don't have a listening port, so cancel EDT

            mrn_dbg( 5, mrn_printf(FLF, stderr, "about to cancel EDT\n") );

            // turn off debug output to prevent mrn_printf deadlock
            MRN::set_OutputLevel( -1 );

            _sync.Lock();
            if( _thread_id ) {
                rc = XPlat::Thread::Cancel( _thread_id );
                if( rc != 0 ) {
                    _sync.Unlock();
                    return false;
                }
            }
            _sync.Unlock();
        }

        // wait for EDT to exit
        if( _thread_id ) {
            rc = XPlat::Thread::Join( _thread_id, (void**)NULL );
            if( rc != 0 )
                return false;
        }
    }

    mrn_dbg_func_end();
    return true;
}
#endif

bool EventDetector::start( Network* inetwork )
{
    XPlat::Thread::Id thread_id = 0;
    mrn_dbg(3, mrn_printf(FLF, stderr, "Creating Event Detection thread ...\n"));

    if( XPlat::Thread::Create( main, (void*)inetwork->_edt, &thread_id ) == -1 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Thread creation failed...\n"));
        return false;
    }

    // record the thread id
    inetwork->_edt->set_ThrId( thread_id );

    mrn_dbg_func_end();
    return true;
}

bool EventDetector::add_FD( XPlat_Socket ifd )
{
    int nchild;

    mrn_dbg_func_begin();

    if( ifd < 0 ) return false;

    if( _pollfds == NULL ) {
        nchild = 256; /* a huge fan-out ; bigger than we ever expect */
        mrn_dbg( 5, mrn_printf(FLF, stderr,
                               "allocating %u slots for pollfds\n", nchild) );
        _pollfds = (struct pollfd*) malloc( nchild * sizeof(struct pollfd) );
        if( _pollfds == NULL ) return false;

        _num_pollfds = 0;
        _max_pollfds = nchild;
    }
    else if( _num_pollfds == _max_pollfds ) {

        struct pollfd* newfds = NULL;
        nchild = _max_pollfds * 2;

        newfds = (struct pollfd*) realloc( (void*)_pollfds, nchild * sizeof(struct pollfd) );
        if( newfds == NULL ) {
            mrn_dbg( 5, mrn_printf(FLF, stderr,
                                   "can't add, already have %u pollfds and realloc failed\n", _max_pollfds) );
            return false;
        }
        _pollfds = newfds;
        _max_pollfds = nchild;
    }

    if( ifd > _max_fd )
        _max_fd = ifd;
    
    _pollfds[_num_pollfds].fd = ifd;
    _pollfds[_num_pollfds].events = POLLIN;
    _pollfds[_num_pollfds].revents = 0;
    mrn_dbg( 5, mrn_printf(FLF, stderr, "added fd %d\n", ifd) );
 
    _num_pollfds++;  

    return true;
}

bool EventDetector::remove_FD( XPlat_Socket ifd )
{
    XPlat_Socket new_max = -1;
    unsigned int i, j;

    mrn_dbg_func_begin();

    if( ifd < 0 ) return false;

    for( i=0; i < _num_pollfds; i++ ) {
        if( _pollfds[i].fd == ifd )
            break;
        else if( _pollfds[i].fd > new_max )
            new_max = _pollfds[i].fd;
    }
    if( i == _num_pollfds ) return false;

    // shift rest of array
    for( j=i; j < _num_pollfds-1; j++ ) {
        _pollfds[j].fd = _pollfds[j+1].fd;
        if( _pollfds[j].fd > new_max )
            new_max = _pollfds[j].fd;
    }

    _num_pollfds--;
    _max_fd = new_max;

    return true;
}


int EventDetector::eventWait( std::set< XPlat_Socket >& event_fds, int timeout_ms, 
                              bool use_poll=true )
{
    int retval, err;
    fd_set readfds;

//    mrn_dbg( 5, mrn_printf(FLF, stderr,
//                           "waiting on %u fds\n", _num_pollfds) );
  
#ifdef os_windows
    use_poll=false;
#else
    if( use_poll ) { 

        retval = poll( _pollfds, _num_pollfds, timeout_ms );
        err = errno;
//        mrn_dbg( 5, mrn_printf(FLF, stderr,
//                               "poll() returned %d\n", retval) );
    }
    else { // select
#endif
        FD_ZERO( &readfds );
        for( unsigned int num=0; num < _num_pollfds; num++ )
            FD_SET( _pollfds[num].fd, &readfds );
 
        struct timeval* tvp = NULL;
        struct timeval tv = {0,0};
        if( timeout_ms != -1 ) {
            if( timeout_ms > 0 ) {
                unsigned extra_ms = timeout_ms % 1000;
                tv.tv_sec = (timeout_ms - extra_ms) / 1000;
                tv.tv_usec = extra_ms * 1000;
            }
            tvp = &tv;
        }

        retval = select( _max_fd+1, &readfds, NULL, NULL, tvp );
        err = errno;
//        mrn_dbg( 5, mrn_printf(FLF, stderr,
//                               "select() returned %d\n", retval) );
#ifndef os_windows
    }
#endif
    if( retval == -1 && err != EINTR )
        mrn_dbg( 5, mrn_printf(FLF, stderr, "select/poll() failed with %s\n",
                               strerror(err)) );

    if( retval <= 0 )
        return retval;
    
    for( unsigned int num=0; num < _num_pollfds; num++ ) {
        bool add = false;
        if( use_poll ) {
            int pfd = _pollfds[num].fd;
            int revts = _pollfds[num].revents;
            if( revts & POLLIN ) {
                mrn_dbg( 5, mrn_printf(FLF, stderr, "poll() fd %d POLLIN\n",
                                       pfd) );
                add = true;
            }
            else if( (revts & POLLHUP) || 
                     (revts & POLLERR) || 
                     (revts & POLLNVAL) ) {
                mrn_dbg( 5, mrn_printf(FLF, stderr, "poll() fd %d POLLHUP | POLLERR | POLLNVAL\n",
                                       pfd) );
                add = true;
            }
            else if( revts ) {
                mrn_dbg( 5, mrn_printf(FLF, stderr, 
                                       "poll() fd %d unhandled condition revents=%x\n",
                                       pfd, revts) );
            }
            _pollfds[num].revents = 0;
        }
        else { // select
            if( FD_ISSET( _pollfds[num].fd, &readfds ) )
                add = true;
        }
        if( add ) {
            event_fds.insert( (XPlat_Socket)_pollfds[num].fd );
            mrn_dbg( 5, mrn_printf(FLF, stderr,
                                   "activity on fd %d\n", _pollfds[num].fd) );
        }
    }

    return retval;    
}

void EventDetector::handle_Timeout( TimeKeeper* tk, int elapsed_ms )
{
    std::set< unsigned int > elapsed_strms;

    //mrn_dbg_func_begin();

    assert( tk != NULL );
    tk->notify_Elapsed( elapsed_ms, elapsed_strms );

    if( elapsed_strms.size() > 0 ) {

	std::set< unsigned int >::iterator siter = elapsed_strms.begin();
	for( ; siter != elapsed_strms.end(); siter++ ) {

	    Stream* strm = _network->get_Stream( *siter );
	    if( strm == NULL ) continue;

	    // push NullPacket to indicate timeout
	    vector<PacketPtr> opackets, opackets_rev;
	    strm->push_Packet( Packet::NullPacket,
			       opackets, opackets_rev,
			       true );

	    // forward output packets as necessary
	    if( ! opackets.empty() ) {
	        if( _network->is_LocalNodeFrontEnd() ) {
		    for( unsigned int i = 0; i < opackets.size(); i++ ) {
		        PacketPtr cur_packet( opackets[i] );
			mrn_dbg( 3, mrn_printf(FLF, stderr, "Put packet in stream %d\n",
					       cur_packet->get_StreamId() ));
			strm->add_IncomingPacket( cur_packet );
		    }
		}
		else {
		    if( _network->send_PacketsToParent( opackets ) == -1 ) {
			 mrn_dbg(1, mrn_printf(FLF, stderr, "send_PacketsToParent() failed\n"));
		    }
		}
		opackets.clear();
	    }
	    if( ! opackets_rev.empty() ) {
		if( _network->is_LocalNodeBackEnd() ) {
		    for( unsigned int i = 0; i < opackets_rev.size(); i++ ) {
			PacketPtr cur_packet( opackets_rev[i] );
			mrn_dbg( 3, mrn_printf(FLF, stderr, "Put packet in stream %d\n",
					       cur_packet->get_StreamId() ));
			strm->add_IncomingPacket( cur_packet );
		    }
		}
		else {
		    if( _network->send_PacketsToChildren( opackets ) == -1 ) {
			mrn_dbg(1, mrn_printf(FLF, stderr, "send_PacketsToChildren() failed\n"));
		    }
		}
		opackets_rev.clear();
	    }
	}
    }

    //mrn_dbg_func_end();
}

void * EventDetector::main( void* iarg )
{
  try {
    list< XPlat_Socket > watch_list; //list of sockets to detect events on
    XPlat_Socket parent_sock = XPlat::SocketUtils::InvalidSocket;
    XPlat_Socket local_sock = XPlat::SocketUtils::InvalidSocket;

    EventDetector* edt = (EventDetector*) iarg;
    Network* net = edt->_network;;
 
    PeerNodePtr  parent_node = PeerNode::NullPeerNode;
    if( net->is_LocalNodeChild() ) {
        parent_node = net->get_ParentNode();
    }
    
    //TLS: set up thread local storage
    Rank myrank = net->get_LocalRank();
    string prettyHost;
    XPlat::NetUtils::GetHostName( net->get_LocalHostName(), prettyHost );
    std::ostringstream namestr;
    namestr << "EDT("
            << prettyHost
            << ':'
            << myrank
            << ')' ;
    net->init_ThreadState( UNKNOWN_NODE, namestr.str().c_str() );

    srand48( net->get_LocalRank() );
    
    //(1) Establish connection with parent Event Detection Thread
    if( net->is_LocalNodeChild() ) {
        if( edt->init_NewChildFDConnection( parent_node ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "init_NewChildFDConnection() failed\n") );
        }
        mrn_dbg( 5, mrn_printf(FLF, stderr, "event connection successful.\n"));

        //monitor parent sock for failure
        parent_sock = parent_node->get_EventSocketFd();
        edt->add_FD(parent_sock);

        watch_list.push_back( parent_sock );
        mrn_dbg( 5, mrn_printf(FLF, stderr,
                               "Parent socket:%d added to list.\n", parent_sock));
    }
    else mrn_dbg(5, mrn_printf(FLF, stderr, "not a child\n"));

    if( net->is_LocalNodeParent() ) {
        //(2) Add local socket to event list
        local_sock = net->get_ListeningSocket();
        if( local_sock != XPlat::SocketUtils::InvalidSocket ) {
            edt->add_FD(local_sock);
            mrn_dbg( 5, mrn_printf(FLF, stderr,
                                   "Monitoring local socket:%d.\n", local_sock));
        }
    }

    //3) do EventDetection Loop, current events are:
    //   - PROT_KILL_SELF 
    //   - PROT_NEW_CHILD_FD_CONNECTION (a new child peer to monitor)
    //   - PROT_NEW_CHILD_DATA_CONNECTION (a new child peer for data)
    //   - socket failures
    ParentNode* p;
    Message msg(net);
    list< PacketPtr > packets;
    mrn_dbg( 5, mrn_printf(FLF, stderr, "starting main loop\n"));
    while( true ) {

        if( net->is_LocalNodeBackEnd() && edt->is_Disabled() ) break;

        Timer waitTimer;
        int timeout = -1; /* block */
        TimeKeeper* tk = NULL;

        tk = net->get_TimeKeeper();
        if( tk != NULL ) {
            timeout = tk->get_Timeout();
            waitTimer.start();
        }

        //mrn_dbg( 5, mrn_printf(FLF, stderr, "eventWait(timeout=%dms)\n", timeout));
        std::set< XPlat_Socket > eventfds;
        int retval = edt->eventWait( eventfds, timeout );
        if( retval == -1 ) {
            continue;
        }
        else if( retval == 0 ) {
            // timed-out, calculate elapsed time
            double elapsed_ms;
            int elapsed;
            waitTimer.stop();
            elapsed_ms = waitTimer.get_latency_msecs();
            elapsed = (int)elapsed_ms;
            if( 0 == elapsed ) elapsed++;
            //mrn_dbg( 5, mrn_printf(FLF, stderr, "timeout after %d ms\n", elapsed));

            // notify streams with registered timeouts
            edt->handle_Timeout( tk, elapsed );
            continue;
        }
        else {
            mrn_dbg( 5, mrn_printf(FLF, stderr, "checking event fds\n") );

            if( eventfds.find(local_sock) != eventfds.end() ) {
                //Activity on our local listening sock, accept connection
                mrn_dbg( 5, mrn_printf(FLF, stderr, 
                                       "activity on listening socket\n") );

                while(true) { // keep accepting until no more connections

                    XPlat_Socket connected_sock;
                    if( ! XPlat::SocketUtils::AcceptConnection(local_sock, 
                                                               connected_sock, 
                                                               0, true /*non-blocking*/) )
                        break;

                    packets.clear();
                    int recv_ret = msg.recv( connected_sock, packets, UnknownRank );
                    if( -1 == recv_ret ) {
                        XPlat::SocketUtils::Close(connected_sock);
                        break;
                    }
                    list< PacketPtr >::iterator packet_list_iter;
                    list< XPlat_Socket >::iterator iter;

                    for( packet_list_iter = packets.begin();
                         packet_list_iter != packets.end();
                         packet_list_iter++ ) {

                        PacketPtr cur_packet( *packet_list_iter );
                        switch ( cur_packet->get_Tag() ) {

                        case PROT_EDT_SHUTDOWN:
                            mrn_dbg(5, mrn_printf(FLF, stderr, "PROT_EDT_SHUTDOWN\n"));

                            if( -1 == net->broadcast_ShutDown() ) {
                                mrn_dbg(1, mrn_printf(FLF, stderr,
                                        "Failed to broadcast shutdown to children!\n"));
                            }

                            edt->set_ThrId( 0 );
                            mrn_dbg(5, mrn_printf(FLF, stderr, "I'm going away now!\n"));
                            Network::free_ThreadState();
                            return NULL;

                        case PROT_NEW_CHILD_FD_CONNECTION:
                            mrn_dbg(5, mrn_printf(FLF, stderr, "PROT_NEW_CHILD_FD_CONNECTION\n"));
                            if( ! edt->is_Disabled() ) {
                                edt->add_FD(connected_sock);
                                watch_list.push_back( connected_sock );
                                mrn_dbg(5, mrn_printf(FLF, stderr,
                                            "FD socket:%d added to list.\n",
                                            connected_sock));

                                edt->proc_NewChildFDConnection( cur_packet, connected_sock );
                            } else {
                                mrn_dbg(5, mrn_printf(FLF, stderr, "...disabled\n"));
                            }
                            break;

                        case PROT_NEW_CHILD_DATA_CONNECTION:
                            mrn_dbg(5, mrn_printf(FLF, stderr, "PROT_NEW_CHILD_DATA_CONNECTION\n"));
                            if( ! edt->is_Disabled() ) {
                                //get ParentNode obj. Try internal node, then FE
                                p = net->get_LocalParentNode();
                                assert(p);

                                p->proc_NewChildDataConnection( cur_packet,
                                                                connected_sock );
                                mrn_dbg( 5, mrn_printf(FLF, stderr, "New child connected.\n"));
                            } else {
                                mrn_dbg(5, mrn_printf(FLF, stderr, "...disabled\n"));
                            }
                            break;

                        case PROT_SUBTREE_INITDONE_RPT:
                            // NOTE: may arrive in a group with NEW_CHILD_DATA_CONNECTION
                            mrn_dbg( 5, mrn_printf(FLF, stderr, "PROT_SUBTREE_INITDONE_RPT\n"));
                            if( ! edt->is_Disabled() ) {
                                p = net->get_LocalParentNode();
                                assert(p);
                                if( p->proc_SubTreeInitDoneReport( cur_packet ) == -1 ) {
                                    mrn_dbg( 1, mrn_printf(FLF, stderr, 
                                                "proc_SubTreeInitDoneReport() failed\n" ));
                                }
                            } else {
                                mrn_dbg(5, mrn_printf(FLF, stderr, "...disabled\n"));
                            }
                            break;

                        default:
                            mrn_dbg(1, mrn_printf(FLF, stderr, 
                                                  "### PROTOCOL ERROR: Unexpected tag %d ###\n",
                                                  cur_packet->get_Tag()));
                            break;
                        }//switch
                    }//for

                    // We don't want any of the sockets created to accept
                    // to stick around if we're disabled
                    if( edt->is_Disabled() ) {
                        XPlat::SocketUtils::Close(connected_sock);
                    }
                }//while
            }//if activity on local sock

            if( net->is_LocalNodeChild() && 
                ( eventfds.find(parent_sock) != eventfds.end() ) ) {
                // event happened on parent connection:
                // either failure or shutdown message
                
                packets.clear();

                int recv_ret = msg.recv( parent_sock, packets, UnknownRank );
                mrn_dbg(5, mrn_printf(FLF, stderr, "recv from parent_sock returned %d\n", recv_ret));
                if( -1 != recv_ret ) {
                    list< PacketPtr >::iterator packet_list_iter;
                    list< XPlat_Socket >::iterator iter;

                    for( packet_list_iter = packets.begin();
                            packet_list_iter != packets.end();
                            packet_list_iter++ ) {

                        PacketPtr cur_packet( *packet_list_iter );
                        switch ( cur_packet->get_Tag() ) {

                            case PROT_EDT_REMOTE_SHUTDOWN:
                                mrn_dbg( 5, mrn_printf(FLF, stderr,
                                            "PROT_EDT_REMOTE_SHUTDOWN\n"));
                                edt->disable();
                                break;
                            default:
                                mrn_dbg(1, mrn_printf(FLF, stderr, 
                                            "### PROTOCOL ERROR: Unexpected tag %d ###\n",
                                            cur_packet->get_Tag()));
                                break;
                        }
                    }
                } else {

                    mrn_dbg( 3, mrn_printf(FLF, stderr, "Parent failure detected ...\n"));

                    // remove old parent socket from watched lists
                    edt->remove_FD( parent_sock );
                    watch_list.remove( parent_sock );

                    if( edt->is_Disabled() ) {
                        mrn_dbg(5, mrn_printf(FLF, stderr,
                                    "Closing parent event socket %d\n",
                                    parent_sock));

                        if( -1 == XPlat::SocketUtils::Shutdown(parent_sock, XPLAT_SHUT_WR) ) {
                            mrn_dbg(1, mrn_printf(FLF, stderr,
                                        "Close of parent sock %d failed!\n",
                                        parent_sock));
                        }

                        if( ! XPlat::SocketUtils::Close(parent_sock) ) {
                            mrn_dbg(1, mrn_printf(FLF, stderr,
                                        "Close of parent sock %d failed!\n",
                                        parent_sock));
                        }
                    } else {
                        parent_sock = -1;
                        edt->recover_FromParentFailure( parent_sock );
                        if( -1 != parent_sock ) {
                            edt->add_FD(parent_sock);
                            watch_list.push_back( parent_sock );
                            mrn_dbg(5, mrn_printf(FLF, stderr,
                                        "Parent socket:%d added to list.\n",
                                        parent_sock));
                        } else {
                            // couldn't recover or recovery turned off,
                            // let's disable myself
                            edt->disable();
                        }
                    }
                }
            }
                    
            // Check for child failures
            list< XPlat_Socket >::iterator iter;
            for( iter = watch_list.begin(); iter != watch_list.end(); ) {
                XPlat_Socket cur_sock = *iter;

                // skip local_sock and parent_sock, or if socket isn't set
                if( (cur_sock == local_sock) ||
                    (net->is_LocalNodeChild() && (cur_sock == parent_sock)) ||
                    (eventfds.find(cur_sock) == eventfds.end()) ) {
                    iter++;
                    continue;
                }

                /*mrn_dbg( 5, mrn_printf(FLF, stderr, "socket:%d IS set\n", cur_sock) );*/

                // remove from watched lists
                edt->remove_FD( cur_sock );
                list< XPlat_Socket >::iterator tmp_iter = iter++;
                watch_list.erase( tmp_iter );

                if( ! edt->is_Disabled() ) {
                    map< XPlat_Socket, Rank >:: iterator iter2 =
                        edt->childRankByEventDetectionSocket.find( cur_sock );

                    if( iter2 != edt->childRankByEventDetectionSocket.end() ) {
                        // this child has failed
                        int failed_rank = (*iter2).second;

                        mrn_dbg( 3, mrn_printf(FLF, stderr,
                                    "Child[%u] failure detected on event fd %d\n",
                                    failed_rank, cur_sock) );

                        edt->recover_FromChildFailure( failed_rank );
                        edt->childRankByEventDetectionSocket.erase( iter2 );
                    }

                }
            }
        }//else
    }//while

    edt->set_ThrId( 0 );
    mrn_dbg(3, mrn_printf(FLF, stderr, "EDT reached end of main!\n"));
    mrn_dbg(5, mrn_printf(FLF, stderr, "I'm going away now!\n"));
    Network::free_ThreadState();

  } catch( ... ) { throw; }

    return NULL;
}

int EventDetector::init_NewChildFDConnection( PeerNodePtr iparent_node )
{
    string lhostname = _network->get_LocalHostName();
    Port lport = _network->get_LocalPort();
    Rank lrank = _network->get_LocalRank();

    mrn_dbg( 5, mrn_printf(FLF, stderr, "Initializing new Child FD Connection ...\n"));
    int num_retry = 5;
    if( iparent_node->connect_EventSocket(num_retry) == -1 ){
        mrn_dbg( 1, mrn_printf(FLF, stderr, "connect_EventSocket( %s:%u ) failed\n",
                               iparent_node->get_HostName().c_str(),
                               iparent_node->get_Port() ) );
    }

    PacketPtr packet( new Packet( CTL_STRM_ID, PROT_NEW_CHILD_FD_CONNECTION, "%s %uhd %ud",
                                  lhostname.c_str(), lport, lrank ) );
    Message msg(_network);
    msg.add_Packet( packet );
    if( msg.send( iparent_node->get_EventSocketFd() ) == -1 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Message.send failed\n" ));
        return -1;
    }

    return 0;
}

int EventDetector::proc_NewChildFDConnection( PacketPtr ipacket, XPlat_Socket isock )
{
    char* child_hostname = NULL;
    Port child_port;
    Rank child_rank;

    ipacket->unpack( "%s %uhd %ud", &child_hostname, &child_port,
                    &child_rank ); 

    mrn_dbg(5, mrn_printf(FLF, stderr,
                          "New FD connection on sock %d from: %s:%u:%u\n",
                          isock, child_hostname, child_port, child_rank ) ); 
    if( child_hostname != NULL )
        free( child_hostname );

    childRankByEventDetectionSocket[ isock ] = child_rank;

    PeerNodePtr child_peer = _network->get_PeerNode( child_rank );
    if( child_peer != PeerNode::NullPeerNode ) {
        child_peer->set_EventSocketFd( isock );
    }

    return 0;
}

int EventDetector::recover_FromChildFailure( Rank ifailed_rank )
{
    mrn_dbg_func_begin();

    if( _network->is_ShuttingDown() ) {
        mrn_dbg(3, mrn_printf(FLF, stderr, "NOT recovering from child's failure\n") );
        return 0;
    }

    Rank my_rank = _network->get_LocalRank();

    // generate topology update for failed child
    if( _network->is_LocalNodeInternal() ) {
        mrn_dbg(5, mrn_printf(FLF, stderr,
                    "Generating topology update for failed child\n") );
        Stream *s = _network->get_Stream( TOPOL_STRM_ID ); // topol prop stream
        if( s != NULL ) {
            int type = NetworkTopology::TOPO_REMOVE_RANK; 
            Port dummy_port = UnknownPort;
            char* dummy_host = strdup("NULL"); // ugh, this needs to be fixed
            s->send_internal( PROT_TOPO_UPDATE, "%ad %aud %aud %as %auhd", 
                              &type, 1, 
                              &my_rank,1, 
                              &ifailed_rank, 1, 
                              &dummy_host, 1, 
                              &dummy_port, 1 );
            free( dummy_host );
        }
    }
    else { // FE
        NetworkTopology* nt = _network->get_NetworkTopology();
        nt->update_removeNode( my_rank, ifailed_rank, true );
    }

    ParentNode *parent = _network->get_LocalParentNode();
    parent->notify_OfChildFailure();
    parent->abort_ActiveControlProtocols();

    mrn_dbg_func_end();
    return 0;
}

int EventDetector::recover_FromParentFailure( XPlat_Socket& new_parent_sock )
{
    Timer new_parent_timer, cleanup_timer, connection_timer, 
          filter_state_timer, overall_timer;

    mrn_dbg_func_begin();
   
    if( _network->is_ShuttingDown() || ! _network->recover_FromFailures() ) {
        mrn_dbg(3, mrn_printf(FLF, stderr, "NOT recovering from parent's failure\n") );
        return 0;
    }

    PeerNodePtr old_parent = _network->get_ParentNode();
    Rank my_rank = _network->get_LocalRank();
    Rank par_rank = old_parent->get_Rank();

    old_parent->mark_Failed();

    mrn_dbg(3, mrn_printf(FLF, stderr, "Recovering from parent[%d]'s failure\n",
                          par_rank));
	
    //_network->get_NetworkTopology()->print(stderr);
 
    //Step 1: Compute new parent
    overall_timer.start();
    new_parent_timer.start();
    NetworkTopology::Node * new_parent_node = 
        _network->get_NetworkTopology()->find_NewParent( my_rank );
    if( new_parent_node == NULL ) {
        mrn_dbg(1, mrn_printf( FLF, stderr, "Can't find new parent! NOT recovering ...\n" ));
        return -1;
    }

    mrn_dbg(3, mrn_printf( FLF, stderr, "RECOVERY: NEW PARENT: %s:%d:%d\n",
                           new_parent_node->get_HostName().c_str(),
                           new_parent_node->get_Port(),
                           new_parent_node->get_Rank() ));
    
    string new_parent_name = new_parent_node->get_HostName();
    Rank new_parent_rank = new_parent_node->get_Rank();
    PeerNodePtr new_parent = _network->new_PeerNode( new_parent_name,
                                                     new_parent_node->get_Port(),
                                                     new_parent_rank,
                                                     true, true );
    new_parent_timer.stop();

    //Step 2. Establish data connection w/ new Parent
    connection_timer.start();
    mrn_dbg(3, mrn_printf( FLF, stderr, "Establish new data connection ...\n"));
    if( _network->get_LocalChildNode()->init_newChildDataConnection( new_parent,
                                                                     par_rank ) == -1 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr,
                              "PeerNode::init_newChildDataConnection() failed\n"));
        return -1;
    }

    //Step 3. Establish event detection connection w/ new Parent
    mrn_dbg(3, mrn_printf( FLF, stderr, "Establish new event connection ...\n"));
    if( init_NewChildFDConnection( new_parent ) == -1 ){
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                               "init_NewChildFDConnection() failed\n") );
        return -1;
    }
    new_parent_sock = new_parent->get_EventSocketFd();
    mrn_dbg(3, mrn_printf( FLF, stderr, "New event connection established...\n"));
    connection_timer.stop();

    //Step 4. Propagate filter state for active streams to new parent
    filter_state_timer.start();
    mrn_dbg(3, mrn_printf( FLF, stderr, "Sending filter states ...\n"));
    if( _network->send_FilterStatesToParent() == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                               "send_FilterStatesToParent() failed\n") );
        return -1;
    }
    mrn_dbg(3, mrn_printf( FLF, stderr, "Sending filter states complete!\n"));
    filter_state_timer.stop();

    //Step 5. Update local topology and data structures
    cleanup_timer.start();
    mrn_dbg(3, mrn_printf( FLF, stderr, "Updating local structures ...\n"));
    _network->remove_Node( par_rank, false );   
    _network->change_Parent( my_rank, new_parent_rank );
    cleanup_timer.stop();

    overall_timer.stop();

    //Internal nodes must report parent failure to children
    mrn_dbg(3, mrn_printf( FLF, stderr, "Report failure to children ...\n"));
    if( _network->is_LocalNodeInternal() ) {
        //format is my_rank, failed_parent_rank, new_parent_rank
        PacketPtr packet( new Packet( CTL_STRM_ID, PROT_RECOVERY_RPT, "%ud %ud %ud",
                                      my_rank,
                                      par_rank,
                                      new_parent_rank ) );

        if( _network->send_PacketToChildren( packet ) == -1 ){
            mrn_dbg(1, mrn_printf(FLF, stderr, "send_PacketDownStream() failed\n"));
        }
    }
    mrn_dbg(3, mrn_printf( FLF, stderr, "Report failure to children complete!\n"));

    mrn_dbg(3, mrn_printf( FLF, stderr, "ostart: %lf, oend: %lf\n",
                           overall_timer._start_d, overall_timer._stop_d ));
    mrn_dbg(3, mrn_printf( FLF, stderr, "pstart: %lf, cend: %lf\n",
                           new_parent_timer._start_d, cleanup_timer._stop_d ));


    mrn_dbg(3, mrn_printf( FLF, stderr, "%u %u %lf %lf %lf %lf %lf %lf\n",
                           my_rank,
                           par_rank,
                           overall_timer._stop_d,
                           new_parent_timer.get_latency_msecs(),
                           connection_timer.get_latency_msecs(),
                           filter_state_timer.get_latency_msecs(),
                           cleanup_timer.get_latency_msecs(),
                           overall_timer.get_latency_msecs() ) );

#ifdef HAVE_FAILURE_MGR
    //Notify Failure Manager that recovery is complete
    PacketPtr packet( new Packet( CTL_STRM_ID, PROT_RECOVERY_RPT,
                                  "%ud %ud %lf %lf %lf %lf %lf %lf",
                                  my_rank,
                                  par_rank,
                                  overall_timer._stop_d,
                                  new_parent_timer.get_latency_msecs(),
                                  connection_timer.get_latency_msecs(),
                                  filter_state_timer.get_latency_msecs(),
                                  cleanup_timer.get_latency_msecs(),
                                  overall_timer.get_latency_msecs() ) );

    mrn_dbg(3, mrn_printf( FLF, stderr, "Notifying FIS...\n"));
    int sock_fd=0;
    mrn_dbg(3, mrn_printf( FLF, stderr, "Sending recovery report to FIS:%s:%d ...\n",
                           _network->get_FailureManager()->get_HostName().c_str(),
                           _network->get_FailureManager()->get_Port() )); 
    if( connectHost(&sock_fd, _network->get_FailureManager()->get_HostName(),
                    _network->get_FailureManager()->get_Port()) == -1 ){
        mrn_dbg(1, mrn_printf(FLF, stderr, "connectHost() failed\n"));
        return -1;
    }
    
    Message msg;
    msg.setNetwork(_network);
    msg.add_Packet( packet );
    if( msg.send( sock_fd ) == -1 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Message.send failed\n" ));
        return -1;
    }
    XPlat::SocketUtils::Close( sock_fd );
    mrn_dbg(3, mrn_printf( FLF, stderr, "Recovery report to FIS complete!\n"));
#endif

    mrn_dbg_func_end();
  
    return 0;
}

XPlat::Thread::Id EventDetector::get_ThrId(void) const
{
    XPlat::Thread::Id rc;
    _sync.Lock();
    rc = _thread_id;
    _sync.Unlock();
    return rc;
}

void EventDetector::set_ThrId( XPlat::Thread::Id tid )
{
    _sync.Lock();
    _thread_id = tid;
    _sync.Unlock();
}

// If this is called, shutdown is starting
void EventDetector::disable(void)
{
    _sync.Lock();
    _disabled = true;
    if( NULL != _network ) {
        _network->signal_ShutDown();
    }
    _sync.Unlock();
}

bool EventDetector::is_Disabled(void)
{
    bool ret;
    _sync.Lock();
    ret = _disabled;
    _sync.Unlock();

    return ret;
}

bool EventDetector::start_ShutDown()
{
    mrn_dbg_func_begin();

    if( _network->is_LocalNodeParent() ) {
        Message msg(_network);
        XPlat_Socket sock_fd = XPlat::SocketUtils::InvalidSocket;
        PacketPtr packet( new Packet(CTL_STRM_ID, PROT_EDT_SHUTDOWN, NULL) );

        string edt_host = _network->get_LocalHostName();
        Port edt_port = _network->get_LocalPort();

        if( connectHost(&sock_fd, edt_host, edt_port) == -1) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "connectHost(%s:%d) failed\n",
                                  edt_host.c_str(), edt_port ));
            return false;
        }

        msg.add_Packet( packet );
        if( msg.send( sock_fd ) == -1 ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "Message.sendDirectly() failed\n" ));
            return false;
        }
        mrn_dbg(5, mrn_printf(FLF, stderr, "After sending PROT_EDT_SHUTDOWN...\n" ));

        XPlat::SocketUtils::Close(sock_fd);
    }

    mrn_dbg_func_end();
    return true;
}

} /* namespace MRN */
