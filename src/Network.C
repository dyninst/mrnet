/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <sys/types.h>
#ifndef os_windows
#include <sys/socket.h>
#endif

#ifndef os_windows
#include "mrnet_config.h"
#endif
#include "utils.h"
#include "BackEndNode.h"
#include "ChildNode.h"
#include "EventDetector.h"
#include "Filter.h"
#include "FrontEndNode.h"
#include "InternalNode.h"
#include "ParentNode.h"
#include "ParsedGraph.h"
#include "PeerNode.h"
#include "TimeKeeper.h"
#include "mrnet/Network.h"
#include "mrnet/MRNet.h"
#include "xplat/NetUtils.h"
#include "xplat/SocketUtils.h"
#include "xplat/Thread.h"
#include "xplat/Process.h"
#include "xplat/Error.h"
#include "PerfDataEvent.h"
#include "PerfDataSysEvent.h"

using namespace XPlat;

extern FILE *mrnin;

using namespace std;

namespace MRN
{

int mrnparse( );

extern const char* mrnBufPtr;
extern unsigned int mrnBufRemaining;

const int MIN_OUTPUT_LEVEL = 0;
const int MAX_OUTPUT_LEVEL = 5;
int CUR_OUTPUT_LEVEL = 0;
char* MRN_DEBUG_LOG_DIRECTORY = NULL;

const Port UnknownPort = XPlat::SocketUtils::InvalidPort;
const Rank UnknownRank = (Rank)-1;

const char *empty_str = NULL_STRING;

// Added by Taylor:
const int MIN_DYNAMIC_PORT = 1024;

void init_local(void)
{
#if !defined(os_windows)
    // Make sure any data sockets we create have descriptors != {1,2}
    // to avoid nasty bugs where fprintf(stdout/stderr) writes to data sockets.
    int fd;
    while( (fd = socket( AF_INET, SOCK_STREAM, 0 )) <= 2 ) {
        /* Code for closing descriptor on exec*/
        int fdflag = fcntl(fd, F_GETFD );
        if( fdflag == -1 )
        {
            // failed to retrive the fd  flags
        fprintf(stderr, "F_GETFD failed!\n");
        }
        int fret = fcntl( fd, F_SETFD, fdflag | FD_CLOEXEC );
        if( fret == -1 )
        {
            // we failed to set the fd flags
           fprintf(stderr, "F_SETFD failed!\n");
        }

        if( fd == -1 ) break;
    } 
    if( fd > 2 ) XPlat::SocketUtils::Close(fd);
#else
    // init Winsock
    WORD version = MAKEWORD(2, 2); /* socket version 2.2 supported by all modern Windows */
    WSADATA data;
    if( WSAStartup(version, &data) != 0 )
        fprintf(stderr, "WSAStartup failed!\n");
    if( (LOBYTE(data.wVersion) != 2) || (HIBYTE(data.wVersion) != 2) ) {
        mrn_dbg(1, mrn_printf(FLF, stderr,
            "Warning: Received Winsock socket version other than requested.\n"
            "Requested %d.%d, got %d.%d\n",
            HIBYTE(version), LOBYTE(version), HIBYTE(data.wVersion),
            LOBYTE(data.wVersion)));
    }
#endif
}

void cleanup_local(void)
{
#if defined(os_windows)
    // cleanup Winsock
    WSACleanup();
#endif
}

/*===========================================================*/
/*             Network DEFINITIONS        */
/*===========================================================*/
Network::Network(void)
    : _local_port(UnknownPort), 
      _local_rank(UnknownRank), 
      _network_topology(NULL), 
      _failure_manager(NULL), 
      _bcast_communicator(NULL), 
      _local_front_end_node(NULL), 
      _local_back_end_node(NULL), 
      _local_internal_node(NULL), 
      _local_time_keeper( new TimeKeeper() ),
      _evt_mgr( new EventMgr() ),
      _next_user_stream_id(USER_STRM_BASE_ID),
      _next_int_stream_id(INTERNAL_STRM_BASE_ID),
      _threaded(true), 
      _recover_from_failures(true),
      _was_shutdown(false), 
      _shutting_down(false),
      _startup_timeout(120),
      _topo_update_timeout_msec(250),
      _perf_data( new PerfDataMgr() ),
      _net_filters(new std::map< unsigned short, FilterInfo >())
{
    Filter::initialize_static_stuff(_net_filters);
    if(XPlat_TLSKey == NULL) {
        XPlat_TLSKey = new TLSKey();
    }

    init_local();

    _shutdown_sync.RegisterCondition( NETWORK_TERMINATION );
    _edt = new EventDetector(this);
}

Network::~Network(void)
{
    shutdown_Network( );
    clear_EndPoints();

    if( _perf_data != NULL ) {
        delete _perf_data;
        _perf_data = NULL;
    }
    if( parsed_graph != NULL ) {
        delete parsed_graph;
        parsed_graph = NULL;
    }
    if( _network_topology != NULL ) {
        delete _network_topology;
        _network_topology = NULL;
    }
    if( _failure_manager != NULL ) {
        delete _failure_manager;
        _failure_manager = NULL;
    }
    if( _bcast_communicator != NULL ) {
        delete _bcast_communicator;
        _bcast_communicator = NULL;
    }
    if( _local_front_end_node != NULL ) {
        delete _local_front_end_node;
        _local_front_end_node = NULL;
    }
    if( _local_back_end_node != NULL ) {
        delete _local_back_end_node;
        _local_back_end_node = NULL;
    }
    if( _local_internal_node != NULL ) {
        delete _local_internal_node;
        _local_internal_node = NULL;
    }
    if( _local_time_keeper != NULL ) {
        delete _local_time_keeper;
        _local_time_keeper = NULL;
    }
    if( _evt_mgr != NULL ) {
        delete _evt_mgr;
        _evt_mgr = NULL;
    }

    cleanup_local();
    free_ThreadState();
}

void Network::close_Streams(void)
{
    map< unsigned int, Stream* >::iterator miter, tmpiter;

    _streams_sync.Lock();

    for( miter = _streams.begin(); miter != _streams.end(); miter++ ) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "closing stream with id=%u\n", miter->first));
        (*miter).second->close();
    }
    for( miter = _internal_streams.begin(); miter != _internal_streams.end(); ) {
        /* Need to be safe with iterator use. ~Stream() calls Network::delete_Stream(), 
           which erases it from the map. To prevent that erase from mucking with the
           increment of miter, we do the erase ourselves in a safe manner. */
        Stream* strm = (*miter).second;
        tmpiter = miter++;
        mrn_dbg(5, mrn_printf(FLF, stderr, "deleting stream with id=%u\n", tmpiter->first));
        _internal_streams.erase( tmpiter );
        delete strm;
    }

    _streams_sync.Unlock();
}

void Network::shutdown_Network(void)
{
    mrn_dbg_func_begin();

    int thd_ret;
    XPlat::Thread::Id edt_tid = _edt->get_ThrId();

    // Only the FE starts shutdown with the main thread and not the EDT,
    // so it needs to disable the EDT.
    if( is_LocalNodeFrontEnd() ) {
        _edt->disable();
    }

    // Get rid of all send/recv threads
    if( is_LocalNodeParent() ) {
        vector<XPlat::Thread::Id> child_send_thds;

        // Flush out any messages we still haven't sent.
        // This is here to guarantee that any user requests made before the
        // network is deleted are taken care of.  We don't care about pending
        // internal requests completing.
        // NOTE: We also don't really care about the return value here.
        flush_PacketsToChildren();

        // Join recv threads first
        _children_mutex.Lock();
        set<PeerNodePtr>::iterator ch_iter = _children.begin();
        for( ; ch_iter != _children.end(); ++ch_iter ) {
            PeerNodePtr cur_child(*ch_iter);
            XPlat::Thread::Id cur_recv_thd = cur_child->get_RecvThrId();
            child_send_thds.push_back(cur_child->get_SendThrId());

            if( 0 != cur_recv_thd ) {
                if( ! cur_child->stop_CommunicationThreads() ) {
                    thd_ret = XPlat::Thread::Cancel(cur_recv_thd);
                    if( 0 != thd_ret ) {
                        mrn_dbg(1, mrn_printf(FLF, stderr,
                                    "Thread::Cancel failed: %s\n",
                                    strerror(thd_ret)));
                    }
                }
                thd_ret = XPlat::Thread::Join(cur_recv_thd, (void **)NULL);
                if( 0 != thd_ret ) {
                    mrn_dbg(1, mrn_printf(FLF, stderr,
                                "Thread::Join failed: %s\n",
                                strerror(thd_ret)));
                }
            }
        }
        _children_mutex.Unlock();

        // Second loop to join all send threads
        vector<XPlat::Thread::Id>::iterator s_iter = child_send_thds.begin();
        for( ; s_iter != child_send_thds.end(); ++s_iter ) {
            if( 0 != *s_iter ) {
                thd_ret = XPlat::Thread::Join(*s_iter, (void **)NULL);
                if( 0 != thd_ret ) {
                    mrn_dbg(1, mrn_printf(FLF, stderr,
                                "Thread::Join failed: %s\n",
                                strerror(thd_ret)));
                }
            }
        }
    }

    if( is_LocalNodeChild() ) {
        if( PeerNode::NullPeerNode == _parent ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, 
                        "Shutdown called after parent deleted!\n") );
        } else {
            _parent_sync.Lock();

            XPlat::Thread::Id par_send_tid = _parent->get_SendThrId();
            XPlat::Thread::Id par_recv_tid = _parent->get_RecvThrId();

            if( 0 != par_recv_tid ) {
                if( ! _parent->stop_CommunicationThreads() ) {
                    thd_ret = XPlat::Thread::Cancel(par_recv_tid);
                    if( 0 != thd_ret ) {
                        mrn_dbg(1, mrn_printf(FLF, stderr,
                                    "Thread::Cancel failed: %s\n",
                                    strerror(thd_ret)));
                    }
                }
                thd_ret = XPlat::Thread::Join(par_recv_tid, (void **)NULL);
                if( 0 != thd_ret ) {
                    mrn_dbg(1, mrn_printf(FLF, stderr,
                                "Thread::Join failed: %s\n",
                                strerror(thd_ret)));
                }
            }

            if( 0 != par_send_tid ) {
                thd_ret = XPlat::Thread::Join(par_send_tid, (void **)NULL);
                if( 0 != thd_ret ) {
                    mrn_dbg(1, mrn_printf(FLF, stderr,
                                "Thread::Join failed: %s\n",
                                strerror(thd_ret)));
                }
            }

            _parent_sync.Unlock();
        }
    }

    if( is_LocalNodeParent() ) {
        // Request the EDT to broadcast the shutdown message to our children, if any
        if( ! _edt->start_ShutDown() ) {
            mrn_dbg(1, mrn_printf(FLF, stderr,
                                  "Failed to start the shutdown process!\n"));
            goto final_shutdown;
        }
    }
    if( 0 != edt_tid ) {
        thd_ret = XPlat::Thread::Join(edt_tid, (void **)NULL);
        if( 0 != thd_ret ) {
            mrn_dbg(1, mrn_printf(FLF, stderr,
                                  "Thread::Join failed: %s\n",
                                  strerror(thd_ret)));
        }
    }

final_shutdown:
    close_Streams();

    _evt_mgr->clear_Events();

    // on to delete the rest of our state!

    mrn_dbg_func_end();
}

bool Network::is_ShutDown(void) const
{
    bool rc;
    _shutdown_sync.Lock();
    rc = _was_shutdown;
    _shutdown_sync.Unlock();
    return rc;
}

bool Network::is_ShuttingDown(void) const
{
    bool rc;
    _shutdown_sync.Lock();
    rc = _shutting_down;
    _shutdown_sync.Unlock();
    return rc;
}

