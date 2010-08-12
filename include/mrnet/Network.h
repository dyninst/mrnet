/****************************************************************************
 * Copyright © 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

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
class ParsedGraph;
class TimeKeeper;
class EventDetector;

class PeerNode;
typedef boost::shared_ptr< PeerNode > PeerNodePtr; 

class Network: public Error {

 public:

    // BEGIN MRNET API

    static Network* CreateNetworkFE( const char* itopology,
                                     const char* ibackend_exe,
                                     const char** ibackend_argv,
                                     const std::map< std::string, std::string >* attrs=NULL,
                                     bool irank_backends=true,
                                     bool iusing_mem_buf=false );
    static Network* CreateNetworkBE( int argc, char* argv[] );

    NetworkTopology* get_NetworkTopology( void ) const;
    
    bool is_ShutDown( void ) const;
    void waitfor_ShutDown( void ) const;

    void print_error( const char * );

    /* Local node information */
    std::string get_LocalHostName( void ) const ;
    Port get_LocalPort( void ) const ;
    Rank get_LocalRank( void ) const ;
    bool is_LocalNodeChild( void ) const ;
    bool is_LocalNodeParent( void ) const ;
    bool is_LocalNodeInternal( void ) const ;
    bool is_LocalNodeFrontEnd( void ) const ;
    bool is_LocalNodeBackEnd( void ) const ;

    /* Communicators */
    Communicator* get_BroadcastCommunicator( void ) const;
    Communicator* new_Communicator( void );
    Communicator* new_Communicator( Communicator& );
    Communicator* new_Communicator( const std::set< Rank > & );
    Communicator* new_Communicator( std::set< CommunicationNode* > & );
    CommunicationNode* get_EndPoint( Rank ) const;

    /* Streams */
    int load_FilterFunc( const char * so_file, const char * func );
    Stream* new_Stream( Communicator*,
                        int us_filter_id=TFILTER_NULL,
                        int sync_id=SFILTER_WAITFORALL,
                        int ds_filter_id=TFILTER_NULL );
    Stream* new_Stream( Communicator* icomm,
                        std::string us_filters,
                        std::string sync_filters,
                        std::string ds_filters );
    Stream* get_Stream( unsigned int iid ) const;
    int recv( int* otag, PacketPtr& opacket, Stream** ostream, bool iblocking=true );

    /* Performance data collection */
    bool enable_PerformanceData( perfdata_metric_t metric, perfdata_context_t context );
    bool disable_PerformanceData( perfdata_metric_t metric, perfdata_context_t context );
    bool collect_PerformanceData( std::map< int, rank_perfdata_map >& results,
                                  perfdata_metric_t metric, 
                                  perfdata_context_t context,
                                  int aggr_filter_id = TFILTER_ARRAY_CONCAT );
    void print_PerformanceData( perfdata_metric_t metric, perfdata_context_t context );

    /* Event notification */
    int get_EventNotificationFd( EventType etyp );
    void clear_EventNotificationFd( EventType etyp );
    void close_EventNotificationFd( EventType etyp );
    
    /* Callback-based event notification */
    bool register_Callback( CBClass icbcl, cb_func func, CBType icbt=ALL_EVENT );
    bool remove_Callback( CBClass icbcl, CBType icbt=ALL_EVENT );
    bool remove_Callback( CBClass icbcl, cb_func func, CBType icbt=ALL_EVENT );

    /* Turn Fault Recovery ON or OFF*/
    bool set_FailureRecovery( bool enable_recovery );

    /* NOTE: DEPRECATED in 3.0 */
    void set_TerminateBackEndsOnShutdown( bool terminate ); 

    // END MRNET API

    virtual ~Network( );

    /* internal node stuff */
    static Network* CreateNetworkIN( int argc, char* argv[] );    // create obj for internal node
    InternalNode* get_LocalInternalNode( void ) const;

    TimeKeeper* get_TimeKeeper( void );

    const std::set< PeerNodePtr > get_ChildPeers() const;
    PeerNodePtr get_PeerNode( Rank );
    bool node_Failed( Rank );
    PeerNodePtr get_OutletNode( Rank ) const ;

    void add_Callbacks();

