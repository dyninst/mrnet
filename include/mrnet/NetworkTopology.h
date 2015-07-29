/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(NetworkTopology_h)
#define NetworkTopology_h 1

#include <map>
#include <set>
#include <string>
#include <vector>

#include <cstdio>
#include <cmath>

#include "mrnet/Error.h"
#include "mrnet/Types.h"
#include "xplat/Monitor.h"

#include <boost/shared_ptr.hpp>

namespace MRN
{

typedef enum{ ALG_RANDOM=0, 
              ALG_WRS,
              ALG_SORTED_RR
} ALGORITHM_T;

class Network;
class Router;
class SerialGraph;
class TopologyLocalInfo;
class PeerNode;
typedef boost::shared_ptr< PeerNode > PeerNodePtr;

class NetworkTopology: public Error {

    friend class TopologyLocalInfo;

  public:

    class Node{
        friend class NetworkTopology;
        friend class TopologyLocalInfo;
        
    public:

        // BEGIN MRNET API

        std::string get_HostName( void ) const;
        Port get_Port( void ) const;
        Rank get_Rank( void ) const;
        Rank get_Parent (void) const;
        const std::set< Node * > & get_Children( void ) const;
        unsigned int get_NumChildren( void ) const;
        unsigned int find_SubTreeHeight( void );

        // END MRNET API

        double get_AdoptionScore( void ) const;
        double get_WRSKey( void ) const;
        double get_RandKey( void ) const;
        bool failed( void ) const ;
	void set_Port( Port ) ;

    private:
        Node( const std::string &, Port, Rank, bool iis_backend );
        void set_Parent( Node * p );
        void add_Child( Node * c );
        bool remove_Child( Node * c ) ;
        bool is_BackEnd( void ) const;
        unsigned int get_DepthIncrease( Node * ) const ;
        unsigned int get_Proximity( Node * );
        void compute_AdoptionScore( Node *iorphan,
                                    unsigned int imin_fanout,
                                    unsigned int imax_fanout,
                                    unsigned int idepth );

        std::string _hostname;
        Port _port;
        Rank _rank;
        bool _failed;
        std::set < Node * > _children;
        std::set < Node * > _ascendants;
        Node * _parent;
        bool _is_backend;

        unsigned int _depth, _subtree_height;
        double _adoption_score;
        double _rand_key;
        double _wrs_key;
    };

    typedef enum {
        TOPO_NEW_BE        = 0,
        TOPO_REMOVE_RANK   = 1,
	TOPO_CHANGE_PARENT = 2,
	TOPO_CHANGE_PORT   = 3,
	TOPO_NEW_CP        = 4 
    } update_type; 	

    typedef struct {
        update_type type;
        Rank par_rank;
	Rank chld_rank;
	char* chld_host;
	Port chld_port;
    } update_contents;	

  public:

    // BEGIN MRNET API

    void print_TopologyFile( const char * filename ) const;
    void print_DOTGraph( const char * filename ) const;
    void print( FILE * ) const;
    
    unsigned int get_NumNodes(void) const;
    void get_TreeStatistics( unsigned int &onum_nodes,
                             unsigned int &odepth,
                             unsigned int &omin_fanout, 
                             unsigned &omax_fanout,
                             double &oavg_fanout,
                             double &ostddev_fanout);

    Node * get_Root(void) { return _root; }
    Node * find_Node( Rank ) const;
    void get_Leaves( std::vector< Node * > &leaves ) const;

    /* For get_XXXNodes, cannot guarantee that set nodes are valid
       in the presence of an updating NetworkTopology, so use carefully */ 
    void get_ParentNodes( std::set<NetworkTopology::Node*> & ) const;
    void get_OrphanNodes( std::set<NetworkTopology::Node*> & ) const;
    void get_BackEndNodes( std::set<NetworkTopology::Node*> & ) const;

    size_t num_BackEndNodes(void) const;
    size_t num_InternalNodes(void) const;

    // END MRNET API

    void get_Descendants( Node *, std::vector< Node * > &odescendants ) const;
    void get_LeafDescendants( Node *inode, 
                              std::vector< Node * > &odescendants ) const;