void Network::set_ShuttingDown(void)
{
    _shutdown_sync.Lock();
    _shutting_down = true;
    _shutdown_sync.Unlock();
}

void Network::waitfor_ShutDown(void) const
{
    mrn_dbg_func_begin();

    _shutdown_sync.Lock();
    
    while( ! _was_shutdown )
        _shutdown_sync.WaitOnCondition( NETWORK_TERMINATION );

    _shutdown_sync.Unlock();
}

void Network::signal_ShutDown(void)
{
    mrn_dbg_func_begin();

    // make sure _was_shutdown has been set to true
    _shutdown_sync.Lock();
    if( ! _was_shutdown )
        _was_shutdown = true;
    _shutdown_sync.Unlock();

    // notify any blocking recv() calls
    _streams_sync.Lock();
    _streams_sync.SignalCondition( STREAMS_NONEMPTY );
    _streams_sync.Unlock();

    _shutdown_sync.Lock();
    _shutdown_sync.SignalCondition( NETWORK_TERMINATION );
    _shutdown_sync.Unlock();

    mrn_dbg_func_end();
}

int Network::broadcast_ShutDown(void)
{
    int rc = 0;
    vector<XPlat_Socket> child_socks;
    mrn_dbg_func_begin();

    size_t bb_size=8;
    char *bogus_buffer = (char *) malloc(sizeof(char)*bb_size);
    if( NULL == bogus_buffer ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Failed to allocate bogus buffer!\n"));
        return -1;
    }
    PacketPtr packet(new Packet(CTL_STRM_ID, PROT_EDT_REMOTE_SHUTDOWN, NULL) );
    _children_mutex.Lock();

    set<PeerNodePtr>::iterator iter = _children.begin();

    // Broadcast the shutdown packet
    for( ; iter != _children.end(); ++iter ) {
        PeerNodePtr cur_child(*iter);
        XPlat_Socket cur_sock = cur_child->get_EventSocketFd();
        if( XPlat::SocketUtils::InvalidSocket == cur_sock ) {
            mrn_dbg(1, mrn_printf(FLF, stderr,
                        "Cannot send shutdown message: Socket for rank %u is invalid\n",
                        cur_child->get_Rank()));
            continue;
        }

        Message msg(this);
        msg.add_Packet(packet);

        if( -1 == msg.send(cur_sock) ) {
            mrn_dbg(1, mrn_printf(FLF, stderr,
                        "Failed to send event packet to child!\n"));
            rc = -1;
        }

        // Shutdown the socket transmissions
        if( XPlat::SocketUtils::Shutdown(cur_sock, XPLAT_SHUT_WR) == -1 ) {
            // mrn_dbg(1, mrn_printf(FLF, stderr,
            //              "Failed to shutdown remote event socket!\n"));
            //rc = -1;
            continue;
        }

        // Only push back the sockets that had a successful shutdown
        child_socks.push_back(cur_sock);
    }


    // Read until 0
    // (only does this to the sockets that had a successful shutdown call)
    vector<XPlat_Socket>::iterator sock_iter = child_socks.begin();
    ssize_t recv_ret;
    for( ; sock_iter != child_socks.end(); ++sock_iter ) {
        mrn_dbg(3, mrn_printf(FLF, stderr,
                    "Waiting for remote socket %d to close\n", *sock_iter));
        while(true) {
            recv_ret = XPlat::SocketUtils::recv(*sock_iter, bogus_buffer,
                                                bb_size);
            mrn_dbg(5, mrn_printf(FLF, stderr,
                        "Socket %d recv returned %zd\n",
                        *sock_iter,
                        recv_ret));

            //NOTE: Currently XPlat does not report the difference between a
            //socket error and an orderly shutdown.
            if( recv_ret <= 0 ) {
                // Remote event socket has performed orderly shutdown
                // and we now have enough proof that it got our shutdown message
                // and won't try to recover from a failure
                break;
            } else {
                mrn_dbg(3, mrn_printf(FLF, stderr,
                            "Discarded " PRIsszt " bytes\n",
                            recv_ret));
            }
        }
    }

    // Explicitly close all event sockets
    iter = _children.begin();
    for( ; iter != _children.end(); ++iter ) {
        PeerNodePtr cur_child(*iter);
        cur_child->close_EventSocket();
    }

    _children_mutex.Unlock();

    free(bogus_buffer);
    mrn_dbg_func_end();
    return rc;
}

std::map< net_settings_key_t, std::string >& Network::get_SettingsMap()
{
    return _network_settings;
}

int Network::get_NetSettingKey( const std::string & s )
{
    if( s.empty() )
        return -1;

    int ret = -1;

    const char* cstr = s.c_str();
    if( 0 == strncmp("MRNET_", cstr, 6) ) {

        if( (strcmp("MRNET_OUTPUT_LEVEL", cstr) == 0) ||
            (strcmp("MRNET_DEBUG_LEVEL", cstr) == 0) )
            ret = MRNET_DEBUG_LEVEL;

        else if( strcmp("MRNET_DEBUG_LOG_DIRECTORY", cstr) == 0 )
            ret = MRNET_DEBUG_LOG_DIRECTORY;

        else if( (strcmp("MRNET_COMM_PATH", cstr) == 0) || 
                 (strcmp("MRNET_COMMNODE_PATH", cstr) == 0) )
            ret = MRNET_COMMNODE_PATH;

        else if( strcmp("MRNET_FAILURE_RECOVERY", cstr) == 0 )
            ret = MRNET_FAILURE_RECOVERY;

        else if( strcmp("MRNET_STARTUP_TIMEOUT", cstr) == 0 )
            ret = MRNET_STARTUP_TIMEOUT;

        else if( strcmp("MRNET_PORT_BASE", cstr) == 0 )
            ret = MRNET_PORT_BASE;

        else if( strcmp("MRNET_EVENT_WAIT_TIMEOUT_MSEC", cstr) == 0 )
            ret = MRNET_EVENT_WAIT_TIMEOUT_MSEC;

        else if( strcmp("MRNET_TOPOLOGY_UPDATE_TIMEOUT_MSEC", cstr) == 0 )
            ret = MRNET_TOPOLOGY_UPDATE_TIMEOUT_MSEC;
    }
    else if( 0 == strncmp("XPLAT_", cstr, 6) ) {

        if( strcmp("XPLAT_RSH", cstr) == 0 )
            ret = XPLAT_RSH;

        else if( strcmp("XPLAT_RSH_ARGS", cstr) == 0 )
            ret = XPLAT_RSH_ARGS;
        
        else if( strcmp("XPLAT_REMCMD", cstr) == 0 )
            ret = XPLAT_REMCMD;
    }
    else if( 0 == strncmp("CRAY_ALPS_", cstr, 10) ) {

        if( strcmp("CRAY_ALPS_APID", cstr) == 0 )
            ret = CRAY_ALPS_APID;

        else if( strcmp("CRAY_ALPS_APRUN_PID", cstr) == 0 )
            ret = CRAY_ALPS_APRUN_PID;

        else if( strcmp("CRAY_ALPS_STAGE_FILES", cstr) == 0 )
            ret = CRAY_ALPS_STAGE_FILES;
    }

    return ret;
}

void Network::convert_SettingsMap( const std::map< std::string, std::string > * iattrs )
{
    std::map<std::string, std::string>::const_iterator it;
    int key_ret;
    if( iattrs != NULL ) {
        for( it = iattrs->begin(); it != iattrs->end(); it++ ) {
            key_ret = Network::get_NetSettingKey( it->first );
            net_settings_key_t key = (net_settings_key_t)key_ret;
            if( key_ret != -1 )
                _network_settings[ key ] = it->second;
        }
    }
}

void Network::init_FE_NetSettings( const std::map< std::string, std::string > * iattrs )
{
    char* envval;
    convert_SettingsMap( iattrs );

    // initialize MRNet from environment if not passed in iattrs
    if( _network_settings.find(MRNET_DEBUG_LEVEL) == _network_settings.end() ) {
        envval = getenv( "MRNET_OUTPUT_LEVEL" );
        if( envval == NULL )
            envval = getenv( "MRNET_DEBUG_LEVEL" );
        if( envval != NULL )
            _network_settings[ MRNET_DEBUG_LEVEL ] = std::string( envval );
    }

    if( _network_settings.find(MRNET_DEBUG_LOG_DIRECTORY) == _network_settings.end() ) {
        envval = getenv( "MRNET_DEBUG_LOG_DIRECTORY" );
        if( envval != NULL )
            _network_settings[ MRNET_DEBUG_LOG_DIRECTORY ] = std::string( envval );
        else {
            char* home = getenv("HOME");
            if( home != NULL ) {
                char logdir[512];
                snprintf(logdir, sizeof(logdir), "%s/mrnet-log", home);
                _network_settings[ MRNET_DEBUG_LOG_DIRECTORY ] = std::string( logdir );
            }
        }
    }

    if( _network_settings.find(MRNET_COMMNODE_PATH) == _network_settings.end() ) {
        envval = getenv( "MRNET_COMM_PATH" );
        if( envval == NULL ) // for backwards compatibility, check MRN_COMM_PATH
            envval = getenv( "MRN_COMM_PATH" );
        if( envval != NULL )
            _network_settings[ MRNET_COMMNODE_PATH ] = std::string( envval );
        else
            _network_settings[ MRNET_COMMNODE_PATH ] = COMMNODE_EXE;
    }

    if( _network_settings.find(MRNET_STARTUP_TIMEOUT) == _network_settings.end() ) {
        envval = getenv( "MRNET_STARTUP_TIMEOUT" );
        if( envval != NULL ) {
            _network_settings[ MRNET_STARTUP_TIMEOUT ] = std::string( envval );
        }
    }
    
    if( _network_settings.find(MRNET_EVENT_WAIT_TIMEOUT_MSEC) == _network_settings.end() ) {
        envval = getenv("MRNET_EVENT_WAIT_TIMEOUT_MSEC");
        if( envval != NULL ) {
            _network_settings[ MRNET_EVENT_WAIT_TIMEOUT_MSEC ] =
                std::string( envval );
        }
    }

    if( _network_settings.find(MRNET_TOPOLOGY_UPDATE_TIMEOUT_MSEC) == _network_settings.end() ) {
        envval = getenv("MRNET_TOPOLOGY_UPDATE_TIMEOUT_MSEC");
        if( envval != NULL ) {
            _network_settings[ MRNET_TOPOLOGY_UPDATE_TIMEOUT_MSEC ] =
                std::string( envval );
        }
    }

    init_NetSettings();
}

void Network::init_NetSettings(void)
{
    // these settings are valid for any type of network

    std::map< net_settings_key_t, std::string >::iterator eit;

    eit = _network_settings.find( MRNET_DEBUG_LEVEL );
    if( eit != _network_settings.end() ) {
        CUR_OUTPUT_LEVEL = atoi( eit->second.c_str() );
    }

    eit = _network_settings.find( MRNET_DEBUG_LOG_DIRECTORY );
    if( eit != _network_settings.end() ) {
        MRN_DEBUG_LOG_DIRECTORY = strdup( eit->second.c_str() );
    }

    eit = _network_settings.find( MRNET_FAILURE_RECOVERY );
    if( eit != _network_settings.end() ) {
        if( ! strcmp( eit->second.c_str(), "0") )
            disable_FailureRecovery();
    }

    eit = _network_settings.find( MRNET_STARTUP_TIMEOUT );
    if( eit != _network_settings.end() ) {
        int timeout = atoi( eit->second.c_str() );
        set_StartupTimeout( timeout );
    }

    eit = _network_settings.find( MRNET_EVENT_WAIT_TIMEOUT_MSEC );
    if( eit != _network_settings.end() ) {
        int timeout_ms = atoi( eit->second.c_str() );
        _local_time_keeper->set_DefaultTimeout( timeout_ms );
    }
    
    eit = _network_settings.find( MRNET_TOPOLOGY_UPDATE_TIMEOUT_MSEC );
    if( eit != _network_settings.end() ) {
        int timeout_ms = atoi( eit->second.c_str() );
        _topo_update_timeout_msec = timeout_ms;
    }
}

