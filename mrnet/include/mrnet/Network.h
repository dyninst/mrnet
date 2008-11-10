/****************************************************************************
 * Copyright © 2003-2008 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ***************************************************************************/

#if !defined(__network_h)
#define __network_h 1

#include <list>
#include <map>
#include <string>
#include <set>

#include "mrnet/Communicator.h"
#include "mrnet/Error.h"
#include "mrnet/Event.h"
#include "mrnet/FilterIds.h"
#include "mrnet/Packet.h"
#include "mrnet/Stream.h"
#include "mrnet/Tree.h"
#include "mrnet/Types.h"
#include "xplat/Monitor.h"

#include <boost/shared_ptr.hpp>

namespace MRN
{

class FrontEndNode;
class BackEndNode;
class InternalNode;
class ChildNode;
class ParentNode;
class Router;
class NetworkTopology;
class SerialGraph;

class PeerNode;
typedef boost::shared_ptr<PeerNode> PeerNodePtr; 

class Network: public Error {
 public:
    // FE constructor
    Network( const char * itopology,
             const char * ibackend_exe,
             const char **ibackend_argv,
             bool irank_backends=true,
             bool iusing_mem_buf=false );

    // BE constructors
    Network( int argc, char **argv );

    Network( const char *iphostname, Port ipport, Rank iprank,
             const char *imyhostname, Rank imyrank );

    ~Network( );
    void shutdown_Network( void );
    void set_TerminateBackEndsOnShutdown( bool terminate ); 

    CommunicationNode * get_EndPoint( Rank ) const;

    Communicator * get_BroadcastCommunicator( void ) const;
    Communicator * new_Communicator( void );
    Communicator * new_Communicator( Communicator& );
    Communicator * new_Communicator( std::set <CommunicationNode *> & );

    int load_FilterFunc( const char * so_file, const char * func );

    Stream * new_Stream( Communicator *,
                         int us_filter_id=TFILTER_NULL,
                         int sync_id=SFILTER_WAITFORALL,
                         int ds_filter_id=TFILTER_NULL );
    Stream * get_Stream( unsigned int iid ) const;

    int recv( int *otag, PacketPtr &opacket, Stream ** ostream, bool iblocking=true );

    bool enable_PerformanceData( perfdata_metric_t metric, perfdata_context_t context );
    bool disable_PerformanceData( perfdata_metric_t metric, perfdata_context_t context );
    bool collect_PerformanceData( std::map< int, rank_perfdata_map >& results,
                                  perfdata_metric_t metric, 
                                  perfdata_context_t context,
                                  int aggr_filter_id = TFILTER_ARRAY_CONCAT );
    void print_PerformanceData( perfdata_metric_t metric, perfdata_context_t context );

    int get_DataSocketFds( int **oarray, unsigned int *oarray_size ) const;
    int get_EventNotificationFd( EventType etyp );
    void clear_EventNotificationFd( EventType etyp );
    void close_EventNotificationFd( EventType etyp );

    void set_BlockingTimeOut( int timeout );
    int get_BlockingTimeOut( void );

    NetworkTopology * get_NetworkTopology( void ) const;

    void print_error( const char * );
    bool node_Failed( Rank );

    bool is_LocalNodeChild( void ) const ;
    bool is_LocalNodeParent( void ) const ;
    bool is_LocalNodeInternal( void ) const ;
    bool is_LocalNodeFrontEnd( void ) const ;
    bool is_LocalNodeBackEnd( void ) const ;

    //NOT IN PUBLIC API
    const std::set < PeerNodePtr > get_ChildPeers() const;
    Network( void ); // internal node constructor

 private:
    friend class Stream;
    friend class FrontEndNode;
    friend class BackEndNode;
    friend class InternalNode;
    friend class ChildNode;
    friend class ParentNode;
    friend class NetworkTopology;
    friend class Router;
    friend class PeerNode;
    friend class EventDetector;

    void update_BcastCommunicator( void );

    int parse_Configuration( const char* itopology, bool iusing_mem_buf );

    void init_BackEnd( const char *iphostname, Port ipport, Rank iprank,
                       const char *imyhostname, Rank imyrank );

