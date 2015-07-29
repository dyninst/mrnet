/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(ParsedGraph_h)
#define ParsedGraph_h

#include <vector>
#include <map>
#include <string>

#include "SerialGraph.h"
#include "mrnet/Error.h"

namespace MRN
{

class ParsedGraph;
extern ParsedGraph * parsed_graph;

class ParsedGraph: public Error {
  public:
    class Node {
        friend class ParsedGraph;
    private:
        std::string _hostname;
        Rank _local_rank;
        Rank _rank;
        std::vector < Node * > _children;
        Node * _parent;
        bool _visited;
        
    public:
        Node( const char *ihostname, Rank ilocal_rank );

        std::string const& get_HostName( ) const { return _hostname; }
        Rank get_LocalRank( ) const { return _local_rank; }
        Rank get_Rank( ) const { return _rank; }

        bool visited( ) const { return _visited; }
        void visit( ) { _visited=true; }

        const std::vector<Node*>& get_Children( void ) const { return _children; }
        void add_Child( Node * c ) { _children.push_back(c); }
        void remove_Child( Node * c ) ;
   
        const Node* get_Parent( void ) const { return _parent; }
        void set_Parent( Node * p ) { _parent = p; };
        
        void print_Node( FILE *, unsigned int idepth );
    };

 private:
    Node * _root;
    std::map < std::string, Node * > _nodes;
    bool _fully_connected;
    bool _cycle_free;

    unsigned int preorder_traversal( Node * );
    void serialize( Node *, bool have_backends );

    static Rank _next_node_rank;
    SerialGraph _serial_graph;

 public:
    ParsedGraph(void)
        : _root(NULL), _fully_connected(true), _cycle_free(true) { }
    ~ParsedGraph(void);

    void set_Root( Node * r ) { _root = r; }
    Node *get_Root(void) const  { return _root; }
    Node *find_Node( char *ihostname, Rank ilocal_rank );
    bool validate(void);
    void add_Node( Node * );
    size_t get_Size(void) const { return _nodes.size(); }
    void assign_NodeRanks( bool iassign_backend_ranks );

    std::string get_SerializedGraphString( bool have_backends );
    SerialGraph & get_SerializedGraph( bool have_backends );

    void print_Graph( FILE *f ) { _root->print_Node(f, 0); }
    void print_DOTGraph( const char * filename );
};

}                               // namespace MRN

#endif                          /* ParsedGraph_h */