int Network::get_StartupTimeout(void)
{
    return _startup_timeout;
}

void Network::set_StartupTimeout( int new_timeout )
{
    _startup_timeout = new_timeout;
}

void Network::update_BcastCommunicator(void)
{
    //add end-points to broadcast communicator
    _endpoints_mutex.Lock();

    if( _bcast_communicator == NULL )
        _bcast_communicator = new Communicator( this );

    map< Rank, CommunicationNode * >::const_iterator iter;
    for( iter=_end_points.begin(); iter!=_end_points.end(); iter++ )
        _bcast_communicator->add_EndPoint( iter->first );

    _endpoints_mutex.Unlock();
    mrn_dbg(5, mrn_printf(FLF, stderr, "Bcast communicator complete \n" ));
}

// setup thread local storage (TLS)
void Network::init_ThreadState( node_type_t node_type,
                                const char* thread_name /*=NULL*/ )
{
    const char *tname;
    Rank myrank = get_LocalRank();
    node_type_t mytype = node_type;

    if( mytype == UNKNOWN_NODE ) {
        if( is_LocalNodeFrontEnd() )
            mytype = FE_NODE;
        else if( is_LocalNodeBackEnd() )
            mytype = BE_NODE;
        else
            mytype = CP_NODE;
    }

    std::ostringstream nameStream;

    if( thread_name == NULL ) {
        switch( mytype ) {
        case FE_NODE:
            nameStream << "FE";
            break;
        case BE_NODE:
            nameStream << "BE";
            break;
        case CP_NODE:
            nameStream << "CP";
            break;
        default:
            nameStream << "UNKNOWN";
            break;
        }

        string myhost = get_LocalHostName();
        if( myhost.empty() )
            XPlat::NetUtils::GetLocalHostName(myhost);
        
        string prettyHost;
        XPlat::NetUtils::GetHostName( myhost, prettyHost );

        nameStream << "("
                   << prettyHost
                   << ":"
                   << myrank
                   << ")" ;
    }
    else
        nameStream << thread_name; 

    tname = (const char *)strdup( nameStream.str().c_str() );
    tsd_t *local_data = new tsd_t;
    local_data->process_rank = myrank;
    local_data->node_type = mytype;
    local_data->network = this;

    int status;
    if( (status = XPlat_TLSKey->InitTLS(tname, local_data)) != 0 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "XPlat::TLSKey::Set(): %s\n",
                               strerror(status)) );
    } 
}
void Network::free_ThreadState(void)
{
    tsd_t* tsd = (tsd_t*)XPlat_TLSKey->GetUserData();
    if( tsd != NULL ) {
        delete tsd;
        if(XPlat_TLSKey->SetUserData(NULL) != 0) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "Thread 0x%lx failed to set"
                        " thread-specific user data to NULL.\n",
                        XPlat::Thread::GetId()));
        }
        if(XPlat_TLSKey->DestroyData() != 0) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "Thread 0x%lx failed to destroy"
                        " thread-specific data.\n", XPlat::Thread::GetId()));
        }
    }
}

// initialize a Network in its role as front end
void Network::init_FrontEnd( const char * itopology,
                             const char * ibackend_exe,
                             const char **ibackend_args,
                             const std::map<std::string,std::string>* iattrs,
                             bool irank_backends,
                             bool iusing_mem_buf )
{
	mrn_dbg_func_begin();
    _network_sync.RegisterCondition( EVENT_NOTIFICATION );
    _streams_sync.RegisterCondition( STREAMS_NONEMPTY );
    _parent_sync.RegisterCondition( PARENT_NODE_AVAILABLE );

    if( parse_Configuration( itopology, iusing_mem_buf ) == -1 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "Failure: parse_Configuration( \"%s\" )!\n",
                              ( iusing_mem_buf ? "memory buffer" : itopology )));
        return;
    }

    if( ! parsed_graph->validate() ) {
        parsed_graph->perror( "ParsedGraph not valid." );
        return;
    }

    parsed_graph->assign_NodeRanks( irank_backends );

    Rank rootRank = parsed_graph->get_Root()->get_Rank();
    _local_rank = rootRank;

    string prettyHost, rootHost;
    rootHost = parsed_graph->get_Root()->get_HostName();
    XPlat::NetUtils::GetHostName( rootHost, prettyHost );
    if( ! XPlat::NetUtils::IsLocalHost( prettyHost ) ) {
        string lhost;
        XPlat::NetUtils::GetLocalHostName(lhost);
        mrn_dbg( 1, mrn_printf(FLF, stderr, 
                               "WARNING: Topology Root (%s) is not local host (%s)\n",
                               prettyHost.c_str(), lhost.c_str()) );
    }

    //TLS: setup thread local storage for frontend
    init_ThreadState( FE_NODE );

    _bcast_communicator = new Communicator( this );
    
    FrontEndNode* fen = CreateFrontEndNode( this, rootHost, rootRank );
    assert( fen != NULL );
    if( fen->has_Error() ) {
        error( ERR_INTERNAL, rootRank, 
               "Failed to create front-end node" );
        return;
    }

    init_FE_NetSettings( iattrs );

    std::string path;
    if( _network_settings.find(MRNET_COMMNODE_PATH) != _network_settings.end() ) {
        path = _network_settings[ MRNET_COMMNODE_PATH ];
    }
    if( path.empty() ) {
        error( ERR_INTERNAL, rootRank, 
               "path for mrnet_commnode is empty" );
        return;
    }

    if( ibackend_exe == NULL ) {
        ibackend_exe = empty_str;
    }

    unsigned int backend_argc = 0;
    if( ibackend_args != NULL ) {
        for( unsigned int i=0; ibackend_args[i] != NULL; i++ ) {
            backend_argc++;
        }
    }

    // create topology propagation stream
    Stream* s = new_InternalStream( _bcast_communicator, TFILTER_TOPO_UPDATE, 
                                    SFILTER_TIMEOUT, TFILTER_TOPO_UPDATE_DOWNSTREAM );
    if(s == NULL) {
        error( ERR_INTERNAL, rootRank, "failed to create topology update stream");
        shutdown_Network();
        return;
    }
    if( s->get_Id() != TOPOL_STRM_ID ) {
        error( ERR_INTERNAL, rootRank, "topology update stream id is wrong");
        shutdown_Network();
        return;
    }

    // set topology propagation stream timeout
    if( s->set_FilterParameters( FILTER_UPSTREAM_SYNC, "%ud", _topo_update_timeout_msec ) == -1) {
        error( ERR_INTERNAL,  rootRank, "failed to set filter parameters");
        shutdown_Network();
        return;
    }

    NetworkTopology * nt = get_NetworkTopology();
    NetworkTopology::Node * local_node = nt->find_Node(get_LocalRank());

    std::set<NetworkTopology::Node *> tmp = local_node->get_Children();
    for (std::set<NetworkTopology::Node *>::iterator i = tmp.begin(); i != tmp.end(); i++) {
        s->add_Stream_Peer((*i)->get_Rank());
    }

   
    // spawn and connect processes that constitute our network
    bool success = Instantiate( parsed_graph, 
                                path.c_str(), 
                                ibackend_exe, 
                                ibackend_args, 
                                backend_argc,
                                iattrs );


    delete parsed_graph;
    parsed_graph = NULL;

    if( ! success ) {
        error( ERR_NETWORK_FAILURE, rootRank,
               "Failed to instantiate the network." );
        // some TBON processes may have been started, so tell them to go away
        shutdown_Network();
        return;
    }

    // wait for startup confirmations
    mrn_dbg(5, mrn_printf(FLF, stderr, "Waiting for subtrees to report ... \n" ));
    if( ! get_LocalFrontEndNode()->waitfor_SubTreeInitDoneReports() ) {
        error( ERR_INTERNAL, rootRank, "waitfor_SubTreeInitDoneReports() failed");
        shutdown_Network();
        return;
    }
   
    mrn_dbg(5, mrn_printf(FLF, stderr, "Updating bcast communicator ... \n" ));
    update_BcastCommunicator();
    

    /* collect port updates and broadcast them
     * - this is a no-op on XT
     */
    PacketPtr packet( new Packet(CTL_STRM_ID, PROT_PORT_UPDATE, 
                                 empty_str) );
    if( -1 == get_LocalFrontEndNode()->proc_PortUpdates(packet) ) {
        error( ERR_INTERNAL, rootRank, "proc_PortUpdates() failed");
        shutdown_Network();
    }

}

void Network::send_TopologyUpdates(void)
{
    _network_topology->send_updates_buffer();
}


void Network::init_BackEnd(const char *iphostname, Port ipport, Rank iprank,
                           const char *imyhostname, Rank imyrank )
{
    _streams_sync.RegisterCondition( STREAMS_NONEMPTY );
    _parent_sync.RegisterCondition( PARENT_NODE_AVAILABLE );

    _local_rank = imyrank;

    //TLS: setup thread local storage for backend
    init_ThreadState( BE_NODE );

    // create the BE-specific stream
    new_Stream(imyrank, &imyrank, 1, (unsigned short)TFILTER_NULL, 
                                     (unsigned short)SFILTER_DONTWAIT,
                                     (unsigned short)TFILTER_NULL);   

    string myhostname(imyhostname);
    BackEndNode* ben = CreateBackEndNode( this, myhostname, imyrank,
                                         iphostname, ipport, iprank );
    assert( ben != NULL );
    if( ben->has_Error() ) 
        error( ERR_SYSTEM, imyrank, "Failed to initialize via CreateBackEndNode()" );
}

void Network::init_InternalNode( const char* iphostname,
                                 Port ipport,
                                 Rank iprank,
                                 const char* imyhostname,
                                 Rank imyrank,
                                 int idataSocket,
                                 Port idataPort )
{
    _parent_sync.RegisterCondition( PARENT_NODE_AVAILABLE );

    _local_rank = imyrank;

    //TLS: setup thread local storage for comm proc
    init_ThreadState( CP_NODE );

    string myhostname(imyhostname);
    InternalNode* in = CreateInternalNode( this, myhostname, imyrank,
                                           iphostname, ipport, iprank,
                                           idataSocket, idataPort );
    if( (in == NULL) || in->has_Error() )
        error( ERR_SYSTEM, imyrank, "Failed to initialize using CreateInternalNode()" );
}

int Network::parse_Configuration( const char* itopology, bool iusing_mem_buf )
{
    int status;

    if( iusing_mem_buf ) {
        // set up to parse config from a buffer in memory
        mrnBufPtr = itopology;
        mrnBufRemaining = (unsigned int)strlen( itopology );
    }
    else {
        // set up to parse config from the file named by our
        // 'filename' member variable
        mrnin = fopen( itopology, "r" );
        if( mrnin == NULL ) {
            int rc = errno;
            error( ERR_SYSTEM, get_LocalRank(), "fopen(%s): %s", itopology,
                   strerror(rc) );
            return -1;
        }
    }

    status = mrnparse( );

    if( status != 0 ) {
        error( ERR_TOPOLOGY_FORMAT, get_LocalRank(), "%s", itopology );
        return -1;
    }

    return 0;
}


void Network::print_error( const char *s )
{
    perror( s );
}

int Network::send_PacketsToParent( std::vector< PacketPtr > &ipackets )
{
    assert( is_LocalNodeChild() );

    vector< PacketPtr >::const_iterator iter = ipackets.begin(),
                                        iend = ipackets.end();
    for( ; iter != iend; iter++ )
        send_PacketToParent( *iter );

    return 0;
}

