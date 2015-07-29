/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <limits.h>

#include "utils.h"
#include "mrnet/Tree.h"
#include "xplat/Tokenizer.h"

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif

using namespace std;

namespace MRN{

Tree::Node::Node(string n) : _name(n), visited(false)
{}


void Tree::Node::add_Child( Tree::Node *n )
{
    children.insert( n );
}

void Tree::Node::print_ToFile( FILE *f )
{
    if( children.size() == 0 )
        return;

    fprintf(f, "%s =>", _name.c_str() );

    set<Tree::Node*>::iterator iter;
    for( iter=children.begin(); iter!= children.end(); iter++){
        fprintf(f, "\n\t%s", (*iter)->name().c_str() );
    }
    fprintf(f, " ;\n\n");

    for( iter=children.begin(); iter!= children.end(); iter++){
        (*iter)->print_ToFile( f );
    }
}

unsigned int Tree::Node::visit()
{
    int retval;
    unsigned int num_nodes_visited=1;

    if( visited ){
        return 0;
    }

    visited=true;

    set<Tree::Node*>::iterator iter;

    for( iter=children.begin(); iter!=children.end(); iter++ ){
        retval = (*iter)->visit();
        if(  retval == 0 ){
            return 0;
        }
        num_nodes_visited += retval;
    }

    return num_nodes_visited;
}

Tree::Tree()
    : root(NULL), 
      _contains_cycle(false), _contains_unreachable_nodes(false), _valid(false), 
      _num_leaves(0), _depth(0),
      _be_procs_per_host(0), _int_procs_per_host(0)
{
    assert(!"Nobody should call this!!");
}

Tree::Tree( std::string & fe_host, 
            std::string & topology_spec,
            unsigned int be_procs_per_host,
            unsigned int int_procs_per_host )
    : root(NULL), _fe_host(fe_host), _topology_spec(topology_spec),
      _contains_cycle(false), _contains_unreachable_nodes(false), _valid(false), 
      _num_leaves(0), _depth(0),
      _be_procs_per_host(be_procs_per_host), _int_procs_per_host(int_procs_per_host)
{
    char cur_host[HOST_NAME_MAX];
    sprintf( cur_host, "%s:0", _fe_host.c_str() );
    string root_node( cur_host );    
    root = get_Node( root_node );
}

Tree::Node * Tree::get_Node( string hostname )
{
    Tree::Node * node;

    map<string, Tree::Node *>::iterator iter;

    iter = NodesByName.find( hostname );
    if( iter != NodesByName.end() ){
        node = (*iter).second;
    }
    else{
        node = new Tree::Node( hostname );
        NodesByName[hostname] = node;
    }

    return node;
}

bool Tree::create_TopologyFile( FILE* ifile )
{
    root->print_ToFile( ifile );

    return true;
}

bool Tree::create_TopologyFile( const char *ifilename )
{
    FILE * f = fopen( ifilename, "w" );

    if( f == NULL ) {
        ::perror( "fopen()" );
        return false;
    }
    root->print_ToFile( f );
    fclose( f );

    return true;
}

bool Tree::get_HostsFromFile( const char *ifilename, list< pair<string,unsigned> > &hosts )
{
    FILE * f = fopen( ifilename, "r" );
    if( f == NULL ) {
        ::perror( "fopen()" );
        return false;
    }
    else {
        get_HostsFromFileStream( f, hosts );
        fclose( f );
        return true;
    }
}

void Tree::get_HostsFromFileStream( FILE *ifile, list< pair<string,unsigned> > &hosts )
{
    char cur_host[HOST_NAME_MAX];
    map< string, unsigned > host_counts;
    map< string, unsigned >::iterator find_host;
    while( fscanf( ifile, "%s", cur_host ) != EOF ) {

        char* colon = strchr( cur_host, ':' );
        unsigned count = 1;
        if( colon != NULL ) {
            *colon = '\0';
            count = (unsigned) atoi( colon+1 );
        }

        string host(cur_host);
        find_host = host_counts.find( host );
        if( find_host != host_counts.end() )
            count += find_host->second; 
        host_counts[ host ] = count;
    }

    // add them to hosts list
    find_host = host_counts.begin();
    for( ; find_host != host_counts.end() ; find_host++ )
        hosts.push_back( make_pair(find_host->first, find_host->second) );
}

void Tree::get_HostsFromBuffer( const char* ibuf, list< pair<string,unsigned> > &hosts )
{
    const char* buf = ibuf;
    char cur_host[HOST_NAME_MAX];
    map< string, unsigned > host_counts;
    map< string, unsigned >::iterator find_host;
    while( sscanf( buf, "%s", cur_host ) != EOF ) {

	buf = strstr(buf, cur_host);
        buf += strlen(cur_host) + 1;
        char* colon = strchr( cur_host, ':' );
        unsigned count = 1;
        if( colon != NULL ) {
            *colon = '\0';
            count = (unsigned) atoi( colon+1 );
        }

        string host(cur_host);
        find_host = host_counts.find( host );
        if( find_host != host_counts.end() )
            count += find_host->second; 
        host_counts[ host ] = count;
    }

    // add them to hosts list
    find_host = host_counts.begin();
    for( ; find_host != host_counts.end() ; find_host++ )
        hosts.push_back( make_pair(find_host->first, find_host->second) );
}

bool Tree::validate()
{
    bool retval=true;

    unsigned int num_nodes_visited = root->visit() ;
    if( num_nodes_visited == 0 ){
        _contains_cycle = true;
        retval = false;
    }

    if(  num_nodes_visited != NodesByName.size() ){
        _contains_unreachable_nodes = true;
        retval = false;
    }

    _valid = retval;
    return retval;
}

BalancedTree::BalancedTree( string & topology_spec,
                            string & fe_host,
                            list< pair<string,unsigned> > & internal_hosts,
                            list< pair<string,unsigned> > & backend_hosts,
                            unsigned int be_procs_per_host  /* =1 */,
                            unsigned int int_procs_per_host /* =1 */ )
    : Tree( fe_host, topology_spec, be_procs_per_host, int_procs_per_host )
{
    if( parse_Spec() )
        _valid = initialize_Tree( internal_hosts, backend_hosts );
}

BalancedTree::BalancedTree( string & topology_spec,
                            string & fe_host,
                            list< pair<string,unsigned> > & hosts,
                            unsigned int max_procs_per_host  /* =1 */ )
    : Tree( fe_host, topology_spec, max_procs_per_host, max_procs_per_host )
{
    if( parse_Spec() )
        _valid = initialize_Tree( hosts );
}

bool BalancedTree::parse_Spec( )
{
    if( ! _topology_spec.empty() ) {

        if( _topology_spec.find_first_of( "^" ) != string::npos ) {
            unsigned fanout;
            if( sscanf(_topology_spec.c_str(), "%u^%u", &fanout, &_depth) != 2 ) {
                fprintf(stderr, "Bad topology specification: \"%s\"."
                    "Should be of format Fanout^Depth.\n", _topology_spec.c_str() );
                return false;
            }

            for(unsigned int i=0; i<_depth; i++){
                _fanouts.push_back( fanout );
            }
        }
        else {
            XPlat::Tokenizer tok( _topology_spec );
            string::size_type cur_len;
            const char* delim = "x\n";
            string::size_type cur_pos = tok.GetNextToken( cur_len, delim );

            while( cur_pos != string::npos ) {
                string cur_fanout = _topology_spec.substr( cur_pos, cur_len );
                _fanouts.push_back( atoi( cur_fanout.c_str() ) );
                cur_pos = tok.GetNextToken( cur_len, delim );
            }
        }
    }
    return true;
}

bool BalancedTree::initialize_Tree( list< pair<string,unsigned> > & backend_hosts,
                                    list< pair<string,unsigned> > & internal_hosts )
{
    vector< vector< Tree::Node*> > nodes_by_level;
    map< string, unsigned > host_proc_counts;

    if( internal_hosts.empty() && backend_hosts.empty() && _fanouts.size() ) {
        fprintf( stderr, "Input host lists are empty. Not enough hosts for topology %s\n",
                 _topology_spec.c_str() );
        return false;
    }

    if( internal_hosts.empty() && (_fanouts.size() > 1) ) {
        fprintf( stderr, "Input internal host list is empty. Not enough hosts for topology %s\n",
                 _topology_spec.c_str() );
        return false;
    }

    //Process 1st level (root)
    nodes_by_level.push_back( vector< Tree::Node*>() );
    nodes_by_level[0].push_back( root );
    host_proc_counts[ _fe_host ] = 1;

    //Process internal levels
    char cur_host[HOST_NAME_MAX];
    unsigned int nodes_at_cur_level=1;
    list< pair<string,unsigned> >::const_iterator list_iter = internal_hosts.begin();

    for( unsigned int i=0; i < _fanouts.size()-1; i++ ) {

        nodes_at_cur_level *= _fanouts[i];
        nodes_by_level.push_back( vector< Tree::Node*>() );

        for( unsigned int j=0; j < nodes_at_cur_level; j++ ) {
            
            if( list_iter == internal_hosts.end() ) {
                fprintf( stderr, "After using all procs on CP, not enough hosts for topology %s\n",
                         _topology_spec.c_str() );
                return false;
            }
            
            if( host_proc_counts.find(list_iter->first) == host_proc_counts.end() ) {
                // initialize host count to zero
                host_proc_counts[ list_iter->first ] = 0;
            }
            unsigned& cnt = host_proc_counts[ list_iter->first ];
            snprintf( cur_host, sizeof(cur_host), "%s:%u",
                      list_iter->first.c_str(), cnt++ );
            
            nodes_by_level[ i+1 ].push_back( get_Node( cur_host ) );

            if( (cnt >= _int_procs_per_host) || (cnt >= list_iter->second) )
                list_iter++;
        }
    }

    //Process last level (leaves)
    list_iter = backend_hosts.begin();
    nodes_at_cur_level *= _fanouts[ _fanouts.size()-1 ];
    nodes_by_level.push_back( vector< Tree::Node*>() );

    for( unsigned int i=0; i < nodes_at_cur_level; i++ ) {

        if( list_iter == backend_hosts.end() ) {
            fprintf( stderr, "Ran out of locations while placing BEs, not enough hosts for topology %s\n",
                     _topology_spec.c_str() );
            return false;
        }
        
        if( host_proc_counts.find(list_iter->first) == host_proc_counts.end() ) {
            // initialize host count to zero
            host_proc_counts[ list_iter->first ] = 0;
        }
        unsigned& cnt = host_proc_counts[ list_iter->first ];
        snprintf( cur_host, sizeof(cur_host), "%s:%u",
                  list_iter->first.c_str(), cnt++ );
        nodes_by_level[ _fanouts.size() ].push_back( get_Node( cur_host ) );

        if( (cnt >= list_iter->second) || (cnt >= _be_procs_per_host) )
            ++list_iter;
    }

    //Connect tree nodes
    unsigned int next_orphan_idx=0;
    Tree::Node * cur_parent_node=0;
    for( unsigned int i=0; i < nodes_by_level.size()-1; i++ ) {
        next_orphan_idx=0;

        for( unsigned int j=0; j < nodes_by_level[i].size(); j++ ) {
            //assign orphans to each parent at current level
            cur_parent_node = nodes_by_level[i][j];

            for(unsigned int k=0; k < _fanouts[i]; k++){
                cur_parent_node->add_Child
                    ( nodes_by_level[i+1][next_orphan_idx] );
                next_orphan_idx++;
            }
        }
    }

    return validate();
}

bool BalancedTree::initialize_Tree( list< pair<string,unsigned> > & hosts )
{
    vector< vector< Tree::Node*> > nodes_by_level;
    map< string, unsigned > host_proc_counts;

    list< pair<string,unsigned> >::const_iterator list_iter = hosts.begin();
    if( list_iter == hosts.end() && _fanouts.size() ) {
        fprintf( stderr, "Input host list is empty. Not enough hosts for topology %s\n", 
                 _topology_spec.c_str() );
        return false;
    }

    //Process 1st level (root)
    nodes_by_level.push_back( vector< Tree::Node*>() );
    nodes_by_level[0].push_back( root );
    host_proc_counts[ _fe_host ] = 1;

    //Process internal levels
    char cur_host[HOST_NAME_MAX];
    unsigned int nodes_at_cur_level=1;
    for( unsigned int i=0; i < _fanouts.size()-1; i++ ) {

        nodes_at_cur_level *= _fanouts[i];
        nodes_by_level.push_back( vector< Tree::Node*>() );

        for( unsigned int j=0; j < nodes_at_cur_level; j++ ) {

            if( list_iter == hosts.end() ) {
                fprintf( stderr, "After using all procs on CP, not enough hosts for topology %s\n",
                         _topology_spec.c_str() );
                return false;
            }
            
            if( host_proc_counts.find(list_iter->first) == host_proc_counts.end() ) {
                // initialize host count to zero
                host_proc_counts[ list_iter->first ] = 0;
            }
            unsigned& cnt = host_proc_counts[ list_iter->first ];
            snprintf( cur_host, sizeof(cur_host), "%s:%u",
                      list_iter->first.c_str(), cnt++ );

            nodes_by_level[ i+1 ].push_back( get_Node( cur_host ) );

            
            if( (cnt >= _int_procs_per_host) || (cnt >= list_iter->second) )
                list_iter++;
        }
    }

    //Process last level (leaves)
    nodes_at_cur_level *= _fanouts[ _fanouts.size()-1 ];
    nodes_by_level.push_back( vector< Tree::Node*>() );

    for( unsigned int i=0; i < nodes_at_cur_level; i++ ) {

        if( list_iter == hosts.end() ) {
            fprintf( stderr, "Ran out of locations while placing BEs, not enough hosts for topology %s\n",
                     _topology_spec.c_str() );
            return false;
        }
        
        if( host_proc_counts.find(list_iter->first) == host_proc_counts.end() ) {
            // initialize host count to zero
            host_proc_counts[ list_iter->first ] = 0;
        }
        unsigned& cnt = host_proc_counts[ list_iter->first ];
        snprintf( cur_host, sizeof(cur_host), "%s:%u",
                  list_iter->first.c_str(), cnt++ );

        nodes_by_level[ _fanouts.size() ].push_back( get_Node( cur_host ) );

        if( (cnt >= list_iter->second) || (cnt >= _be_procs_per_host) )
            ++list_iter;
    }

    //Connect tree nodes
    unsigned int next_orphan_idx=0;
    Tree::Node * cur_parent_node=0;
    for( unsigned int i=0; i < nodes_by_level.size()-1; i++ ) {
        next_orphan_idx=0;

        for( unsigned int j=0; j < nodes_by_level[i].size(); j++ ) {
            //assign orphans to each parent at current level
            cur_parent_node = nodes_by_level[i][j];

            for(unsigned int k=0; k < _fanouts[i]; k++){
                cur_parent_node->add_Child
                    ( nodes_by_level[i+1][next_orphan_idx] );
                next_orphan_idx++;
            }
        }
    }

    return validate();
}

KnomialTree::KnomialTree( string & topology_spec, 
                          string & fe_host,
                          list< pair<string,unsigned> > & hosts,
                          unsigned int max_procs_per_host  /* =1 */ )
    : Tree( fe_host, topology_spec, max_procs_per_host, max_procs_per_host )
{
    if( parse_Spec() )
        _valid = initialize_Tree( hosts );
}

KnomialTree::KnomialTree( string & topology_spec,
                          string & fe_host,
                          list< pair<string,unsigned> > & backend_hosts,
                          list< pair<string,unsigned> > & internal_hosts,
                          unsigned int be_procs_per_host  /* =1 */,
                          unsigned int int_procs_per_host /* =1 */ )
    : Tree( fe_host, topology_spec, be_procs_per_host, int_procs_per_host )
{
    if( parse_Spec() )
        _valid = initialize_Tree( backend_hosts, internal_hosts );
}

bool KnomialTree::parse_Spec( )
{
    bool badspec = false;
    if( _topology_spec.empty() ) {
        badspec = true;
    }
    else if( _topology_spec.find_first_of( "@" ) != string::npos ) {
        if( sscanf(_topology_spec.c_str(), "%u@%u", &_kfactor, &_num_nodes) != 2 ) {
            badspec = true;
        }
    }
    else {
        badspec = true;
    }

    if( badspec ) {
        fprintf( stderr, "Bad topology specification: \"%s\". "
                 "Should be of format K@NumNodes.\n", _topology_spec.c_str() );
        return false;
    }

    return true;
}

bool KnomialTree::initialize_Tree( list< pair<string,unsigned> > & backend_hosts,
                                   list< pair<string,unsigned> > & internal_hosts )
{
    map< string, vector<string> > tree; 
    map< string, unsigned > host_proc_counts;

    char cur_host[HOST_NAME_MAX];
    list< string > int_hostids;
    list< string > be_hostids;

    // treat root as internal for purposes of this algorithm
    snprintf( cur_host, sizeof(cur_host), "%s:%u",
              _fe_host.c_str(), 0 );
    int_hostids.push_back( cur_host );
    host_proc_counts[ _fe_host ] = 1;

    // get internal locations
    list< pair<string,unsigned> >::iterator liter = internal_hosts.begin();
    for( ; liter != internal_hosts.end() ; liter++ ) {
        
        if( host_proc_counts.find(liter->first) == host_proc_counts.end() )
            host_proc_counts[ liter->first ] = 0;

        unsigned& cnt = host_proc_counts[ liter->first ];
        for( unsigned u=0; u < liter->second && u < _int_procs_per_host; u++, cnt++ ) {
            snprintf( cur_host, sizeof(cur_host), "%s:%u",
                      liter->first.c_str(), cnt );
            int_hostids.push_back( cur_host );
        }
    }

    // get backend locations
    liter = backend_hosts.begin();
    for( ; liter != backend_hosts.end() ; liter++ ) {

        if( host_proc_counts.find(liter->first) == host_proc_counts.end() )
            host_proc_counts[ liter->first ] = 0;

        unsigned& cnt = host_proc_counts[ liter->first ];
        for( unsigned u=0; u < liter->second && u < _be_procs_per_host; u++, cnt++ ) {
            snprintf( cur_host, sizeof(cur_host), "%s:%u",
                      liter->first.c_str(), cnt );
            be_hostids.push_back( cur_host );
        }
    }
    
    size_t nhosts = int_hostids.size() + be_hostids.size();
    if( nhosts < _num_nodes ) {
        fprintf( stderr, "Not enough hosts(%" PRIszt") for topology %s\n", 
                 nhosts, _topology_spec.c_str() );
        return false;
    }

    /* Algorithm: Host list treated as two sub-lists, USED and AVAIL. USED is
                  initialized to the first host (the root), AVAIL to the rest.
                  While we still have available hosts and haven't reached the 
                  desired total number of nodes, each member of USED will be
                  assigned k-1 new children from AVAIL. After each round of 
                  adding children from AVAIL to the members of USED, the size
                  of USED is effectively multiplied by k.
    */

    unsigned curr_step_nodes = 1;
    unsigned max_depth = 0;

    list< string >::iterator avail_iter = int_hostids.begin();
    list< string >::iterator avail_end = int_hostids.end();
    avail_iter++;

    while( avail_iter != int_hostids.end() && 
           curr_step_nodes < _num_nodes ) {

        max_depth++;

        list< string >::iterator used_iter = int_hostids.begin();
        list< string >::iterator stop_iter = avail_iter;

        if( (curr_step_nodes * _kfactor) >= _num_nodes ) {
            // leaf level, assign back-ends
            avail_iter = be_hostids.begin();
            avail_end = be_hostids.end();
        }

        for( ; (used_iter != stop_iter) && (curr_step_nodes < _num_nodes); used_iter++) {
            vector< string >& children = tree[ *used_iter ];
            unsigned int i = 0;
            for( ; (i < (_kfactor-1)) && (avail_iter != avail_end) && (curr_step_nodes < _num_nodes); 
                 i++, avail_iter++, curr_step_nodes++ ) {
                children.push_back( *avail_iter );
            }
            
            if( avail_iter == avail_end ) {
                if( i < (_kfactor-1) ) {
                    if( avail_end == int_hostids.end() )
                        fprintf( stderr, "Not enough internal hosts(%" PRIszt") for topology %s\n", 
                                 int_hostids.size(), _topology_spec.c_str() );
                    else
                        fprintf( stderr, "Not enough back-end hosts(%" PRIszt") for topology %s\n", 
                                 be_hostids.size(), _topology_spec.c_str() );
                    return false;
                }
            }
        }
    }
    if( curr_step_nodes < _num_nodes ) {
        fprintf( stderr, "Not enough internal(%" PRIszt") and back-end(%" PRIszt") hosts for topology %s\n", 
                 int_hostids.size(), be_hostids.size(), _topology_spec.c_str() );
        return false;
    }
    

    _depth = max_depth;
    _num_leaves = _num_nodes - (unsigned int)tree.size();

    // Connect tree
    map< string, vector< string > >::iterator parent = tree.begin();
    for( ; parent != tree.end() ; parent++ ) {
       Tree::Node* pnode = get_Node( parent->first );
       
       vector< string >& children = parent->second;
       vector< string >::iterator child = children.begin();
       for( ; child != children.end(); child++ )
          pnode->add_Child( get_Node(*child) );
    }

    return validate();
}

bool KnomialTree::initialize_Tree( list< pair<string,unsigned> > & hosts )
{
    map< string, vector<string> > tree;
    
    char cur_host[HOST_NAME_MAX];
    list< string > hostids;
   
    // treat root as internal for purposes of this algorithm
    snprintf( cur_host, sizeof(cur_host), "%s:%u",
              _fe_host.c_str(), 0 );
    hostids.push_back( cur_host );

    // get locations
    list< pair<string,unsigned> >::iterator liter = hosts.begin();
    for( ; liter != hosts.end() ; liter++ ) {
	unsigned u = 0;
        if( liter->first == _fe_host ) u++;
        for( ; u < liter->second && u < _int_procs_per_host; u++ ) {
            snprintf( cur_host, sizeof(cur_host), "%s:%u",
                      liter->first.c_str(), u );
            hostids.push_back( cur_host );
        }
    }
    if( hostids.size() < _num_nodes ) {
        fprintf( stderr, "Not enough hosts(%" PRIszt") for topology %s\n", 
                 hostids.size(), _topology_spec.c_str() );
        return false;
    }

    /* Algorithm: Host list treated as two sub-lists, USED and AVAIL. USED is
                  initialized to the first host (the root), AVAIL to the rest.
                  While we still have available hosts and haven't reached the 
                  desired total number of nodes, each member of USED will be
                  assigned k-1 new children from AVAIL. After each round of 
                  adding children from AVAIL to the members of USED, the size
                  of USED is effectively multiplied by k.
    */

    unsigned curr_step_nodes = 1;
    unsigned max_depth = 0;

    list< string >::iterator avail_iter = hostids.begin();
 
    root = get_Node( *avail_iter );
    avail_iter++;

    while( avail_iter != hostids.end() && 
           curr_step_nodes < _num_nodes ) {

        max_depth++;

        list< string >::iterator used_iter = hostids.begin();
        list< string >::iterator stop_iter = avail_iter;

        for( ; (used_iter != stop_iter) && (curr_step_nodes < _num_nodes); used_iter++) {
            vector< string >& children = tree[ *used_iter ];
            for(unsigned int i=0; (i < _kfactor-1) && 
                                  (avail_iter != hostids.end()) && 
                                  (curr_step_nodes < _num_nodes) ; 
                i++, avail_iter++, curr_step_nodes++) {
                children.push_back( *avail_iter );
            }
        }

    }

    _depth = max_depth;
    _num_leaves = _num_nodes - (unsigned int)tree.size();

    // Connect tree
    map< string, vector< string > >::iterator parent = tree.begin();
    for( ; parent != tree.end() ; parent++ ) {
       Tree::Node* pnode = get_Node( parent->first );
       
       vector< string >& children = parent->second;
       vector< string >::iterator child = children.begin();
       for( ; child != children.end(); child++ )
          pnode->add_Child( get_Node(*child) );
    }

    return validate();
}

GenericTree::GenericTree( string & topology_spec,
                          string & fe_host,
                          list< pair<string,unsigned> > & hosts,
                          unsigned int max_procs_per_host /* =1 */ ) 
    : Tree( fe_host, topology_spec, max_procs_per_host, max_procs_per_host )
{
    if( parse_Spec() )
        _valid = initialize_Tree( hosts );
}

GenericTree::GenericTree( string & topology_spec,
                          string & fe_host,
                          list< pair<string,unsigned> > & backend_hosts,
                          list< pair<string,unsigned> > & internal_hosts,
                          unsigned int be_procs_per_host /* =1 */,
                          unsigned int int_procs_per_host /* =1 */ )
    : Tree( fe_host, topology_spec, be_procs_per_host, int_procs_per_host )
{
    if( parse_Spec() )
        _valid = initialize_Tree( backend_hosts, internal_hosts );
}

bool GenericTree::parse_Spec( )
{
    bool new_child_spec=false, new_level=false, first_time=true;
    const char * cur_pos_ptr;
    char cur_item[16];
    unsigned int cur_item_pos=0, cur_num_children;
    unsigned int child_spec_multiplier=1;
    
    _children_by_level.reserve(10);
    vector< unsigned int >* cur_level_num_children = new vector< unsigned int >;
    cur_level_num_children->reserve(64);
    _children_by_level.push_back( cur_level_num_children );
 
    for( cur_pos_ptr = _topology_spec.c_str(); /*NULLCOND*/; cur_pos_ptr++ ) {

        if( *cur_pos_ptr == ',' ) {
            cur_item[ cur_item_pos ] = '\0';
            new_child_spec=true;
            cur_item_pos=0;
        }
        else if( *cur_pos_ptr == ':' || *cur_pos_ptr == '\0' ) {
            cur_item[ cur_item_pos ] = '\0';
            new_child_spec=true;
            new_level=true;
            cur_item_pos=0;
        }
        else if( *cur_pos_ptr == 'x' ) {
            cur_item[ cur_item_pos ] = '\0';
            cur_item_pos=0;
            child_spec_multiplier = atoi( cur_item );
        }
        else {
            if( ! isdigit( *cur_pos_ptr ) ) {
                fprintf(stderr, "Invalid character '%c' in topology "
                        "specification \"%s\".\n",
                        *cur_pos_ptr, _topology_spec.c_str() );
                return false;
            }
            cur_item[ cur_item_pos++ ] = *cur_pos_ptr;
        }

        if( new_child_spec || new_level ) {
            cur_num_children = atoi( cur_item );
            cur_item_pos = 0;
            new_child_spec = false;

            for(unsigned int i=0; i < child_spec_multiplier; i++)
                cur_level_num_children->push_back( cur_num_children );

            child_spec_multiplier=1;

            if( new_level ) {

                if( first_time ) {
                    if( cur_level_num_children->size() != 1 ) {
                        fprintf(stderr, "Error: Bad topology \"%s\". First level "
                                "of topology tree should only have one child spec\n",
                                _topology_spec.c_str() );
                        return false;
                    }
                    first_time = false;
                }

                if( *cur_pos_ptr == '\0' )
                    break;

                cur_level_num_children = new vector< unsigned int >;
                cur_level_num_children->reserve(64);
                _children_by_level.push_back( cur_level_num_children );
 
                new_level = false;
            }
        }
    }

    return true;
}

bool GenericTree::initialize_Tree( list< pair<string,unsigned> > & backend_hosts,
                                   list< pair<string,unsigned> > & internal_hosts )
{
    map< string, unsigned > host_proc_counts;
    vector< Tree::Node *> cur_level_nodes, next_level_nodes;

    unsigned int cur_num_children;
    Tree::Node *cur_node, *next_child;

    char cur_host[HOST_NAME_MAX];
    list< string > int_hostids;
    list< string > be_hostids;

    if( internal_hosts.empty() && (_children_by_level.size() > 1) ) {
        fprintf( stderr, "Input internal host list is empty. Not enough hosts for topology %s\n",
                 _topology_spec.c_str() );
        return false;
    }

    // account for root
    host_proc_counts[ _fe_host ] = 1;

    // get internal and backend locations
    list< pair<string,unsigned> >::iterator liter = internal_hosts.begin();
    for( ; liter != internal_hosts.end() ; liter++ ) {

        if( host_proc_counts.find(liter->first) == host_proc_counts.end() )
            host_proc_counts[ liter->first ] = 0;

        unsigned& cnt = host_proc_counts[ liter->first ];
        for( unsigned u=0; u < liter->second && u < _int_procs_per_host; u++, cnt++ ) {
            snprintf( cur_host, sizeof(cur_host), "%s:%u",
                      liter->first.c_str(), cnt );
            int_hostids.push_back( cur_host );
        }
    }
    liter = backend_hosts.begin();
    for( ; liter != backend_hosts.end() ; liter++ ) {
  
        if( host_proc_counts.find(liter->first) == host_proc_counts.end() )
            host_proc_counts[ liter->first ] = 0;

        unsigned& cnt = host_proc_counts[ liter->first ];
        for( unsigned u=0; u < liter->second && u < _be_procs_per_host; u++, cnt++ ) {
            snprintf( cur_host, sizeof(cur_host), "%s:%u",
                      liter->first.c_str(), cnt );
            be_hostids.push_back( cur_host );
        }
    }
    size_t nhosts = int_hostids.size() + be_hostids.size();
    
    // root is parent for first level
    snprintf( cur_host, sizeof(cur_host), "%s:0",
              _fe_host.c_str());
    next_level_nodes.push_back( get_Node(cur_host) );
    
    // at each internal level, add proper number of children to each parent node
    unsigned num_levels = (unsigned int)_children_by_level.size();
    list< string >::iterator avail_iter = int_hostids.begin();
    for( unsigned u=0; u < (num_levels - 1); u++ ) {

        cur_level_nodes = next_level_nodes;
        next_level_nodes.clear();

        vector< unsigned int >& level_children = *(_children_by_level[u]);
        for( unsigned int i=0; i < level_children.size(); i++ ) {

            cur_num_children = level_children[ i ];

            cur_node = cur_level_nodes[ i ];

            for( unsigned int j=0; j < cur_num_children; j++ ) {
                if( avail_iter == int_hostids.end() ) {
                    fprintf( stderr, "Not enough hosts(%" PRIszt") for topology %s\n", 
                             nhosts, _topology_spec.c_str() );
                    return false;
                }
                next_child = get_Node( *avail_iter );
                avail_iter++;
                cur_node->add_Child( next_child );
                next_level_nodes.push_back( next_child );
            }
        }
    }

    // add backends to leaf internal nodes
    avail_iter = be_hostids.begin();
    cur_level_nodes = next_level_nodes;
    next_level_nodes.clear();

    vector< unsigned int >& level_children = *(_children_by_level[num_levels-1]);
    for( unsigned int i=0; i < level_children.size(); i++ ) {

        cur_num_children = level_children[ i ];

        cur_node = cur_level_nodes[ i ];

        for( unsigned int j=0; j < cur_num_children; j++ ) {
            if( avail_iter == be_hostids.end() ) {
                fprintf( stderr, "Not enough hosts(%" PRIszt") for topology %s\n", 
                         int_hostids.size() + be_hostids.size(), _topology_spec.c_str() );
                return false;
            }
            next_child = get_Node( *avail_iter );
            avail_iter++;
            cur_node->add_Child( next_child );
        }
    }

    return validate();
}

bool GenericTree::initialize_Tree( list< pair<string,unsigned> > & hosts )
{
    unsigned int cur_num_children;
    vector< Tree::Node* > cur_level_nodes, next_level_nodes;
    Tree::Node *cur_node, *next_child;

    char cur_host[HOST_NAME_MAX];
    list< string > hostids;
    list< pair<string,unsigned> >::iterator liter = hosts.begin();
    for( ; liter != hosts.end() ; liter++ ) {
        unsigned u = 0;
        if( liter->first == _fe_host ) u++;
        for( ; u < liter->second && u < _int_procs_per_host; u++ ) {
            snprintf( cur_host, sizeof(cur_host), "%s:%u",
                      liter->first.c_str(), u );
            hostids.push_back( cur_host );
        }
    }

    list< string >::iterator avail_iter = hostids.begin();

    // root is parent for first level
    snprintf( cur_host, sizeof(cur_host), "%s:0",
              _fe_host.c_str());
    next_level_nodes.push_back( get_Node(cur_host) );
    
    // at each level, add proper number of children to each parent node
    for( unsigned u=0; u < _children_by_level.size(); u++ ) {

        cur_level_nodes = next_level_nodes;
        next_level_nodes.clear();

        vector< unsigned int >& level_children = *(_children_by_level[u]);
        for( unsigned int i=0; i < level_children.size(); i++ ) {

            cur_num_children = level_children[ i ];

            cur_node = cur_level_nodes[ i ];

            for( unsigned int j=0; j < cur_num_children; j++ ) {
                if( avail_iter == hostids.end() ) {
                    fprintf( stderr, "Not enough hosts(%" PRIszt") for topology %s\n", 
                             hostids.size(), _topology_spec.c_str() );
                    return false;
                }
                next_child = get_Node( *avail_iter );
                avail_iter++;
                cur_node->add_Child( next_child );
                next_level_nodes.push_back( next_child );
            }
        }
        

    }

    return validate();
}

GenericTree::~GenericTree()
{
    for( unsigned u=0; u < _children_by_level.size(); u++ )
        delete _children_by_level[u];
    _children_by_level.clear();
}

} /* namespace MRN */