    NetworkTopology( Network *, SerialGraph & );
    NetworkTopology( Network *, std::string &ihostname, Port iport, Rank irank, 
                     bool iis_backend = false );
    ~NetworkTopology(void);

    //Topology update operations
    bool add_SubGraph( Rank, SerialGraph &, bool iupdate );
    bool remove_Node( Rank, bool iupdate );
    bool remove_Node( Node * );
    bool set_Parent( Rank c,  Rank p, bool iupdate );
    bool reset( std::string serial_graph="", bool iupdate=true );

    //Access topology components
    bool node_Failed( Rank irank ) const ;
    PeerNodePtr get_OutletNode( Rank irank ) const;
    std::string get_TopologyString(void);
    std::string get_LocalSubTreeString(void);

    Node * find_NewParent( Rank ichild_rank, unsigned int iretry=0,
                           ALGORITHM_T algorithm=ALG_WRS );

    void compute_TreeStatistics(void);

    // routines used by topology update propagation
    bool new_Node( const std::string &, Port, Rank, bool iis_backend );
    bool in_Topology( std::string ihostname, Port iport, Rank irank );
 
    void insert_updates_buffer( update_contents* uc );
    bool send_updates_buffer(void);

    void update_addBackEnd( Rank par_rank, Rank chld_rank, char* chld_host, 
                            Port chld_port, bool upstream );
    void update_addInternalNode( Rank par_rank, Rank chld_rank, 
                                 char* chld_host, Port chld_port, bool upstream );
    void update_changeParent( Rank par_rank, Rank chld_rank, bool upstream);
    void update_changePort( Rank chld_rank, Port chld_port, bool upstream );
    void update_removeNode( Rank par_rank, Rank chld_rank, bool upstream);
  
    void update_Router_Table(void);
    void update_TopoStreamPeers( std::vector< Rank >& new_nodes );

    void serialize( Node * );

  private:   

    Node * find_NodeHoldingLock( Rank ) const;
    bool remove_Orphan( Rank );
    void remove_SubGraph( Node * inode );


    unsigned int get_TreeDepth(void) const;

    void print_DOTSubTree( NetworkTopology::Node * inode, FILE * f ) const;

    //void serialize( Node * );
    bool add_SubGraph( Node *, SerialGraph &, bool iupdate );
    void find_PotentialAdopters( Node * iadoptee,
                                 Node * ipotential_adopter,
                                 std::vector<Node*> &oadopters );
    void compute_AdoptionScores( std::vector<Node*> iadopters, Node *iorphan );

    //Data Members
    Network *_network;
    Node *_root;
    Router *_router;
    unsigned int _min_fanout, _max_fanout, _depth;
    double _avg_fanout, _stddev_fanout, _var_fanout;
    std::map< Rank, Node * > _nodes;
    std::set< Node * > _orphans;
    std::set< Node * > _backend_nodes;
    std::set< Node * > _parent_nodes;
    mutable XPlat::Monitor _sync;
    SerialGraph *_serial_graph;
    std::vector< update_contents* > _updates_buffer;

    void find_PotentialAdopters( Node * iadoptee,
                                 Node * ipotential_adopter,
                                 std::list<Node*> &oadopters );
};

class TopologyLocalInfo {
    friend class NetworkTopology;
        
 private:
    NetworkTopology::Node* local_node;
    NetworkTopology* topol;

 public:
    TopologyLocalInfo( NetworkTopology* itopol, NetworkTopology::Node* inode )
        : local_node(inode), topol(itopol) 
        {}

    Rank get_Rank() const;

    unsigned int get_NumChildren() const;
    unsigned int get_NumSiblings() const;
    unsigned int get_NumDescendants() const;
    unsigned int get_NumLeafDescendants() const;

    unsigned int get_RootDistance() const;
    unsigned int get_MaxLeafDistance() const;

    const NetworkTopology* get_Topology() const;
    const Network* get_Network() const;
};

}                               // namespace MRN

#endif                          /* NetworkTopology_h */