int Network::send_PacketToParent( PacketPtr ipacket )
{
    mrn_dbg_func_begin();
    
    unsigned int strm_id = ipacket->get_StreamId();
    Stream * strm = get_Stream( strm_id );
    PerfDataMgr* pdm = NULL;
    if( NULL != strm )
        pdm = strm->get_PerfData();

    if( NULL != pdm ) {
        if( pdm->is_Enabled(PERFDATA_MET_ELAPSED_SEC, 
                            PERFDATA_CTX_PKT_NET_SENDPAR) )
            ipacket->start_Timer(PERFDATA_PKT_TIMERS_NET_SENDPAR);
    }
 
    get_ParentNode()->send( ipacket );

    if( NULL != pdm ) {
        if( pdm->is_Enabled(PERFDATA_MET_ELAPSED_SEC, 
                            PERFDATA_CTX_PKT_NET_SENDPAR) )
            ipacket->stop_Timer(PERFDATA_PKT_TIMERS_NET_SENDPAR);
    }

    return 0;
}

int Network::flush_PacketsToParent(void) const
{
    return get_ParentNode()->flush();
}

int Network::send_PacketsToChildren( std::vector< PacketPtr > &ipackets  )
{
    assert( is_LocalNodeParent() );

    vector< PacketPtr >::const_iterator iter = ipackets.begin(),
                                        iend = ipackets.end();
    for( ; iter != iend; iter++ )
        send_PacketToChildren( *iter );

    return 0;
}

int Network::send_PacketToChildren( PacketPtr ipacket,
                                    bool iinternal_only /* =false */ )
{
    mrn_dbg_func_begin();

    unsigned int strm_id = ipacket->get_StreamId();
    Stream* strm = get_Stream( strm_id );
    PerfDataMgr* pdm = NULL;
    if( NULL != strm )
        pdm = strm->get_PerfData();

    if( NULL != pdm ) {
        if( pdm->is_Enabled(PERFDATA_MET_ELAPSED_SEC, 
                            PERFDATA_CTX_PKT_NET_SENDCHILD) )
            ipacket->start_Timer(PERFDATA_PKT_TIMERS_NET_SENDCHILD);
    }

    if( strm_id == CTL_STRM_ID ) { // control stream message

        _children_mutex.Lock();

        PeerNodePtr cur_node;
        std::set< PeerNodePtr >::const_iterator iter = _children.begin(),
                                                iend = _children.end();
        for( ; iter != iend; iter++ ) {
            cur_node = *iter;
            mrn_dbg( 5, mrn_printf(FLF, stderr, "node \"%s:%u:%hu\": %s, %s\n",
                                   cur_node->get_HostName().c_str(),
                                   cur_node->get_Rank(),
                                   cur_node->get_Port(),
                                   (cur_node->is_parent() ? "parent" : "child"),
                                   (cur_node->is_internal() ? "internal" : "end-point")) );
        
            //Never send packet back to inlet
            if( cur_node->get_Rank() == ipacket->get_InletNodeRank() )
                continue;
            
            //if internal_only, don't send to non-internal nodes
            if( iinternal_only && !cur_node->is_internal( ) )
                continue;
            
            cur_node->send( ipacket );
        }

        _children_mutex.Unlock();
    }
    else if( strm_id < CTL_STRM_ID ) {
        // BE stream (stream id is rank)
        PeerNodePtr cur_peer = get_OutletNode( (Rank)strm_id );
        if( cur_peer != PeerNode::NullPeerNode )
            cur_peer->send( ipacket );
    }
    else {

        // two cases: all stream peers, or subset according to BE destinations
        unsigned ndests;
        Rank* dests = NULL;
        if( ipacket->get_Destinations(ndests, &dests) ) {
            // packet has explicit BE destination list
            PeerNodePtr cur_peer;
            for( unsigned u=0; u < ndests; u++ ) {
                cur_peer = get_OutletNode( dests[u] );
                if( cur_peer != PeerNode::NullPeerNode )
                    cur_peer->send( ipacket );
                else
                    mrn_dbg( 5, mrn_printf(FLF, stderr, 
                                           "peer outlet is null\n") );
            }
        }
        else {
            // all peers in stream
            if( NULL == strm ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, 
                                       "stream %u lookup failed\n", strm_id));
                return -1;
            }
            strm->send_to_children( ipacket );
        }
    }

    if( NULL != pdm ) {
        if( pdm->is_Enabled(PERFDATA_MET_ELAPSED_SEC, 
                            PERFDATA_CTX_PKT_NET_SENDCHILD) )
            ipacket->stop_Timer(PERFDATA_PKT_TIMERS_NET_SENDCHILD);
    }

    mrn_dbg_func_end();
    return 0;
}

int Network::flush_PacketsToChildren(void) const
{
    int retval = 0;
    
    mrn_dbg_func_begin();
    
    _children_mutex.Lock();

    std::set< PeerNodePtr >::const_iterator iter = _children.begin(),
                                            iend = _children.end();
    for( ; iter != iend; iter++ ) {
  
        PeerNodePtr cur_node = *iter;

        mrn_dbg( 5, mrn_printf(FLF, stderr, "Calling child[%d].flush() ...\n",
                               cur_node->get_Rank()) );

        if( cur_node->flush() == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "child.flush() failed\n") );
            retval = -1;
        }
    }

    _children_mutex.Unlock();

    mrn_dbg( 5, mrn_printf(FLF, stderr, "flush_PacketsToChildren %s",
                retval == 0 ? "succeeded\n" : "failed\n") );
    return retval;
}

int Network::recv( bool iblocking )
{
    mrn_dbg_func_begin();

    if( is_ShutDown() || is_ShuttingDown() )
        return -1;

    if( is_LocalNodeFrontEnd() ) { 
        if( iblocking ){
            return waitfor_NonEmptyStream();
        }
        return 0;
    }
    else if( is_LocalNodeBackEnd() ){
        std::list <PacketPtr> packet_list;
        mrn_dbg(3, mrn_printf(FLF, stderr, "In backend.recv(%s)...\n",
                              (iblocking? "blocking" : "non-blocking") ));

        //if not blocking and no data present, return immediately
        if( ! iblocking && ! this->has_PacketsFromParent() ){
            return 0;
        }

        //check if we already have data
        if( is_LocalNodeThreaded() )
            return waitfor_NonEmptyStream();
        else {
            mrn_dbg(5, mrn_printf(FLF, stderr, "Calling recv_packets()\n"));
            if( recv_PacketsFromParent( packet_list ) == -1){
                mrn_dbg(1, mrn_printf(FLF, stderr, "recv_packets() failed\n"));
                return -1;
            }

            if( packet_list.size() == 0 ) {
                mrn_dbg(5, mrn_printf(FLF, stderr, "No packets read!\n"));
                return 0;
            }
            else {
                mrn_dbg(5, mrn_printf(FLF, stderr,
                                      "Calling proc_packets()\n"));
                if( get_LocalChildNode()->proc_PacketsFromParent(packet_list) == -1){
                    mrn_dbg(1, mrn_printf(FLF, stderr, "proc_packets() failed\n"));
                    return -1;
                }
                mrn_dbg(5, mrn_printf(FLF, stderr, "Found/processed packets! Returning\n"));
                return 1;
            }
        }
    }

    return -1; //shouldn't get here
}

int Network::recv( int *otag, PacketPtr &opacket, Stream **ostream, bool iblocking )
{
    mrn_dbg(3, mrn_printf(FLF, stderr, "blocking: \"%s\"\n",
                          (iblocking ? "yes" : "no")) );
    
    bool checked_network = false;   // have we checked sockets for input?
    PacketPtr cur_packet( Packet::NullPacket );

    if( ! have_Streams() && is_LocalNodeFrontEnd() ){
        //No streams exist -- bad for FE
        mrn_dbg(1, mrn_printf(FLF, stderr, "%s recv in FE when no streams "
                              "exist\n", (iblocking? "Blocking" : "Non-blocking") ));
        
        return -1;
    }

    // check streams for input
get_packet_from_stream_label:
    if( have_Streams() ) {

        bool packet_found=false;
        map <unsigned int, Stream *>::iterator start_iter;

        start_iter = _stream_iter;
        do {
            Stream* cur_stream = _stream_iter->second;

            cur_packet = cur_stream->get_IncomingPacket();
            if( cur_packet != Packet::NullPacket ){
                packet_found = true;
            }
            mrn_dbg( 5, mrn_printf(FLF, stderr,
                                   "packets %s on stream[%d]\n",
                                   (packet_found ? "found" : "not found"),
                                   cur_stream->get_Id()) );

            _streams_sync.Lock();
            _stream_iter++;
            if( _stream_iter == _streams.end() ){
                //wrap around to start of map entries
                _stream_iter = _streams.begin();
            }
            _streams_sync.Unlock();
        } while( (start_iter != _stream_iter) && ! packet_found );
    }

    if( cur_packet != Packet::NullPacket ) {
        *otag = cur_packet->get_Tag();
        *ostream = get_Stream( cur_packet->get_StreamId() );
        assert(*ostream);
        opacket = cur_packet;
        mrn_dbg( 5, mrn_printf(FLF, stderr, "cur_packet tag:%d, fmt:%s\n",
                               cur_packet->get_Tag(), 
                               cur_packet->get_FormatString()) );
        return 1;
    }

    if( iblocking || ! checked_network ) {

        // No packets are already in the stream
        // check whether there is data waiting to be read on our sockets
        int retval = recv( iblocking );
        checked_network = true;

        if( retval == -1 )
            return -1;
        else if ( retval == 1 ){
            // go back if we found a packet
            mrn_dbg( 5, mrn_printf(FLF, stderr, "Network::recv() found a packet!\n" ));
            goto get_packet_from_stream_label;
        }
    }
    mrn_dbg( 5, mrn_printf(FLF, stderr, "Network::recv() No packets found.\n" ));
    return 0;
}

int Network::send( Rank ibe, int itag, const char *iformat_str, ... )
{
    va_list arg_list;
    va_start(arg_list, iformat_str);
    PacketPtr new_packet( new Packet(get_LocalRank(), ibe, 
                                     itag, iformat_str, arg_list) );
    va_end(arg_list);

    return send( ibe, new_packet );
}

int Network::send( Rank ibe, const char *idata_fmt, va_list idata, int itag )
{
    PacketPtr new_packet( new Packet(idata_fmt, idata, ibe, itag) );
    return send( ibe, new_packet );
}

int Network::send( Rank ibe, int itag, 
                   const void **idata, const char *iformat_str )
{
    PacketPtr new_packet( new Packet(ibe, itag, idata, iformat_str) );
    return send( ibe, new_packet );
}

int Network::send( Rank ibe, PacketPtr& ipacket )
{
    if( ! is_LocalNodeFrontEnd() )
        return -1;

    ipacket->set_Destinations( &ibe, 1 );
    ipacket->set_SourceRank( get_LocalRank() );

    return send_PacketToChildren( ipacket );
}

int Network::flush(void) const
{
    if( is_LocalNodeFrontEnd() )
        return flush_PacketsToChildren();
    else if( is_LocalNodeBackEnd() )
        return flush_PacketsToParent();

    return -1;
}

CommunicationNode* Network::get_EndPoint( Rank irank ) const
{
    map< Rank, CommunicationNode * >::const_iterator iter =
        _end_points.find( irank );

    if( iter == _end_points.end() ){
        return NULL;
    }

    return (*iter).second;
}

Communicator* Network::get_BroadcastCommunicator(void) const
{
    return _bcast_communicator;
}

Communicator* Network::new_Communicator()
{
    return new Communicator(this);
}

Communicator* Network::new_Communicator( Communicator& comm )
{
    return new Communicator( this, comm );
}

