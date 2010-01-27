/****************************************************************************
 * Copyright © 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ***************************************************************************/

#if !defined( __tree_h )
#define __tree_h  1

#include <list>
#include <map>
#include <string>
#include <set>
#include <utility>

namespace MRN {

class BalancedTree;
class KnomialTree;
class GenericTree;

class Tree {

    friend class BalancedTree;
    friend class GenericTree;
    friend class KnomialTree;

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

    bool create_TopologyFile( const char* ifilename );
    bool create_TopologyFile( FILE* ifile );
    void get_TopologyBuffer( char** buf );

    static bool get_HostsFromFile( const char* ifilename, 
                                   std::list< std::pair<std::string,unsigned> >& hosts );
    static void get_HostsFromFileStream( FILE* ifile, 
                                         std::list< std::pair<std::string,unsigned> >& hosts );

    // END MRNET API

    Tree();
    virtual ~Tree(){}

 protected:
    Node * root;
    std::map<std::string, Node *> NodesByName;
    bool _contains_cycle, _contains_unreachable_nodes;
    unsigned int _fanout, _num_leaves, _depth;

    bool validate();
    bool contains_Cycle() { return _contains_cycle; }
    bool contains_UnreachableNodes() { return _contains_unreachable_nodes; }
    virtual bool initialize_Tree( std::string &topology_spec,
                                  std::list< std::pair<std::string,unsigned> > &hosts)=0;
};

class BalancedTree: public Tree {
 private:
    virtual bool initialize_Tree( std::string &topology_spec,
                                  std::list< std::pair<std::string,unsigned> > &hosts );
    unsigned int _fe_procs_per_host, _be_procs_per_host, _int_procs_per_host;

 public:

    // BEGIN MRNET API

    BalancedTree( std::string &topology_spec, 
                  std::list< std::pair<std::string,unsigned> > &hosts,
                  unsigned int ife_procs_per_host=1,
                  unsigned int ibe_procs_per_host=1,
                  unsigned int iint_procs_per_host=1 );
    BalancedTree( std::string &topology_spec, 
                  std::string &host_file,
                  unsigned int ife_procs_per_host=1,
                  unsigned int ibe_procs_per_host=1,
                  unsigned int iint_procs_per_host=1 );

    // END MRNET API
};

class KnomialTree: public Tree {
 private:
    virtual bool initialize_Tree( std::string &topology_spec,
                                  std::list< std::pair<std::string,unsigned> > &hosts );
    unsigned int _max_procs_per_host;

 public:

    // BEGIN MRNET API

    KnomialTree( std::string &topology_spec, 
                 std::list< std::pair<std::string,unsigned> > &hosts,
                 unsigned int imax_procs_per_host=1 );
    KnomialTree( std::string &topology_spec, std::string &host_file,
                 unsigned int imax_procs_per_host=1 );

    // END MRNET API
};

class GenericTree: public Tree {
 private:
    virtual bool initialize_Tree( std::string &topology_spec,
                                  std::list< std::pair<std::string,unsigned> > &hosts );
    unsigned int _max_procs_per_host;

 public:

    // BEGIN MRNET API

    GenericTree( std::string &topology_spec, 
                 std::list< std::pair<std::string,unsigned> > &hosts,
                 unsigned int imax_procs_per_host=1 );
    GenericTree( std::string &topology_spec, std::string &host_file,
                 unsigned int imax_procs_per_host=1 );

    // END MRNET API
};

} /* namespace MRN */

#endif /* __tree_h */
