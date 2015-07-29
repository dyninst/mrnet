/****************************************************************************
 * Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller   *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ***************************************************************************/

#if !defined( __tree_h )
#define __tree_h  1

#include <list>
#include <map>
#include <string>
#include <set>
#include <utility>
#include <vector>

namespace MRN {

class BalancedTree;
class KnomialTree;
class GenericTree;

class Tree {

    friend class BalancedTree;
    friend class GenericTree;
    friend class KnomialTree;

    Tree();
    Tree( std::string & fe_host,
          std::string & topology_spec,
          unsigned int be_procs_per_host,
          unsigned int int_procs_per_host );

 public:

    // BEGIN MRNET API

    class Node{
     private:
        std::string _name;
        std::set<Node*> children;
        bool visited;
        
     public:
        Node( std::string n );
        void add_Child( Node * );
        void print_ToFile(FILE *);
        std::string name(){ return _name; }       
        unsigned int visit();
    };

    Node * get_Node( std::string );
    bool is_Valid() { return _valid; }

    bool create_TopologyFile( const char* ifilename );
    bool create_TopologyFile( FILE* ifile );

    static bool get_HostsFromFile( const char* ifilename, 
                                   std::list< std::pair<std::string,unsigned> >& hosts );
    static void get_HostsFromFileStream( FILE* ifile, 
                                         std::list< std::pair<std::string,unsigned> >& hosts );
    static void get_HostsFromBuffer( const char* ibuf, 
                                     std::list< std::pair<std::string,unsigned> >& hosts );

    // END MRNET API

    virtual ~Tree(){}

 protected:
    Node * root;
    std::map<std::string, Node *> NodesByName;
    std::string _fe_host, _topology_spec;
    bool _contains_cycle, _contains_unreachable_nodes, _valid;
    unsigned int _num_leaves, _depth;
    unsigned int _be_procs_per_host, _int_procs_per_host;

    bool validate();
    bool contains_Cycle() { return _contains_cycle; }
    bool contains_UnreachableNodes() { return _contains_unreachable_nodes; }
};

class BalancedTree: public Tree {
 private:
    bool parse_Spec( );

    bool initialize_Tree( std::list< std::pair<std::string,unsigned> > & hosts );
    bool initialize_Tree( std::list< std::pair<std::string,unsigned> > & backend_hosts,
                          std::list< std::pair<std::string,unsigned> > & internal_hosts );

    std::vector< unsigned int > _fanouts;

 public:

    // BEGIN MRNET API

    BalancedTree( std::string & topology_spec,
                  std::string & fe_host, 
                  std::list< std::pair<std::string,unsigned> > & hosts,
                  unsigned int max_procs_per_host=1 );

    BalancedTree( std::string & topology_spec,
                  std::string & fe_host, 
                  std::list< std::pair<std::string,unsigned> > & backend_hosts,
                  std::list< std::pair<std::string,unsigned> > & internal_hosts,
                  unsigned int be_procs_per_host=1,
                  unsigned int int_procs_per_host=1 );


    // END MRNET API
};

class KnomialTree: public Tree {
 private:
    bool parse_Spec( );

    bool initialize_Tree( std::list< std::pair<std::string,unsigned> > & hosts );
    bool initialize_Tree( std::list< std::pair<std::string,unsigned> > & backend_hosts,
                          std::list< std::pair<std::string,unsigned> > & internal_hosts );

    unsigned int _kfactor;
    unsigned int _num_nodes;

 public:

    // BEGIN MRNET API

    KnomialTree( std::string & topology_spec,
                 std::string & fe_host,
                 std::list< std::pair<std::string,unsigned> > & hosts,
                 unsigned int max_procs_per_host=1 );

    KnomialTree( std::string & topology_spec,
                 std::string & fe_host,
                 std::list< std::pair<std::string,unsigned> > & backend_hosts,
                 std::list< std::pair<std::string,unsigned> > & internal_hosts,
                 unsigned int be_procs_per_host=1,
                 unsigned int int_procs_per_host=1 );

    // END MRNET API
};

class GenericTree: public Tree {
 private:
    bool parse_Spec( );

    bool initialize_Tree( std::list< std::pair<std::string,unsigned> > & hosts );
    bool initialize_Tree( std::list< std::pair<std::string,unsigned> > & backend_hosts,
                          std::list< std::pair<std::string,unsigned> > & internal_hosts );
 
    std::vector< std::vector<unsigned int>* > _children_by_level;

 public:

    // BEGIN MRNET API

    GenericTree( std::string & topology_spec,
                 std::string & fe_host, 
                 std::list< std::pair<std::string,unsigned> > & hosts,
                 unsigned int max_procs_per_host=1 );

    GenericTree( std::string & topology_spec,
                 std::string & fe_host, 
                 std::list< std::pair<std::string,unsigned> > & backend_hosts,
                 std::list< std::pair<std::string,unsigned> > & internal_hosts,
                 unsigned int be_procs_per_host=1,
                 unsigned int int_procs_per_host=1 );

    // END MRNET API

    ~GenericTree();
};

} /* namespace MRN */

#endif /* __tree_h */