Communicator* Network::new_Communicator( const set< Rank >& endpoints )
{
    return new Communicator( this, endpoints );
}

Communicator* Network::new_Communicator( set< CommunicationNode* >& endpoints )
{
    return new Communicator( this, endpoints );
}

CommunicationNode* Network::new_EndPoint( string &ihostname, 
                                          Port iport, Rank irank )
{
    return new CommunicationNode(ihostname, iport, irank);
}


Stream* Network::new_Stream( Communicator *icomm, 
                             int ius_filter_id /*=TFILTER_NULL*/,
                             int isync_filter_id /*=SFILTER_WAITFORALL*/, 
                             int ids_filter_id /*=TFILTER_NULL*/ )
{
    if( NULL == icomm ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "cannot create stream from NULL communicator\n") );
        return NULL;
    }
    if( is_LocalNodeBackEnd() ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "new_Stream() called from back-end\n") );
        return NULL;
    }

    // seems like a good time to send any topology updates
    send_TopologyUpdates();

    //get array of back-ends from communicator
    Rank* backends = icomm->get_Ranks();
    uint32_t num_pts = icomm->size();
    if( num_pts == 0 ) {
        if( icomm != _bcast_communicator ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, 
                       "cannot create stream from communicator containing zero end-points\n") );
            return NULL;
        }
    }

    PacketPtr packet( new Packet(CTL_STRM_ID, PROT_NEW_STREAM, "%ud %ad %d %d %d",
                                 _next_user_stream_id, backends, num_pts,
                                 ius_filter_id, isync_filter_id, ids_filter_id) );
    _next_user_stream_id++;

    Stream* stream = get_LocalFrontEndNode()->proc_newStream(packet);
    if( stream == NULL )
        mrn_dbg( 3, mrn_printf(FLF, stderr, "proc_newStream() failed\n" ));

    delete [] backends;
    return stream;
}

Stream* Network::new_Stream( Communicator* icomm,
                             std::string us_filters,
                             std::string sync_filters,
                             std::string ds_filters )
{
    if( NULL == icomm ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "NULL communicator\n"));
        return NULL;
    }
    if( is_LocalNodeBackEnd() ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "new_Stream() called from back-end\n"));
        return NULL;
    }

    //get array of back-ends from communicator
    Rank* backends = icomm->get_Ranks();
    unsigned num_pts = icomm->size();
    if( num_pts == 0 ) {
        if( icomm != _bcast_communicator ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, 
                                  "communicator contains zero end-points\n"));
            return NULL;
        }
    }

    PacketPtr packet( new Packet(CTL_STRM_ID, PROT_NEW_HETERO_STREAM, "%ud %ad %s %s %s",
                                _next_user_stream_id, backends, num_pts,
                                 us_filters.c_str(), sync_filters.c_str(), 
                                 ds_filters.c_str()) );
    _next_user_stream_id++;

    Stream* stream = get_LocalFrontEndNode()->proc_newStream(packet);
    if( stream == NULL ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "proc_newStream() failed\n"));
    }

    delete [] backends;
    return stream;
}

Stream* Network::new_InternalStream( Communicator *icomm, 
                                     int ius_filter_id /*=TFILTER_NULL*/,
                                     int isync_filter_id /*=SFILTER_WAITFORALL*/, 
                                     int ids_filter_id /*=TFILTER_NULL*/ )
{
    if( NULL == icomm ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "cannot create stream from NULL communicator\n") );
        return NULL;
    }
    if( is_LocalNodeBackEnd() ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "new_InternalStream() called from back-end\n") );
        return NULL;
    }

    //get array of back-ends from communicator
    Rank* backends = icomm->get_Ranks();
    unsigned num_pts = icomm->size();
    if( num_pts == 0 ) {
        if( icomm != _bcast_communicator ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, 
                       "cannot create stream from communicator containing zero end-points\n") );
            return NULL;
        }
    }

    PacketPtr packet( new Packet(CTL_STRM_ID, PROT_NEW_INTERNAL_STREAM, "%ud %ad %d %d %d",
                                 _next_int_stream_id, backends, num_pts,
                                 ius_filter_id, isync_filter_id, ids_filter_id) );
    _next_int_stream_id++;

    Stream* stream = get_LocalFrontEndNode()->proc_newStream(packet);
    if( stream == NULL )
        mrn_dbg( 3, mrn_printf(FLF, stderr, "proc_newStream() failed\n" ));

    delete [] backends;

    return stream;
}

bool Network::is_UserStreamId( unsigned int id )
{
    return (id >= USER_STRM_BASE_ID) || (id < CTL_STRM_ID);
}

Stream* Network::new_Stream( unsigned int iid,
                             Rank* ibackends,
                             unsigned int inum_backends,
                             unsigned short ius_filter_id,
                             unsigned short isync_filter_id,
                             unsigned short ids_filter_id )
{
    mrn_dbg_func_begin();

    Stream* stream = new Stream( this, iid, ibackends, inum_backends,
                                 ius_filter_id, isync_filter_id, ids_filter_id );

    _streams_sync.Lock();

    if( is_UserStreamId(iid) ) {
        _streams[iid] = stream;
        if( _streams.size() == 1 )
            _stream_iter = _streams.begin();
    }
    else
        _internal_streams[iid] = stream;

    _streams_sync.Unlock();

    mrn_dbg_func_end();

    return stream;
}

Stream* Network::get_Stream( unsigned int iid ) const
{
    Stream* ret = NULL;
    if( CTL_STRM_ID == iid ) return ret;

    _streams_sync.Lock();

    map< unsigned int, Stream* >::const_iterator iter; 
    if( is_UserStreamId(iid) ) {
        iter = _streams.find( iid );
        if( iter != _streams.end() )
            ret = iter->second;
    }
    else {
        iter = _internal_streams.find( iid );
        if( iter != _internal_streams.end() )
            ret = iter->second;
    }

    _streams_sync.Unlock();

    if( (ret == NULL) && is_LocalNodeFrontEnd() && (iid < CTL_STRM_ID) ) {
        // generate BE stream instance as needed for valid BE rank
        Rank r = iid;
        NetworkTopology::Node* be = _network_topology->find_Node( r );
        if( be != NULL ) {
            Network* me = const_cast< Network* >( this );
            ret = me->new_Stream( r, &r, 1, 
                                  (unsigned short)TFILTER_NULL, 
                                  (unsigned short)SFILTER_DONTWAIT, 
                                  (unsigned short)TFILTER_NULL );
        }
    }

    mrn_dbg(5, mrn_printf(FLF, stderr, "network(%p): stream[%d] = %p\n", this, iid, ret));
    return ret;
}

void Network::delete_Stream( unsigned int iid )
{
    map< unsigned int, Stream* >::iterator iter; 

    _streams_sync.Lock();

    // is it a user stream?
    if( is_UserStreamId(iid) ) {
        iter = _streams.find( iid );
        if( iter != _streams.end() ) {
            //if we are about to delete _stream_iter, set it to next elem. (w/wrap)
            if( iter == _stream_iter ) {
                _stream_iter++;
                if( _stream_iter == _streams.end() ) {
                    _stream_iter = _streams.begin();
                }
            }
            _streams.erase( iter );
        }
    }
    else { // must be an internal stream
        iter = _internal_streams.find( iid );
        if( iter != _internal_streams.end() ) {
            _internal_streams.erase( iter );
        }
    }

    _streams_sync.Unlock();
}

bool Network::have_Streams( )
{
    bool ret;
    _streams_sync.Lock();
    ret = !_streams.empty();
    _streams_sync.Unlock();
    return ret;
}

/* Event Management */
void Network::clear_Events(void)
{
    _evt_mgr->clear_Events();
}

unsigned int Network::num_EventsPending(void)
{
    return _evt_mgr->get_NumEvents();
}

Event* Network::next_Event(void)
{
    return _evt_mgr->get_NextEvent();
}

/* Event Notification by File Descriptor */
int Network::get_EventNotificationFd( EventClass eclass )
{
#if !defined(os_windows)
    EventPipe* ep;
    map< EventClass, EventPipe* >::iterator iter = _evt_pipes.find( eclass );
    if( iter == _evt_pipes.end() ) {
        ep = new EventPipe();
        bool rc = _evt_mgr->register_Callback( eclass, Event::EVENT_TYPE_ALL, 
                                               PipeNotifyCallbackFn, (void*)ep,
                                               false );
        if( ! rc ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "failed to register PipeNotifyCallbackFn"));
            delete ep;
            return -1;
        }
        _evt_pipes[eclass] = ep;
    }
    else
        ep = iter->second;

    return ep->get_ReadFd();
#else
    return -1;
#endif
}

void Network::clear_EventNotificationFd( EventClass eclass )
{
#if !defined(os_windows)
    map< EventClass, EventPipe* >::iterator iter = _evt_pipes.find( eclass );
    if( iter != _evt_pipes.end() ) {
        EventPipe* ep = iter->second;
        ep->clear();
    }
#endif
}

void Network::close_EventNotificationFd( EventClass eclass )
{
#if !defined(os_windows)
    map< EventClass, EventPipe* >::iterator iter = _evt_pipes.find( eclass );
    if( iter != _evt_pipes.end() ) {
        EventPipe* ep = iter->second;
        bool rc = _evt_mgr->remove_Callback( PipeNotifyCallbackFn, 
                                             eclass, Event::EVENT_TYPE_ALL );
        if( ! rc ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "failed to remove PipeNotifyCallbackFn"));
        }
        else {
            delete ep;
            _evt_pipes.erase( iter );
        }
    }
#endif
}

/* Register & Remove Callbacks */
bool Network::register_EventCallback( EventClass iclass, EventType ityp, 
                                      evt_cb_func ifunc, void* idata,
                                      bool onetime /*=false*/ )
{
    bool ret = _evt_mgr->register_Callback( iclass, ityp, ifunc, idata, onetime );
    if( ret == false ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "failed to register callback"));
        return false;
    }
    return true;
}

bool Network::remove_EventCallback( evt_cb_func ifunc, 
                                    EventClass iclass, EventType ityp )
{
    bool ret = _evt_mgr->remove_Callback( ifunc, iclass, ityp );
    if( ret == false ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "failed to remove Callback function for Topology Event\n"));
        return false;
    }
    return true;
}

bool Network::remove_EventCallbacks( EventClass iclass, EventType ityp )
{
    bool ret = _evt_mgr->remove_Callbacks( iclass, ityp );
    if( ret == false ) {    
        mrn_dbg(1, mrn_printf(FLF, stderr, "failed to remove Callback function for Topology Event\n"));
        return false;
    }
    return true;
}

/* Performance Data Management */
bool Network::enable_PerformanceData( perfdata_metric_t metric, 
                                      perfdata_context_t context )
{
    mrn_dbg_func_begin();
    if( metric >= PERFDATA_MAX_MET )
        return false;
    if( context >= PERFDATA_MAX_CTX )
        return false;

    map<unsigned int, Stream*>::iterator iter; 
    _streams_sync.Lock();
    for( iter = _streams.begin(); iter != _streams.end(); iter++ ) {
        bool rc = (*iter).second->enable_PerformanceData( metric, context );
        if( rc == false ) {
           mrn_dbg(1, mrn_printf(FLF, stderr, 
                                 "strm->enable_PerformanceData() failed, cancelling prior enables\n"));
           while( iter != _streams.begin() ) {
              iter--;
              (*iter).second->disable_PerformanceData( metric, context );
           }
           _streams_sync.Unlock();
           return false;
        }
    }
    _streams_sync.Unlock();

    mrn_dbg_func_end();
    return true;
}