    Stream * new_Stream( int iid,
                         Rank *ibackends,
                         unsigned int inum_backends,
                         int ius_filter_id,
                         int isync_filter_id,
                         int ids_filter_id);
    void delete_Stream( unsigned int );
    bool have_Streams( void );
    void waitfor_NonEmptyStream( void );
    void signal_NonEmptyStream( void );
    enum {STREAMS_NONEMPTY};

    enum {PARENT_NODE_AVAILABLE};

    int send_PacketsToParent( std::vector <PacketPtr> & );
    int send_PacketToParent( PacketPtr );
    int send_PacketsToChildren( std::vector <PacketPtr> & );
    int send_PacketToChildren( PacketPtr, bool internal_only = false );
    int recv( bool iblocking=true );
    int recv_PacketsFromParent( std::list <PacketPtr> & ) const ;
    bool has_PacketsFromParent( void );
    int flush_PacketsToParent( void );
    int flush_PacketsToChildren( void ) const;

    CommunicationNode * new_EndPoint(std::string &hostname, Port port, Rank rank );

    void set_LocalHostName( std::string & );
    void set_LocalPort( Port );
    void set_LocalRank( Rank );

    PeerNodePtr get_ParentNode( void ) const;
    void set_ParentNode( PeerNodePtr ip );
    void set_BackEndNode( BackEndNode * );
    void set_FrontEndNode( FrontEndNode * );
    void set_InternalNode( InternalNode * );
    void set_NetworkTopology( NetworkTopology * );
    void set_Router( Router * );
    void set_FailureManager( CommunicationNode * );

    PeerNodePtr get_PeerNode( Rank );
    void cancel_IOThreads( void );

    PeerNodePtr get_OutletNode( Rank ) const ;
    char * get_LocalSubTreeStringPtr( void ) const ;
    char * get_TopologyStringPtr( void ) const ;
    FrontEndNode * get_LocalFrontEndNode( void ) const ;
    InternalNode * get_LocalInternalNode( void ) const ;
    BackEndNode * get_LocalBackEndNode( void ) const ;
    ChildNode * get_LocalChildNode( void ) const ;
    ParentNode * get_LocalParentNode( void ) const ;
    std::string get_LocalHostName( void ) const ;
    Port get_LocalPort( void ) const ;
    Rank get_LocalRank( void ) const ;
    int get_ListeningSocket( void ) const ;
    CommunicationNode * get_FailureManager( void ) const ;
    bool is_LocalNodeThreaded( void ) const ;
    int send_FilterStatesToParent( void );
    bool update( void );
    bool reset_Topology( std::string & itopology );
    bool add_SubGraph( Rank iroot_rank, SerialGraph & sg  );
    bool remove_Node( Rank ifailed_rank, bool iupdate=true );
    bool change_Parent( Rank ichild_rank, Rank inew_parent_rank );

    bool delete_PeerNode( Rank );
    PeerNodePtr new_PeerNode( std::string const& ihostname, Port iport,
                              Rank irank, bool iis_upstream,
                              bool iis_internal );
    void close_PeerNodeConnections( void );
    void disable_FailureRecovery( void );
    void enable_FailureRecovery( void );
    bool recover_FromFailures( void ) const ;

    void collect_PerfData( void );

    //Data Members
    std::string _local_hostname;
    Port _local_port;
    Rank _local_rank;
    NetworkTopology *_network_topology;
    CommunicationNode *_failure_manager;
    Communicator *_bcast_communicator;
    FrontEndNode *_local_front_end_node;
    BackEndNode *_local_back_end_node;
    InternalNode *_local_internal_node;

    PeerNodePtr _parent;
    std::set < PeerNodePtr > _children;
    std::map < unsigned int, Stream * > _streams;
    std::map< Rank, CommunicationNode * > _end_points;
    std::map < unsigned int, Stream * >::iterator _stream_iter;

    bool _threaded;
    bool _recover_from_failures;
    bool _terminate_backends;

    /* EventPipe notifications */
    std::map< EventType, EventPipe* > _evt_pipes;

    mutable XPlat::Monitor _parent_sync;
    mutable XPlat::Monitor _streams_sync;
    mutable XPlat::Mutex _children_mutex;
};

extern Network * network;

} /* MRN namespace */
#endif /* __network_h */