protected:
    // constructor
    Network( void );

    // Initializers for separate Network roles
    // With the current design where a single Network class can be
    // used in three distinct roles (FE, BE, and internal node), we
    // need to searate the initialization method from the constructor
    // so that the initialization method can call virtual methods
    
    // initialize as FE role
    void init_FrontEnd( const char*  itopology,
                        const char*  ibackend_exe,
                        const char** ibackend_argv,
                        const std::map< std::string, std::string >* iattrs,
                        bool irank_backends=true,
                        bool iusing_mem_buf=false );
    virtual FrontEndNode* CreateFrontEndNode( Network* inetwork,
                                              std::string ihostname,
                                              Rank irank ) = 0;
    FrontEndNode* get_LocalFrontEndNode( void ) const ;
    virtual void Instantiate( ParsedGraph* parsed_graph,
                              const char* mrn_commnode_path,
                              const char* ibackend_exe,
                              const char** ibackend_args,
                              unsigned int backend_argc,
                              const std::map< std::string, std::string >* iattrs) = 0;


    // initialize as BE role
    void init_BackEnd( const char* iphostname, Port ipport, Rank iprank,
                       const char* imyhostname, Rank imyrank );
    virtual BackEndNode* CreateBackEndNode( Network* inetwork, 
                                            std::string imy_hostname, 
                                            Rank imy_rank,
                                            std::string iphostname, 
                                            Port ipport, 
                                            Rank iprank ) = 0;

    // initialize as IN role
    void init_InternalNode( const char* iphostname,
                            Port ipport,
                            Rank iprank,
                            const char* imyhostname,
                            Rank imyrank,
                            int idataSocket = -1,
                            Port idataPort = UnknownPort );
    virtual InternalNode* CreateInternalNode( Network* inetwork, 
                                              std::string imy_hostname,
                                              Rank imy_rank,
                                              std::string iphostname,
                                              Port ipport, 
                                              Rank iprank,
                                              int idataSocket = -1,
                                              Port idataPort = UnknownPort ) = 0;


    void set_LocalHostName( std::string const& );
    static const char* FindCommnodePath( void );
    
    void shutdown_Network( void );
    bool reset_Topology(std::string& itopology);
    void update_TopoStream();
    PeerNodePtr _parent;

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
    friend class RSHParentNode;
    friend class RSHChildNode;
    
    //NEW_TOPO propagation code
    //The topology is propagated from parent to child when child connects to parent not when child first
    //gets the topology
    SerialGraph* readTopology( int topoFd);
    void writeTopology( int topoFd,
                        SerialGraph* topology );

    // some conditions we waitfor/signal
    enum {
        STREAMS_NONEMPTY,
        PARENT_NODE_AVAILABLE,
        NETWORK_TERMINATION
    };

    void update_BcastCommunicator( void );

    int parse_Configuration( const char* itopology, bool iusing_mem_buf );


    Stream * new_Stream( int iid,
                         Rank* ibackends,
                         unsigned int inum_backends,
                         int ius_filter_id,
                         int isync_filter_id,
                         int ids_filter_id);
    void delete_Stream( unsigned int );
    bool have_Streams( void );
    void waitfor_NonEmptyStream( void );
    void signal_NonEmptyStream( void );

    int send_PacketsToParent( std::vector< PacketPtr >& );
    int send_PacketToParent( PacketPtr );
    int send_PacketsToChildren( std::vector< PacketPtr >& );
    int send_PacketToChildren( PacketPtr, bool internal_only = false );
    int recv( bool iblocking=true );
    int recv_PacketsFromParent( std::list< PacketPtr >& ) const ;
    bool has_PacketsFromParent( void );
    int flush_PacketsToParent( void );
    int flush_PacketsToChildren( void ) const;

    CommunicationNode * new_EndPoint( std::string& hostname, Port port, Rank rank );

    void set_LocalPort( Port );
    void set_LocalRank( Rank );

    PeerNodePtr get_ParentNode( void ) const;
    void set_ParentNode( PeerNodePtr ip );
    void set_BackEndNode( BackEndNode* );
    void set_FrontEndNode( FrontEndNode* );
    void set_InternalNode( InternalNode* );
    void set_NetworkTopology( NetworkTopology* );
    void set_Router( Router* );
    void set_FailureManager( CommunicationNode* );

    void cancel_IOThreads( void );
    void signal_ShutDown( void );

    //PeerNodePtr get_OutletNode( Rank ) const ;
    char* get_LocalSubTreeStringPtr( void ) const ;
    char* get_TopologyStringPtr( void ) const ;
    BackEndNode* get_LocalBackEndNode( void ) const ;
    ChildNode* get_LocalChildNode( void ) const ;
    ParentNode* get_LocalParentNode( void ) const ;
    int get_ListeningSocket( void ) const ;
    CommunicationNode* get_FailureManager( void ) const ;
    bool is_LocalNodeThreaded( void ) const ;
    int send_FilterStatesToParent( void );
    bool update( void );
    //bool reset_Topology( std::string& itopology );
    bool add_SubGraph( Rank iroot_rank, SerialGraph& sg, bool iupdate  );
    bool remove_Node( Rank ifailed_rank, bool iupdate=true );
    bool change_Parent( Rank ichild_rank, Rank inew_parent_rank );
    bool insert_EndPoint( std::string& hostname, Port port, Rank rank );
    bool remove_EndPoint( Rank irank );

    bool delete_PeerNode( Rank );
    PeerNodePtr new_PeerNode( std::string const& ihostname, Port iport,
                              Rank irank, bool iis_upstream,
                              bool iis_internal );
    void close_PeerNodeConnections( void );
    void disable_FailureRecovery( void );
    void enable_FailureRecovery( void );
    bool recover_FromFailures( void ) const ;
    void send_BufferedTopoUpdates(std::vector< update_contents_t* > );

    void collect_PerfData( void );

    //Data Members
    std::string _local_hostname;
    Port _local_port;
    Rank _local_rank;
    NetworkTopology* _network_topology;
    CommunicationNode* _failure_manager;
    Communicator* _bcast_communicator;
    FrontEndNode* _local_front_end_node;
    BackEndNode* _local_back_end_node;
    InternalNode* _local_internal_node;
    TimeKeeper* _local_time_keeper;
    EventDetector* _edt;
    unsigned int next_stream_id;

    std::set< PeerNodePtr > _children;
    std::map< unsigned int, Stream* > _streams;
    std::map< Rank, CommunicationNode* > _end_points;
    std::map< unsigned int, Stream* >::iterator _stream_iter;

    bool _threaded;
    bool _recover_from_failures;
    bool _terminate_backends;
    bool _was_shutdown;

    /* EventPipe notifications */
    std::map< EventType, EventPipe* > _evt_pipes;

    mutable XPlat::Monitor _parent_sync;
    mutable XPlat::Monitor _streams_sync;
    mutable XPlat::Mutex _children_mutex;
    mutable XPlat::Mutex _endpoints_mutex;
    mutable XPlat::Monitor _shutdown_sync;
};

extern Network* _global_network;

} /* MRN namespace */

#endif /* __network_h */