bool Network::disable_PerformanceData( perfdata_metric_t metric, 
                                       perfdata_context_t context )
{
    mrn_dbg_func_begin();
    if( metric >= PERFDATA_MAX_MET )
        return false;
    if( context >= PERFDATA_MAX_CTX )
        return false;

    bool ret = true;
    map< unsigned int, Stream* >::iterator iter; 
    _streams_sync.Lock();
    for( iter = _streams.begin(); iter != _streams.end(); iter++ ) {
        bool rc = (*iter).second->disable_PerformanceData( metric, context );
        if( rc == false ) {
            ret = false;
            mrn_dbg( 1, mrn_printf(FLF, stderr, 
                     "strm->disable_PerformanceData() failed for strm %d\n", iter->first));
        }
    }
    _streams_sync.Unlock();

    mrn_dbg_func_end();
    return ret;
}

bool Network::collect_PerformanceData( map< int, rank_perfdata_map >& results,
                                       perfdata_metric_t metric, 
                                       perfdata_context_t context,
                                       int aggr_filter_id /*=TFILTER_CONCAT*/ )
{
    mrn_dbg_func_begin();
    if( metric >= PERFDATA_MAX_MET )
        return false;
    if( context >= PERFDATA_MAX_CTX )
        return false;

    bool ret = true;

    map< unsigned int, Stream* >::const_iterator citer;
    _streams_sync.Lock();
    for( citer = _streams.begin(); citer != _streams.end(); citer++ ) {

        rank_perfdata_map& strm_results = results[citer->first];
        
        bool rc = (*citer).second->collect_PerformanceData( strm_results, metric, context, 
                                                            aggr_filter_id );
        if( rc == false ) {
            ret = false;
            mrn_dbg( 1, mrn_printf(FLF, stderr, 
                     "strm->collect_PerformanceData() failed for strm %d\n", citer->first));
        }
    }
    _streams_sync.Unlock();

    mrn_dbg_func_end();
    return ret;
}

void Network::print_PerformanceData( perfdata_metric_t metric, 
                                     perfdata_context_t context )
{
    mrn_dbg_func_begin();
    if( metric >= PERFDATA_MAX_MET )
        return;
    if( context >= PERFDATA_MAX_CTX )
        return;

    map< unsigned int, Stream* >::const_iterator citer; 
    _streams_sync.Lock();
    for( citer = _streams.begin(); citer != _streams.end(); citer++ ) {
        (*citer).second->print_PerformanceData( metric, context );
    }
    _streams_sync.Unlock();

    mrn_dbg_func_end();
}


int Network::load_FilterFunc( const char* so_file, const char* func_name )
{
    std::vector< const char* > funcs;
    std::vector< int > fids;

    char* func = strdup(func_name);
    funcs.push_back( func );

    int rc = load_FilterFuncs( so_file, funcs, fids );
    if( rc == 1 )
        rc = fids[0];
    else
        rc = -1;

    if( func != NULL )
        free( func );

    return rc;
}

int Network::load_FilterFuncs( const char* so_file,
                               const std::vector< const char* > & functions,
                               std::vector< int > & filter_ids )
{
    // start user-defined filter ids at 100 to avoid conflicts with built-ins
    static unsigned short next_filter_id=100; 
    bool wait_ForChildren = true;
    mrn_dbg_func_begin();

    size_t nfuncs = functions.size();
    if( filter_ids.size() != nfuncs )
        filter_ids.resize( nfuncs );

    unsigned short* fids = (unsigned short*) calloc( nfuncs, sizeof(unsigned short) );
    char** funcs = (char**) calloc( nfuncs, sizeof(char*) );
    if( (fids == NULL) || (funcs == NULL) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "calloc() failed\n") );
        return 0;
    }

    char* so_copy = strdup(so_file);

    unsigned int success_count = 0;
    for( size_t u=0; u < nfuncs; u++ ) {

        unsigned short cur_filter_id = next_filter_id;
        const char* func_name = functions[u];

        int rc = Filter::load_FilterFunc(GetFilterInfo(), cur_filter_id, so_copy, func_name );
        if( rc == -1 ) {
            filter_ids[u] = -1;
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "Filter::load_FilterFunc(%hu, %s, %s) failed\n", 
                                   cur_filter_id, so_copy, func_name) );
        }
        else {
            filter_ids[u] = (int) cur_filter_id;
            fids[success_count] = cur_filter_id;
            funcs[success_count] = strdup(func_name);
            success_count++;
            next_filter_id++;
        }
    }

    if( success_count ) {
        //Filter registered locally, now propagate to tree
        //TODO: ensure that filter is loaded down the entire tree
        mrn_dbg( 3, mrn_printf(FLF, stderr, "sending PROT_NEW_FILTER\n") );
        PacketPtr packet( new Packet(CTL_STRM_ID, PROT_NEW_FILTER, "%s %as %auhd",
                                     so_copy, 
                                     funcs, success_count, 
                                     fids, success_count) );
        if (get_NumChildren() == 0)
            wait_ForChildren = false;
        send_PacketToChildren( packet );
        flush();
        if (wait_ForChildren){
            waitOn_ProtEvent();
        }

        // We have an error in loading filters elsewhere if this happens.
        if (_filter_error_hosts.size() > 0) {
            std::set<unsigned> load_error;
            for (unsigned u = 0; u < _filter_error_hosts.size(); u++) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "Error Loading Filter %s : %s on %s\n",
                _filter_error_sonames[u], funcs[_filter_error_funcids[u]], _filter_error_hosts[u]));
                load_error.insert(_filter_error_funcids[u]);
            }
            success_count =  success_count - (unsigned) load_error.size();
        }
    }

    
    free( so_copy );
    free( fids );
    for( unsigned u=0; u < success_count; u++ )
        free( funcs[u] );
    free( funcs );

    mrn_dbg_func_end();
    return success_count;
}

void Network::waitOn_ProtEvent(void) {
    // Wait on the reception of a PROT_EVENT message
    _network_sync.Lock();
    _network_sync.WaitOnCondition( EVENT_NOTIFICATION );
    _network_sync.Unlock();
}

void Network::signal_ProtEvent(std::vector<char *> hostnames, 
                               std::vector<char *> so_names, 
                               std::vector<unsigned> func_ids) {
    _network_sync.Lock();
    _filter_error_hosts = hostnames;
    _filter_error_sonames = so_names;
    _filter_error_funcids = func_ids;
    _network_sync.SignalCondition( EVENT_NOTIFICATION );
    _network_sync.Unlock();
}


int Network::waitfor_NonEmptyStream(void)
{
    mrn_dbg_func_begin();

    Stream* cur_strm = NULL;
    map< unsigned int, Stream * >::const_iterator iter;
    _streams_sync.Lock();
    while( true ) { 

        // first, check for data available 
        for( iter = _streams.begin(); iter != _streams.end(); iter++ ) {
            cur_strm = iter->second;
            mrn_dbg(5, mrn_printf(FLF, stderr, "Checking stream[%d] (%p) for data\n",
                                  cur_strm->get_Id(), cur_strm ));
            if( cur_strm->has_Data() ) {
                mrn_dbg(5, mrn_printf(FLF, stderr, "Data on stream[%d]\n",
                                      cur_strm->get_Id() ));
                _streams_sync.Unlock();
                mrn_dbg_func_end();
                return 1;
            }
        }

        // if no data, have we shutdown?
        if( is_ShutDown() )
            break;

        // not shutdown, any streams now closed?
        for( iter = _streams.begin(); iter != _streams.end(); iter++ ) {
            cur_strm = iter->second;
            if( cur_strm->is_Closed() ) {
                unsigned int cur_id = iter->first;
                _streams_sync.Unlock();
                mrn_dbg(5, mrn_printf(FLF, stderr, "stream[%d] has been closed\n",
                                      cur_id ));
                delete_Stream( cur_id );
                mrn_dbg_func_end();
                return 0;
            }
        }

        mrn_dbg(5, mrn_printf(FLF, stderr, "Waiting on CV[STREAMS_NONEMPTY] ...\n"));
        _streams_sync.WaitOnCondition( STREAMS_NONEMPTY );

    }
    _streams_sync.Unlock();
    return -1;
}

void Network::signal_NonEmptyStream( Stream* strm )
{
    _streams_sync.Lock();
    mrn_dbg(5, mrn_printf(FLF, stderr, "Signaling CV[STREAMS_NONEMPTY] ...\n"));
    _streams_sync.SignalCondition( STREAMS_NONEMPTY );

    DataEvent::DataEventData* ded = new DataEvent::DataEventData( strm );
    DataEvent* de = new DataEvent( DataEvent::DATA_AVAILABLE, ded );
    _evt_mgr->add_Event( de );

    _streams_sync.Unlock();
}

/* Methods to access internal network state */

void Network::set_BackEndNode( BackEndNode* iback_end_node )
{
    _local_back_end_node = iback_end_node;
}

void Network::set_FrontEndNode( FrontEndNode* ifront_end_node )
{
    _local_front_end_node = ifront_end_node;
}

void Network::set_InternalNode( InternalNode* iinternal_node )
{
    _local_internal_node = iinternal_node;
}

void Network::set_NetworkTopology( NetworkTopology* inetwork_topology )
{
    _network_topology = inetwork_topology;
}

void Network::set_LocalHostName( string const& ihostname )
{
    _local_hostname = ihostname;
}

void Network::set_LocalPort( Port iport )
{
    _local_port = iport;
}

void Network::set_LocalRank( Rank irank )
{
    _local_rank = irank;
}

void Network::set_FailureManager( CommunicationNode* icomm )
{
    if( _failure_manager != NULL ) {
        delete _failure_manager;
    }
    _failure_manager = icomm;
}

NetworkTopology* Network::get_NetworkTopology(void) const
{
    return _network_topology;
}

FrontEndNode* Network::get_LocalFrontEndNode(void) const
{
    return _local_front_end_node;
}

InternalNode* Network::get_LocalInternalNode(void) const
{
    return _local_internal_node;
}

BackEndNode* Network::get_LocalBackEndNode(void) const
{
    return _local_back_end_node;
}

ChildNode* Network::get_LocalChildNode(void) const
{
    if( is_LocalNodeInternal() )
        return (ChildNode*)(_local_internal_node);
    else
        return (ChildNode*)(_local_back_end_node) ;
}

ParentNode* Network::get_LocalParentNode(void) const
{
    if( is_LocalNodeInternal() )
        return dynamic_cast< ParentNode* >(_local_internal_node);
    else
        return dynamic_cast< ParentNode* >(_local_front_end_node); 
}

string Network::get_LocalHostName(void) const
{
    return _local_hostname;
}

Port Network::get_LocalPort(void) const
{
    return _local_port;
}

Rank Network::get_LocalRank(void) const
{
    return _local_rank;
}

XPlat_Socket Network::get_ListeningSocket(void) const
{
    assert( is_LocalNodeParent() );
    return get_LocalParentNode()->get_ListeningSocket();
}

CommunicationNode* Network::get_FailureManager(void) const
{
    return _failure_manager;
}

bool Network::is_LocalNodeFrontEnd(void) const
{
    return ( _local_front_end_node != NULL );
}

bool Network::is_LocalNodeBackEnd(void) const
{
    return ( _local_back_end_node != NULL );
}

bool Network::is_LocalNodeInternal(void) const
{
    return ( _local_internal_node != NULL );
}

bool Network::is_LocalNodeParent(void) const
{
    return ( is_LocalNodeFrontEnd() || is_LocalNodeInternal() );
}

bool Network::is_LocalNodeChild(void) const
{
    return ( is_LocalNodeBackEnd() || is_LocalNodeInternal() );
}

bool Network::is_LocalNodeThreaded(void) const
{
    return _threaded;
}

int Network::send_FilterStatesToParent(void)
{
    mrn_dbg_func_begin();
    map< unsigned int, Stream* >::iterator iter; 

    _streams_sync.Lock();

    for( iter = _streams.begin(); iter != _streams.end(); iter++ ) {
        (*iter).second->send_FilterStateToParent();
    }

    _streams_sync.Unlock();
    mrn_dbg_func_end();
    return 0;
}

