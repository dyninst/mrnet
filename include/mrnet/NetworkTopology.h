/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
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
#include "mrnet/Network.h"
#include "mrnet/Types.h"
#include "xplat/Monitor.h"

namespace MRN
{

typedef enum{ ALG_RANDOM=0, 
              ALG_WRS,
              ALG_SORTED_RR
} ALGORITHM_T;

typedef enum{ TOPOL_RESET=0,
              TOPOL_ADD_SUBGRAPH,
              TOPOL_REMOVE_NODE,
              TOPOL_PARENT_CHANGE
} TopologyEvent;

class Router;
class SerialGraph;
class TopologyLocalInfo;

class NetworkTopology: public Error {

    friend class Router;
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

  public:

    // BEGIN MRNET API

    void print_TopologyFile( const char * filename ) const;
    void print_DOTGraph( const char * filename ) const;
    void print( FILE * ) const;
    
    unsigned int get_NumNodes() const;
    void get_TreeStatistics( unsigned int &onum_nodes,
                             unsigned int &odepth,
                             unsigned int &omin_fanout, 
                             unsigned &omax_fanout,
                             double &oavg_fanout,
                             double &ostddev_fanout);

    Node * get_Root() { return _root; }
    Node * find_Node( Rank ) const;
    void get_Leaves( std::vector< Node * > &leaves ) const;

    /* For get_XXXNodes, cannot guarantee that set nodes are valid
       in the presence of an updating NetworkTopology, so use carefully */ 
    void get_ParentNodes( std::set<NetworkTopology::Node*> & ) const;
    void get_OrphanNodes( std::set<NetworkTopology::Node*> & ) const;
    void get_BackEndNodes( std::set<NetworkTopology::Node*> & ) const;

    // END MRNET API

    NetworkTopology( Network *, SerialGraph & );
    NetworkTopology( Network *, std::string &ihostname, Port iport, Rank irank, 
                     bool iis_backend = false );

    //Topology update operations
    bool add_SubGraph( Rank, SerialGraph &, bool iupdate );
    bool remove_Node( Rank, bool iupdate );
    bool remove_Node( Node * );
    bool set_Parent( Rank c,  Rank p, bool iupdate );
    bool reset( std::string serial_graph="", bool iupdate=true );

    //Access topology components
    bool node_Failed( Rank irank ) const ;
    PeerNodePtr get_OutletNode( Rank irank ) const;
    char * get_TopologyStringPtr( );
    char * get_LocalSubTreeStringPtr();

    Node * find_NewParent( Rank ichild_rank, unsigned int iretry=0,
                           ALGORITHM_T algorithm=ALG_WRS );

    void compute_TreeStatistics( void );

    //new members added for topology propagation change
    bool new_Node( const std::string &, Port, Rank, bool iis_backend );
    bool isInTopology(std::string hostname, Port _port, Rank _rank);
    void insert_updates_buffer( update_contents_t* uc);
    std::vector<update_contents_t* > get_updates_buffer( void );

    //these two members are made public from private for topo prop change
    void serialize( Node * );
    void update_Router_Table();

  private:
   
    Node * find_NodeHoldingLock( Rank ) const;
    bool remove_Orphan( Rank );
    void remove_SubGraph( Node * inode );

    void get_Descendants( Node *, std::vector< Node * > &odescendants ) const;
    void get_LeafDescendants( Node *inode, 
                              std::vector< Node * > &odescendants ) const;

    unsigned int get_TreeDepth() const;

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
    std::vector<update_contents_t* > _updates_buffer;


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
