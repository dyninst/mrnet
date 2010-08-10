/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <sstream>

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

using namespace std;

namespace MRN {

bool EventDetector::stop( )
{
    int edt_port, sock_fd=0;
    string edt_host;
    Message msg;
    PacketPtr packet( new Packet( 0, PROT_KILL_SELF, "" ) );

    mrn_dbg_func_begin();

    if( 0 == _thread_id )
        return true;

    if( _network->is_LocalNodeParent() ) {
        // send KILL_SELF message to EDT on listening port
        edt_host = _network->get_LocalHostName();
        edt_port = _network->get_LocalPort();
        mrn_dbg(3, mrn_printf( FLF, stderr, "Telling EDT(%s:%d) to go away\n",
                               edt_host.c_str(), edt_port )); 
        if( connectHost( &sock_fd, edt_host.c_str(), edt_port ) == -1 ) {
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
        if( XPlat::Thread::Cancel( _thread_id ) != 0 )
            return false;
    }

    // wait for EDT to exit
    if( XPlat::Thread::Join( _thread_id, (void**)NULL ) != 0 )
        return false;

    mrn_dbg_func_end();
    return true;
}

bool EventDetector::start( Network* inetwork )
{
    long thread_id = 0;
    mrn_dbg(3, mrn_printf(FLF, stderr, "Creating Event Detection thread ...\n"));

    if( XPlat::Thread::Create( main, (void*)inetwork->_edt, &thread_id ) == -1 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Thread creation failed...\n"));
        return false;
    }

    // record the thread id
    inetwork->_edt->_thread_id = thread_id;

    mrn_dbg_func_end();
    return true;
}

bool EventDetector::add_FD( int ifd )
{
    int nchild;

    mrn_dbg_func_begin();

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
    mrn_dbg( 5, mrn_printf(FLF, stderr, "added fd %d\n", ifd) );
 
    _num_pollfds++;  

    return true;
}

bool EventDetector::remove_FD( int ifd )
{
    int new_max = -1;
    unsigned int i, j;

    mrn_dbg_func_begin();

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


int EventDetector::eventWait( std::set< int >& event_fds, int timeout_ms, 
                              bool use_poll=true )
{
    int retval, err;
    fd_set readfds;

    mrn_dbg( 5, mrn_printf(FLF, stderr,
                           "waiting on %u fds\n", _num_pollfds) );
  
#ifdef os_windows
    use_poll=false;
#else
    if( use_poll ) { 

        retval = poll( _pollfds, _num_pollfds, timeout_ms );
        err = errno;
        mrn_dbg( 5, mrn_printf(FLF, stderr,
                               "poll() returned %d\n", retval) );
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
        mrn_dbg( 5, mrn_printf(FLF, stderr,
                               "select() returned %d\n", retval) );
#ifndef os_windows
    }
#endif
    if( retval == -1 && err != EINTR )
        perror("select() or poll()");

    if( retval <= 0 )
        return retval;
    
    for( unsigned int num=0; num < _num_pollfds; num++ ) {
        bool add = false;
        if( use_poll ) {
            if(( _pollfds[num].revents & POLLIN ) )
                add = true;   
        }
        else { // select
            if( FD_ISSET( _pollfds[num].fd, &readfds ) )
                add = true;
        }
        if( add ) {
            event_fds.insert( _pollfds[num].fd );
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
    list< int > watch_list; //list of sockets to detect events on
    int parent_sock=0;
    int local_sock=0;
    bool goto_outer;

    EventDetector* edt = (EventDetector*) iarg;
    Network* net = edt->_network;;
 
    PeerNodePtr  parent_node = PeerNode::NullPeerNode;
    if( net->is_LocalNodeChild() ) {
        parent_node = net->get_ParentNode();
    }
    
    //set up debugging stuff
    Rank myrank = net->get_LocalRank();
    string prettyHost;
    XPlat::NetUtils::GetHostName( net->get_LocalHostName(), prettyHost );
    std::ostringstream namestr;
    namestr << "EDT("
            << prettyHost
            << ':'
            << myrank
            << ')'
            << std::ends;

    tsd_t * local_data = new tsd_t;
    local_data->thread_id = XPlat::Thread::GetId();
    local_data->thread_name = strdup( namestr.str().c_str() );
    local_data->process_rank = myrank;
    local_data->node_type = UNKNOWN_NODE;

    int status;
    if( (status = tsd_key.Set( local_data )) != 0){
        fprintf(stderr, "XPlat::TLSKey::Set(): %s\n", strerror(status)); 
        return NULL;
    }

    srand48( net->get_LocalRank() );
    
    //(1) Establish connection with parent Event Detection Thread
    if( net->is_LocalNodeChild() ) {
        if( edt->init_NewChildFDConnection( parent_node ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "init_NewChildFDConnection() failed\n") );
        }
        mrn_dbg( 5, mrn_printf(0,0,0, stderr, "success!\n"));

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
        if( local_sock != -1 ){
            edt->add_FD(local_sock);
            mrn_dbg( 5, mrn_printf(FLF, stderr,
                                   "Monitoring local socket:%d.\n", local_sock));
        }
    }
    else mrn_dbg(5, mrn_printf(FLF, stderr, "not a parent\n"));

    //3) do EventDetection Loop, current events are:
    //   - PROT_KILL_SELF 
    //   - PROT_NEW_CHILD_FD_CONNECTION (a new child peer to monitor)
    //   - PROT_NEW_CHILD_DATA_CONNECTION (a new child peer for data)
    //   - socket failures
    Message msg;
    list< PacketPtr > packets;
    mrn_dbg( 5, mrn_printf(FLF, stderr, "EDT Main Loop ...\n"));
    while ( true ) {

        Timer waitTimer;
        int timeout = -1; /* block */
        TimeKeeper* tk = NULL;

        if( edt->_network == NULL ) /* Network going away */
            break;

        tk = net->get_TimeKeeper();
	if( tk != NULL ) {
	    timeout = tk->get_Timeout();
	    waitTimer.start();
	}

        std::set< int > eventfds;
        mrn_dbg( 5, mrn_printf(FLF, stderr, "eventWait(timeout=%dms)\n", timeout));
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
	    if( elapsed == 0 ) elapsed++;
            mrn_dbg( 5, mrn_printf(FLF, stderr, "timeout after %d ms\n", elapsed));
                
	    // notify streams with registered timeouts
	    edt->handle_Timeout( tk, elapsed );
	    continue;
        }
        else{
            mrn_dbg( 5, mrn_printf(FLF, stderr,
                                   "checking event fds"));
            if( eventfds.find(local_sock) != eventfds.end() ) {
                //Activity on our local listening sock, accept connection
                mrn_dbg( 5, mrn_printf(FLF, stderr, "Activity on listening socket\n"));

                while(true)
		{

                int inout_errno;

                int connected_sock = getSocketConnection( local_sock, inout_errno );

				if( (connected_sock == -1 )  && ( (inout_errno == EAGAIN ) ||  (inout_errno == EWOULDBLOCK) ) ) 
					break;

                if ( connected_sock == -1) {
                    perror("getSocketConnection()");
                    mrn_dbg( 1, mrn_printf(FLF, stderr, "getSocketConnection() failed\n"));
                    perror("getSocketConnection()");
		    goto_outer=true;
		    break;
		}    
		 
                packets.clear();
                msg.recv( connected_sock, packets, UnknownRank );
                list< PacketPtr >::iterator packet_list_iter;
                list< int >::iterator iter;

                for( packet_list_iter = packets.begin();
                     packet_list_iter != packets.end();
                     packet_list_iter++ ) {
                    ParentNode* p;
                    PacketPtr cur_packet( *packet_list_iter );
                    switch ( cur_packet->get_Tag() ) {

                    case PROT_KILL_SELF:
                        mrn_dbg( 5, mrn_printf(FLF, stderr, "PROT_KILL_SELF\n"));

                        //close event sockets explicitly
                        mrn_dbg(5, mrn_printf(FLF, stderr,
                                              "Closing %d sockets\n",
                                              watch_list.size() ));
                        for( iter=watch_list.begin(); iter!=watch_list.end(); iter++ ) {
                            mrn_dbg(5, mrn_printf(FLF, stderr,
                                                  "Closing event socket: %d\n", *iter ));
                            char c = 1;
                            mrn_dbg(5, mrn_printf(FLF, stderr, "... writing \n"));
                            // TODO: Commented out for Windows because of write assert() failure.
#ifndef os_windows
                            if( write( *iter, &c, 1) == -1 ) {
                                perror("write(event_fd)");
                            }
                            mrn_dbg(5, mrn_printf(FLF, stderr, "... closing\n"));
#endif

                            if( XPlat::SocketUtils::Close( *iter ) == -1 ){
                                perror("close(event_fd)");
                            }
                        }
                        mrn_dbg(5, mrn_printf(FLF, stderr, "I'm going away now!\n"));
                        return NULL;

                    case PROT_NEW_CHILD_FD_CONNECTION:
                        mrn_dbg(5, mrn_printf(FLF, stderr, "PROT_NEW_CHILD_FD_CONNECTION\n"));
                        edt->add_FD(connected_sock);
                        watch_list.push_back( connected_sock );
                        mrn_dbg(5, mrn_printf(FLF, stderr,
                                               "FD socket:%d added to list.\n",
                                               connected_sock));
                        
                        edt->proc_NewChildFDConnection( cur_packet, connected_sock );
                        break;

                    case PROT_NEW_CHILD_DATA_CONNECTION:
                        mrn_dbg(5, mrn_printf(FLF, stderr, "PROT_NEW_CHILD_DATA_CONNECTION\n"));
                        //get ParentNode obj. Try internal node, then FE
                        p = net->get_LocalParentNode();
                        assert(p);

                        p->proc_NewChildDataConnection( cur_packet,
                                                        connected_sock );
                        mrn_dbg( 5, mrn_printf(FLF, stderr, "New child connected.\n"));
                        break;

                    case PROT_NEW_SUBTREE_RPT:
                        // NOTE: needed since back-ends are now threaded, and we can't
                        //       guarantee a packet containing this protocol message
                        //       won't arrive in a group with NEW_CHILD_DATA_CONNECTION
                        mrn_dbg(5, mrn_printf(FLF, stderr, "PROT_NEW_SUBTREE_RPT\n"));
                        //get ParentNode obj. Try internal node, then FE
                        p = net->get_LocalParentNode();
                        assert(p);

                        if( p->proc_newSubTreeReport( cur_packet ) == -1 ) {
                            mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_newSubTreeReport() failed\n" ));
                        }
                        break;
                    
                     case PROT_SUBTREE_INITDONE_RPT:
                        // NOTE: needed since back-ends are now threaded, and we can't
                        //       guarantee a packet containing this protocol message
                        //       won't arrive in a group with NEW_CHILD_DATA_CONNECTION
                        mrn_dbg( 5, mrn_printf(FLF, stderr, "PROT_NEW_SUBTREE_RPT\n"));
                        //get ParentNode obj. Try internal node, then FE
                        p = net->get_LocalParentNode();
                        assert(p);

                        if( p->proc_SubTreeInitDoneReport( cur_packet ) == -1 ) {
                            mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_SubTreeInitDoneReport() failed\n" ));
                        }
                        break;

                    default:
                        mrn_dbg(1, mrn_printf(FLF, stderr, 
                                              "### PROTOCOL ERROR: Unexpected tag %d ###\n",
                                              cur_packet->get_Tag()));
                        break;
                    }//switch
                }//for
	      }//while	
            }//if activity on local sock

            if(!goto_outer)
	    {

            if( net->is_LocalNodeChild() && 
                ( eventfds.find(parent_sock) != eventfds.end() ) ) {
                mrn_dbg( 3, mrn_printf(FLF, stderr, "Parent failure detected ...\n"));
                //event happened on parent monitored connections, likely failure

                //remove old parent socket from "select" sockets list
                edt->remove_FD(parent_sock);
                watch_list.remove( parent_sock );

		//fprintf(stdout,"Debug FR in main() : Recovering from parent failure\n");
                if( net->recover_FromFailures() ) {

                    mrn_dbg(3, mrn_printf(FLF, stderr, "... recovering from parent failure\n"));
                    edt->recover_FromParentFailure();

                    //add new parent sock to monitor for failure
                    parent_node = net->get_ParentNode();
                    parent_sock = parent_node->get_EventSocketFd();
                    edt->add_FD(parent_sock);
                    watch_list.push_back( parent_sock );
                    mrn_dbg(5, mrn_printf(FLF, stderr,
                                          "Parent socket:%d added to list.\n", parent_sock));
			                       
                }
                else {
			
                    edt->recover_off_FromParentFailure();

		    mrn_dbg(3, mrn_printf(FLF, stderr, "NOT recovering from parent failure ...\n"));
                    if( watch_list.size() == 0 ) {
                        mrn_dbg(5, mrn_printf(FLF, stderr, "No more sockets to watch, bye!\n"));
                        return NULL;
                    }	
                }
                
            }

	
                    
            //Check for child failures. Whom to notify?
            list< int >::iterator iter;
            for( iter=watch_list.begin(); iter != watch_list.end(); ) {
                int cur_sock = *iter;
		
                //skip local_sock and parent_sock or if socket isn't set
                if( ( cur_sock == local_sock ) ||
                    ( (net->is_LocalNodeChild() ) && (cur_sock == parent_sock) ) ||
                    ( eventfds.find(cur_sock)==eventfds.end() ) ) {

                    mrn_dbg( 5, mrn_printf(0,0,0, stderr, "Is socket:%d set? NO!\n", cur_sock));
                    iter++;

                    continue;
                }
                    
                mrn_dbg( 5, mrn_printf(0,0,0, stderr, "Is socket:%d set? YES!\n", cur_sock));

                map< int, Rank >:: iterator iter2 =
                    edt->childRankByEventDetectionSocket.find( cur_sock );

                if( iter2 != edt->childRankByEventDetectionSocket.end() ) {
                    //this child has failed
                    int failed_rank = (*iter2).second;

                    
		mrn_dbg( 3, mrn_printf(FLF, stderr,
                                           "Child[%u] failure detected ...\n",
                                           failed_rank ));

                    if( net->recover_FromFailures() ) {
                        edt->recover_FromChildFailure( failed_rank );
		    }
		    else	
		    {
			//fprintf(stdout,"Debug FR: Calling Remove Node (recover from Child Failure)\n\n");
			net->remove_Node(failed_rank);
	             }
		    fflush(stdout);
                    edt->childRankByEventDetectionSocket.erase( iter2 );

                    //remove from "select" sockets list
                    edt->remove_FD(cur_sock);

                    //remove socket from socket watch list
                    list< int >::iterator tmp_iter;
                    tmp_iter = iter;
                    iter++;
                    watch_list.erase( tmp_iter );
                }
            }
	  }//flag goto_outer  
        }//else
    }//while

    return NULL;
}

int EventDetector::init_NewChildFDConnection( PeerNodePtr iparent_node )
{
    string lhostname = _network->get_LocalHostName();
    Port lport = _network->get_LocalPort();
    Rank lrank = _network->get_LocalRank();

    mrn_dbg( 5, mrn_printf(FLF, stderr, "Initializing new Child FD Connection ...\n"));
    if( iparent_node->connect_EventSocket() == -1 ){
        mrn_dbg( 1, mrn_printf(FLF, stderr, "connect_EventSocket( %s:%u ) failed\n",
                               iparent_node->get_HostName().c_str(),
                               iparent_node->get_Port() ) );
    }

    PacketPtr packet( new Packet( 0, PROT_NEW_CHILD_FD_CONNECTION, "%s %uhd %ud",
                                  lhostname.c_str(), lport, lrank ) );
    Message msg;
    msg.add_Packet( packet );
    if( msg.send( iparent_node->get_EventSocketFd() ) == -1 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Message.send failed\n" ));
        return -1;
    }

    return 0;
}

int EventDetector::proc_NewChildFDConnection( PacketPtr ipacket, int isock )
{
    char * child_hostname_ptr;
    Port child_port;
    Rank child_rank;

    ipacket->unpack( "%s %uhd %ud", &child_hostname_ptr, &child_port,
                    &child_rank ); 


    string child_hostname( child_hostname_ptr );

    mrn_dbg(5, mrn_printf(FLF, stderr,
                          "New FD connection on sock %d from: %s:%u:%u\n",
                          isock, child_hostname_ptr, child_port, child_rank ) ); 

    childRankByEventDetectionSocket[ isock ] = child_rank;

    return 0;
}

int EventDetector::recover_FromChildFailure( Rank ifailed_rank )
{
    PacketPtr packet( new Packet( 0, PROT_FAILURE_RPT, "%ud", ifailed_rank ) );

    assert( _network->is_LocalNodeParent() );

    mrn_dbg(1, mrn_printf(FLF, stderr, "calling proc_FailureReport(node:%u)\n", ifailed_rank));
    if( _network->get_LocalParentNode()->proc_FailureReport( packet ) == -1 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "proc_FailureReport() failed\n"));
        return -1;
    }

    return 0;
}

int EventDetector::recover_FromParentFailure( )
{
    Timer new_parent_timer, cleanup_timer, connection_timer, 
          filter_state_timer, overall_timer;

   //fprintf(stdout,"Debug FR Recovering from parent failure inside the recover_Parent()\n");
    mrn_dbg_func_begin();

   
    _network->get_ParentNode()->mark_Failed();
    Rank failed_rank = _network->get_ParentNode()->get_Rank();
    _network->set_ParentNode( PeerNode::NullPeerNode );
    mrn_dbg(3, mrn_printf( FLF, stderr, "Recovering from parent[%d]'s failure\n",
                           failed_rank ));
	
    _network->get_NetworkTopology()->print(NULL);
  
    
 
    //Step 1: Compute new parent
    overall_timer.start();
    new_parent_timer.start();
    NetworkTopology::Node * new_parent_node = _network->get_NetworkTopology()->
        find_NewParent( _network->get_LocalRank() );
    if( !new_parent_node ) {
        mrn_dbg(1, mrn_printf( FLF, stderr, "Can't find new parent! Exiting ...\n" ));
        exit(-1);
    }

    mrn_dbg(1, mrn_printf( FLF, stderr, "RECOVERY: NEW PARENT: %s:%d:%d\n",
                           new_parent_node->get_HostName().c_str(),
                           new_parent_node->get_Port(),
                           new_parent_node->get_Rank() ));
    
    string new_parent_name = new_parent_node->get_HostName();
    PeerNodePtr new_parent = _network->new_PeerNode( new_parent_name,
                                                     new_parent_node->get_Port(),
                                                     new_parent_node->get_Rank(),
                                                     true, true );
    new_parent_timer.stop();

    //Step 2. Establish data connection w/ new Parent
    connection_timer.start();
    mrn_dbg(3, mrn_printf( FLF, stderr, "Establish new data connection ...\n"));
    if( _network->get_LocalChildNode()->init_newChildDataConnection( new_parent,
                                                                     failed_rank ) == -1 ) {
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
    
    

     //remove node, but don't update datastructs since following procedure will
    _network->remove_Node( failed_rank, false );
   
    _network->change_Parent( _network->get_LocalRank(),
                             new_parent_node->get_Rank() );
    cleanup_timer.stop();

    overall_timer.stop();

    //Internal nodes must report parent failure to children
    mrn_dbg(3, mrn_printf( FLF, stderr, "Report failure to children ...\n"));
    if( _network->is_LocalNodeInternal() ) {
        //format is my_rank, failed_parent_rank, new_parent_rank
        PacketPtr packet( new Packet( 0, PROT_RECOVERY_RPT, "%ud %ud %ud",
                                      _network->get_LocalRank(),
                                      failed_rank,
                                      _network->get_ParentNode()->get_Rank() ) );

        if( _network->send_PacketToChildren( packet ) == -1 ){
            mrn_dbg(1, mrn_printf(FLF, stderr, "send_PacketDownStream() failed\n"));
        }
    }
    mrn_dbg(3, mrn_printf( FLF, stderr, "Report failure to children complete!\n"));

   //Notify Failure Manager that recovery is complete
    PacketPtr packet( new Packet( 0, PROT_RECOVERY_RPT,
                                  "%ud %ud %lf %lf %lf %lf %lf %lf",
                                  _network->get_LocalRank(),
                                  failed_rank,
                                  overall_timer._stop_d,
                                  new_parent_timer.get_latency_msecs(),
                                  connection_timer.get_latency_msecs(),
                                  filter_state_timer.get_latency_msecs(),
                                  cleanup_timer.get_latency_msecs(),
                                  overall_timer.get_latency_msecs() ) );

    mrn_dbg(3, mrn_printf( FLF, stderr, "ostart: %lf, oend: %lf\n",
                           overall_timer._start_d, overall_timer._stop_d ));
    mrn_dbg(3, mrn_printf( FLF, stderr, "pstart: %lf, cend: %lf\n",
                           new_parent_timer._start_d, cleanup_timer._stop_d ));


    mrn_dbg(3, mrn_printf( FLF, stderr, "%u %u %lf %lf %lf %lf %lf %lf\n",
                           _network->get_LocalRank(),
                           failed_rank,
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
    if(connectHost( &sock_fd, _network->get_FailureManager()->get_HostName().c_str(),
                    _network->get_FailureManager()->get_Port() ) == -1){
        mrn_dbg(1, mrn_printf(FLF, stderr, "connectHost() failed\n"));
        return -1;
    }
    
    Message msg;
    msg.add_Packet( packet );
    if( msg.send( sock_fd ) == -1 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Message.send failed\n" ));
        return -1;
    }
    XPlat::SocketUtils::Close( sock_fd );
    mrn_dbg(3, mrn_printf( FLF, stderr, "Recovery report to FIS complete!\n"));

    mrn_dbg_func_end();
  
    return 0;
}


int EventDetector::recover_off_FromParentFailure(void)
{
    _network->get_ParentNode()->mark_Failed();
    Rank fail_rank = _network->get_ParentNode()->get_Rank();
    _network->remove_Node( fail_rank);  
    return 0;
}

};