bool Network::reset_Topology( string& itopology )
{
    if( ! _network_topology->reset(itopology) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "Topology->reset() failed\n" ));
        return false;
    }

    return true;
}

bool Network::add_SubGraph( Rank iroot_rank, SerialGraph& sg, bool iupdate ) 
{
    if( ! _network_topology->add_SubGraph(iroot_rank, sg, iupdate) ) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "add_SubGraph() failed\n"));
        return false;
    }

    return update_Streams();
}

bool Network::remove_Node( Rank ifailed_rank, bool iupdate )
{
    mrn_dbg_func_begin();

    mrn_dbg( 5, mrn_printf(FLF, stderr, 
                           "Failed rank is %d\n", ifailed_rank) );
    delete_PeerNode( ifailed_rank );
    _network_topology->remove_Node( ifailed_rank, iupdate );

    _streams_sync.Lock();
    map < unsigned int, Stream* >::iterator iter;
    for( iter=_streams.begin(); iter != _streams.end(); iter++ ) {
        (*iter).second->remove_Node( ifailed_rank );
    }
    _streams_sync.Unlock();

    bool rc = true;
    if( iupdate ) {
        if( ! update_Streams() )
            rc = false;
    }

    mrn_dbg_func_end();
    return rc;
}

bool Network::change_Parent( Rank ichild_rank, Rank inew_parent_rank )
{
    // update topology
    if( ! _network_topology->set_Parent(ichild_rank, inew_parent_rank, true) ) {
        return false;
    }

    // update streams if I'm the new parent
    if( inew_parent_rank == _local_rank )
        update_Streams();

    return true;
}

bool Network::insert_EndPoint( string& ihostname, Port iport, Rank irank )
{
    bool retval = true;
    CommunicationNode* cn = NULL;
    
    mrn_dbg_func_begin();

    _endpoints_mutex.Lock();

    map< Rank, CommunicationNode* >::iterator iter = _end_points.find( irank );
    if( iter != _end_points.end() ) {
        if( (iter->second->get_HostName() != ihostname) ||
            (iter->second->get_Port() != iport) ) {
            mrn_dbg( 5, mrn_printf(FLF, stderr, "Replacing Node[%d] [%s:%d] with [%s:%d]\n",
                                   iter->second->get_HostName().c_str(), iter->second->get_Port(),
                                   ihostname.c_str(), iport) );
            cn = iter->second;
            _end_points.erase( iter );
            if( is_LocalNodeFrontEnd() )
                _bcast_communicator->remove_EndPoint( irank );
            delete cn;
    
            cn = new_EndPoint(ihostname, iport, irank);
            _end_points[ irank ] = cn;
        }
        else
            retval = false;
    }
    else {
        cn = new_EndPoint(ihostname, iport, irank);
        _end_points[ irank ] = cn;
    }

    if( is_LocalNodeFrontEnd() && (cn != NULL) )
        _bcast_communicator->add_EndPoint( cn );
        
    _endpoints_mutex.Unlock();
    
    mrn_dbg_func_end();
    return retval;
}

bool Network::remove_EndPoint( Rank irank )
{
    _endpoints_mutex.Lock();
    _end_points.erase( irank );
    if( is_LocalNodeFrontEnd() )
        _bcast_communicator->remove_EndPoint( irank );
    _endpoints_mutex.Unlock();
    
    return true;
}

void Network::clear_EndPoints()
{
    if( is_LocalNodeFrontEnd() ) {
        _endpoints_mutex.Lock();
        map< Rank, CommunicationNode* >::iterator iter = _end_points.begin();
        for( ; iter != _end_points.end(); iter++ )
            delete iter->second;
        _end_points.clear();
        _endpoints_mutex.Unlock();
    }
}

bool Network::update_Streams(void)
{
    map< unsigned int, Stream* >::iterator iter;

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Updating stream children ...\n") );

    _streams_sync.Lock();
    for( iter=_streams.begin(); iter != _streams.end(); iter++ ) {
        (*iter).second->recompute_ChildrenNodes();
    }
    _streams_sync.Unlock();

    return true;
}

void Network::close_PeerNodeConnections(void)
{
    mrn_dbg_func_begin();

    set< PeerNodePtr >::const_iterator iter;
 
    if( is_LocalNodeParent() ) {
        _children_mutex.Lock();
        for( iter = _children.begin(); iter != _children.end(); iter++ ) {
            PeerNodePtr cur_node = *iter;
            cur_node->close_Sockets();
        }
        _children_mutex.Unlock();
    }

    if( is_LocalNodeChild() ) {
        _parent_sync.Lock();
        _parent->close_Sockets();
        _parent_sync.Unlock();
    }

    mrn_dbg_func_end();
}

char* Network::get_TopologyStringPtr(void) const
{
    return strdup( _network_topology->get_TopologyString().c_str() );
}

char* Network::get_LocalSubTreeStringPtr(void) const
{
    std::string subtree( _network_topology->get_LocalSubTreeString() );
    if( subtree.empty() )
        return NULL;
    else
        return strdup( subtree.c_str() );
}

void Network::enable_FailureRecovery(void)
{
    _recover_from_failures = true;
}

void Network::disable_FailureRecovery(void)
{
    _recover_from_failures = false;
}

bool Network::recover_FromFailures(void) const
{
    return _recover_from_failures;
}

PeerNodePtr Network::get_ParentNode(void) const
{
    _parent_sync.Lock();

    while( _parent == PeerNode::NullPeerNode ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "Wait for parent node available\n"));
        _parent_sync.WaitOnCondition( PARENT_NODE_AVAILABLE );
    }

    _parent_sync.Unlock();
    return _parent;
}

void Network::set_ParentNode( PeerNodePtr ip )
{
    _parent_sync.Lock();

    _parent = ip;
    if( _parent != PeerNode::NullPeerNode ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr, "Signal parent node available\n"));
        _parent_sync.SignalCondition( PARENT_NODE_AVAILABLE );
    }

    _parent_sync.Unlock();
}

PeerNodePtr Network::new_PeerNode( string const& ihostname, Port iport,
                                   Rank irank, bool iis_parent,
                                   bool iis_internal )
{
    PeerNodePtr node( new PeerNode(this, ihostname, iport, irank,
                                   iis_parent, iis_internal) );

    mrn_dbg( 5, mrn_printf(FLF, stderr, "new peer node: %s:%d (%p) \n",
                           node->get_HostName().c_str(), node->get_Rank(),
                           node.get() )); 

    if( iis_parent ) {
        set_ParentNode( node );
    }
    else {
        _children_mutex.Lock();
        mrn_dbg( 5, mrn_printf(FLF, stderr, "inserted into children\n") );
        _children.insert( node );
        _children_mutex.Unlock();
    }

    return node;
}

bool Network::delete_PeerNode( Rank irank )
{
    set< PeerNodePtr >::const_iterator iter;

    if( is_LocalNodeChild() ) {
        _parent_sync.Lock();
        if( _parent != PeerNode::NullPeerNode ) { 
            if( _parent->get_Rank() == irank ) {
                _parent->mark_Failed();
                _parent = PeerNode::NullPeerNode;
                _parent_sync.Unlock();
                return true;
            }
        }
        _parent_sync.Unlock();
    }

    _children_mutex.Lock();
    for( iter = _children.begin(); iter != _children.end(); iter++ ) {
        if( (*iter)->get_Rank() == irank ) {
            (*iter)->mark_Failed();
            _children.erase( iter );
            _children_mutex.Unlock();
            mrn_dbg( 5, mrn_printf(FLF, stderr, "deleted %d from children\n", irank) );
            return true;
        }
    }
    _children_mutex.Unlock();

    return false;
}

PeerNodePtr Network::get_PeerNode( Rank irank )
{
    PeerNodePtr peer = PeerNode::NullPeerNode;

    set< PeerNodePtr >::const_iterator iter;

    if( is_LocalNodeChild() ) {
        _parent_sync.Lock();
        if( _parent->get_Rank() == irank ) {
            peer = _parent;
        }
        _parent_sync.Unlock();
    }

    if( peer == PeerNode::NullPeerNode ) {
        _children_mutex.Lock();
    if( _children.empty() )
        mrn_dbg( 5, mrn_printf(FLF, stderr, "children is empty\n") );
        for( iter = _children.begin(); iter != _children.end(); iter++ ) {
            mrn_dbg( 5, mrn_printf(FLF, stderr, "rank is %d, irank is %d\n", 
                                   (*iter)->get_Rank(), irank) ); 
            if( (*iter)->get_Rank() == irank ) {
                peer = *iter;
                break;
            }
        }
        _children_mutex.Unlock();
    }

    return peer;
}

void Network::get_ChildPeers( set< PeerNodePtr >& peers ) const
{
    _children_mutex.Lock();
    peers = _children;
    _children_mutex.Unlock();
}


unsigned int Network::get_NumChildren(void) const
{
    _children_mutex.Lock();
    unsigned int size = (unsigned int) _children.size();
    _children_mutex.Unlock();
    return size;
}

PeerNodePtr Network::get_OutletNode( Rank irank ) const
{
    return _network_topology->get_OutletNode( irank );
}

bool Network::has_PacketsFromParent(void)
{
    assert( is_LocalNodeChild() );
    return get_ParentNode()->has_data();
}

int Network::recv_PacketsFromParent( std::list <PacketPtr> & opackets ) const
{
    assert( is_LocalNodeChild() );
    return get_ParentNode()->recv( opackets );
}

bool Network::node_Failed( Rank irank  )
{
    return _network_topology->node_Failed( irank );
}

TimeKeeper* Network::get_TimeKeeper(void) 
{ 
    return _local_time_keeper;
}

void set_OutputLevel(int l)
{
    if( l <= MIN_OUTPUT_LEVEL ) {
        CUR_OUTPUT_LEVEL = MIN_OUTPUT_LEVEL;
    }
    else if( l >= MAX_OUTPUT_LEVEL ) {
        CUR_OUTPUT_LEVEL = MAX_OUTPUT_LEVEL;
    }
    else {
        CUR_OUTPUT_LEVEL = l;
    }
    XPlat::set_DebugLevel(CUR_OUTPUT_LEVEL);
}

/* Failure Recovery */
bool Network::set_FailureRecovery( bool enable_recovery )
{
    int tag;

    mrn_dbg_func_begin();

    if( is_LocalNodeFrontEnd() ) {

        if( enable_recovery ) {
            tag = PROT_ENABLE_RECOVERY;
            enable_FailureRecovery();
        }
        else {
            tag = PROT_DISABLE_RECOVERY;
            disable_FailureRecovery();
        }
    
        PacketPtr packet( new Packet(CTL_STRM_ID, tag, "%d", (int)enable_recovery) );
        if( packet->has_Error() ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "new packet() fail\n") );
            return false;
        }

        if( send_PacketToChildren( packet ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n") );
            return false;
        }
    }
    else {
        mrn_dbg( 1, mrn_printf(FLF, stderr, 
                               "Network::set_FailureRecovery() can only be used by front-end\n") ); 
        return false;
    }

    mrn_dbg_func_end();
    return true;
}

// network performance section starts

PacketPtr Network::collect_PerfData( perfdata_metric_t metric,
                                     perfdata_context_t context,
                                     int aggr_strm_id )
{
    vector< perfdata_t > data;
    vector< perfdata_t >::iterator iter;

    //PerfDataMgr * _perf_data = boost::any_cast<PerfDataMgr *>(_perf_data_store); 
    if (metric == PERFDATA_MET_MEM_VIRT_KB || metric == PERFDATA_MET_MEM_PHYS_KB) {
        _perf_data->get_MemData(metric);
    }
    _perf_data->collect( metric, context, data );
    iter = data.begin();
    Rank my_rank = get_LocalRank();
    uint32_t num_elems = (uint32_t)data.size();
    void* data_arr = NULL;
    const char* fmt = NULL;
    switch( PerfDataMgr::get_MetricType(metric) ) {
     case PERFDATA_TYPE_UINT: {
         fmt = "%ad %ad %auld";
         if( num_elems ) {
             uint64_t* u64_arr = (uint64_t*)malloc(sizeof(uint64_t) * num_elems);
             if( u64_arr == NULL ) {
                 mrn_dbg(1, mrn_printf(FLF, stderr, "malloc() data array failed\n"));
                 return Packet::NullPacket;
             }
             for( unsigned u=0; iter != data.end(); u++, iter++ )
                 u64_arr[u] = (*iter).u;
             data_arr = u64_arr;
         }
         break;
     }
     case PERFDATA_TYPE_INT: {
         fmt = "%ad %ad %ald";
         if( num_elems ) {
             int64_t* i64_arr = (int64_t*)malloc(sizeof(int64_t) * num_elems);
             if( i64_arr == NULL ) {
                 mrn_dbg(1, mrn_printf(FLF, stderr, "malloc() data array failed\n"));
                 return Packet::NullPacket;
             }
             for( unsigned u=0; iter != data.end(); u++, iter++ )
                 i64_arr[u] = (*iter).i;
             data_arr = i64_arr;
         }
         break;
     }
     case PERFDATA_TYPE_FLOAT: {
         fmt = "%ad %ad %alf";
         if( num_elems ) {
             double* dbl_arr = (double*)malloc(sizeof(double) * num_elems);
             if( dbl_arr == NULL ) {
                 mrn_dbg(1, mrn_printf(FLF, stderr, "malloc() data array failed\n"));
                 return Packet::NullPacket;
             }
             for( unsigned u=0; iter != data.end(); u++, iter++)
                 dbl_arr[u] = (*iter).d;
             data_arr = dbl_arr;
         }
         break;
     }
     default:
         mrn_dbg(1, mrn_printf(FLF, stderr, "bad metric type\n"));
         return Packet::NullPacket;
    }
    // create output packet
    int* rank_arr = (int*) malloc( sizeof(int) );
    int* nelems_arr = (int*) malloc( sizeof(int) );
    *rank_arr = my_rank;
    *nelems_arr = num_elems;
    PacketPtr packet( new Packet( aggr_strm_id, PROT_COLLECT_PERFDATA, fmt,
                                  rank_arr, 1, nelems_arr, 1, data_arr, num_elems ) );
    if( packet->has_Error() ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "new packet() fail\n"));
        return Packet::NullPacket;
    }
    packet->set_DestroyData(true); // free arrays when Packet no longer in use
    return packet;
}


bool Network::collect_NetPerformanceData ( rank_perfdata_map& results,
                                           perfdata_metric_t metric,
                                           perfdata_context_t context,
                                           int aggr_filter_id /*=TFILTER_ARRAY_CONCAT*/)
{
    //PerfDataMgr * _perf_data = boost::any_cast<PerfDataMgr *>(_perf_data_store); 
    mrn_dbg_func_begin();
    if( metric >= PERFDATA_MAX_MET )
        return false;
    if( context >= PERFDATA_MAX_CTX )
        return false;

    // create stream to aggregate performance data
    //Communicator* comm = new Communicator( this,_end_points);
    Stream* aggr_strm = this->new_Stream( _bcast_communicator, TFILTER_PERFDATA );
    aggr_strm->set_FilterParameters( FILTER_UPSTREAM_TRANS, "%d %d %ud %ud",
                                     (int)metric, (int)context,
                                     aggr_filter_id,0);
    int aggr_strm_id = aggr_strm->get_Id();

    if( this->is_LocalNodeFrontEnd() ) {

        // send collect request
        int tag = PROT_COLLECT_PERFDATA;
        PacketPtr packet( new Packet( aggr_strm_id, tag, "%d %d %ud",
                    (int)metric, (int)context, aggr_strm_id ) );
        if( packet->has_Error() ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "new packet() fail\n"));
            return false;
        }

        if( this->is_LocalNodeParent() ) {
            if( this->send_PacketToChildren( packet ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n" ));
                return false;
            }
        }

        // receive data
        int rtag;
        PacketPtr resp_packet;
        aggr_strm->recv( &rtag, resp_packet );

        // unpack data
        int* rank_arr;
        int* nelems_arr;
        uint64_t rank_len, nelems_len;
	int data_len;
        void* data_arr;
        const char* fmt = NULL;
        perfdata_mettype_t mettype = PerfDataMgr::get_MetricType(metric);
        switch( mettype ) {
         case PERFDATA_TYPE_UINT: {
             fmt = "%ad %ad %auld";
             uint64_t* u64_arr;
         resp_packet->unpack( fmt, &rank_arr, &rank_len,
                                  &nelems_arr, &nelems_len,
                                  &u64_arr, &data_len );
             data_arr = u64_arr;
             break;
         }
         case PERFDATA_TYPE_INT: {
             fmt = "%ad %ad %ald";
             int64_t* i64_arr;
         resp_packet->unpack( fmt, &rank_arr, &rank_len,
                                  &nelems_arr, &nelems_len,
                                  &i64_arr, &data_len );
             data_arr = i64_arr;
             break;
         }
         case PERFDATA_TYPE_FLOAT: {
             fmt = "%ad %ad %alf";
             double* dbl_arr;
         resp_packet->unpack( fmt, &rank_arr, &rank_len,
                                  &nelems_arr, &nelems_len,
                                  &dbl_arr, &data_len );
             data_arr = dbl_arr;
             break;
         }
         default:
             mrn_dbg(1, mrn_printf(FLF, stderr, "bad metric type\n"));
             return false; 
        }
        // add data to result map
        unsigned data_ndx = 0;
        for( unsigned u=0; u < rank_len ; u++ ) {
            unsigned nelems = nelems_arr[u];
            int rank = rank_arr[u];
            vector<perfdata_t>& rank_data = results[ rank ];
            rank_data.clear();
            rank_data.reserve( nelems );
            perfdata_t datum;
            switch( mettype ) {
             case PERFDATA_TYPE_UINT: {
                 uint64_t* u64_arr = (uint64_t*)data_arr + data_ndx;
                 for( unsigned v=0; v < nelems; v++ ) {
                     datum.u = u64_arr[v];
                     rank_data.push_back( datum );
                 }
                 break;
             }
             case PERFDATA_TYPE_INT: {
                 int64_t* i64_arr = (int64_t*)data_arr +  data_ndx;
                 for( unsigned v=0; v < nelems; v++ ) {
                     datum.i = i64_arr[v];
                     rank_data.push_back( datum );
                 }
                 break;
             }
             case PERFDATA_TYPE_FLOAT: {
                 double* dbl_arr = (double*)data_arr + data_ndx;
                 for( unsigned v=0; v < nelems; v++ ) {
                     datum.d = dbl_arr[v];
                     rank_data.push_back( datum );
                 }
                 break;
             }
            }
            data_ndx += nelems;
        }
        if( rank_arr != NULL ) free( rank_arr );
        if( nelems_arr != NULL ) free( nelems_arr );
        if( data_arr != NULL ) free( data_arr );
    }
    else {
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                 "collect_PerformanceData() can only be used by Network front-end\n" ));
        return false;
    }
    mrn_dbg_func_end();
    return true;
}

FilterInfoPtr Network::GetFilterInfo() {
    return _net_filters;
}

#ifdef LIBI_HEADER_INCLUDE
void Network::get_Identity( SerialGraph* sg,
                    int & n,
                    const char * myhostname,
                    Rank & myrank,
                    int & mynumchildren,
                    char* & phostname,
                    Rank & prank,
                    bool includeNonLeaves,
                    bool includeLeaves,
                    bool useNetworkHostName,
                    bool isRoot ){

    Rank r = sg->get_RootRank();
    string hn = sg->get_RootHostName();
    if( useNetworkHostName )
        XPlat::NetUtils::GetNetworkName( hn, hn );
    sg->set_ToFirstChild();
    SerialGraph* child = sg->get_NextChild();
    bool isLeaf = ( child == NULL );
    if( !isRoot ){
        if( (isLeaf && includeLeaves) || (!isLeaf && includeNonLeaves) ){
            if( hn.compare( myhostname ) == 0 ){
                n--;
                if( n == 0 ){
                    myrank = r;
                    mynumchildren=0;
                    for( ; child != NULL; child = sg->get_NextChild() )
                        mynumchildren++;
                }
            }
        }
    }
    if( n > 0 ){
        for( ; (n > 0) && (child != NULL); child = sg->get_NextChild() )
            get_Identity( child, n, myhostname, myrank, mynumchildren, phostname, prank, includeNonLeaves, includeLeaves, useNetworkHostName, false );
        if( n == 0 && phostname == NULL){
            phostname = strdup( hn.c_str() );
            prank = r;
        }
    }
}

void Network::CreateHostDistributions( NetworkTopology::Node* node,
                             bool isRoot,
                             bool MWincludesLeaves,
                             host_dist_t** & mw,
                             host_dist_t** & be)
{
    const std::set<NetworkTopology::Node*> children = node->get_Children();
    bool isNotLeaf = ( children.size() > 0 );

    if( !isRoot ){
        if( isNotLeaf || MWincludesLeaves ){
            (*mw) = (host_dist_t*)malloc( sizeof( host_dist_t ) );
            (*mw)->hostname = strdup( node->get_HostName().c_str() );
            (*mw)->nproc = 1;
            (*mw)->next = NULL;
            mw = &( (*mw)->next );
        } else {
            (*be) = (host_dist_t*)malloc( sizeof( host_dist_t ) );
            (*be)->hostname = strdup( node->get_HostName().c_str() );
            (*be)->nproc = 1;
            (*be)->next = NULL;
            be = &( (*be)->next );
        }
    }
    std::set<NetworkTopology::Node*>::iterator iter = children.begin();
    while(iter != children.end()){
        CreateHostDistributions( (*iter), false, MWincludesLeaves, mw, be );
        iter++;
    }
}
#endif

std::string Network::get_NetSettingName( int s )
{
     std::string ret;
     if( s == MRNET_DEBUG_LEVEL )
         ret = "MRNET_DEBUG_LEVEL";
     else if( s == MRNET_DEBUG_LOG_DIRECTORY )
         ret = "MRNET_DEBUG_LOG_DIRECTORY";
     else if( s == MRNET_COMMNODE_PATH )
         ret = "MRNET_COMMNODE_PATH";
     else if( s == MRNET_FAILURE_RECOVERY )
         ret = "MRNET_FAILURE_RECOVERY";
     return ret;
}

/**
 * GetParametersIN - Processes parameters sent to internal nodes on Cray machines (XT/CTI)
 **/
int Network::GetParametersIN( int argc, char * argv[], int & port, int & timeout, int & topoPipeFd) {
    const char* topofd_optstr = "--topofd";
    const char* port_optstr = "--listen-port";
    const char* timeout_optstr = "--listen-timeout";

    int start = 0;
    while (start < argc) {
        if( strcmp(argv[start], topofd_optstr) == 0){
            // Topology Pipe Parameter
            topoPipeFd = (int)strtol(argv[start+1],  NULL, 10);
            start += 2;
        } else if (strcmp(argv[start], port_optstr) == 0 ) {
            // Port Parameter
            port = atoi( argv[start+1] );
            start += 2;
        } else if (strcmp(argv[start], timeout_optstr) == 0 ) {
            // Timeout Parameter
            timeout = atoi( argv[start+1] );
            start += 2;
        } else {
            // Unknown Parameter, stop parsing.
            break;
            // start++;
        }
    }
    return start;
}


}  // namespace MRN
