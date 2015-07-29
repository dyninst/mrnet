/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <sstream>
#include <set>
#include <vector>
#include <time.h>

#ifndef os_windows
#include "mrnet_config.h"
#endif
#include "FailureManagement.h"
#include "Router.h"
#include "SerialGraph.h"
#include "utils.h"
#include "mrnet/MRNet.h"
#include "xplat/Tokenizer.h"

using namespace std;

namespace MRN
{

/***************************************************
 * NetworkTopology
 **************************************************/

//const float NetworkTopology::Node::_depth_weight=0.7;
//const float NetworkTopology::Node::_proximity_weight=0.3;

NetworkTopology::Node::Node( const string & ihostname, Port iport, Rank irank,
                             bool iis_backend )
    : _hostname(ihostname), _port(iport), _rank(irank), _failed(false), _parent(NULL),
      _is_backend(iis_backend), _depth(0), _subtree_height(0), _adoption_score(0)
{
    static bool first_time=true;

    if ( first_time ) {
        first_time = false;
        srand48( time(NULL) );
    }
    _rand_key = drand48();
}

bool NetworkTopology::Node::failed(void) const 
{
    return _failed;
}

string NetworkTopology::Node::get_HostName(void) const
{
    return _hostname;
}

Port NetworkTopology::Node::get_Port(void) const
{
    return _port;
}

void NetworkTopology::Node::set_Port(Port port)
{
    _port = port;
}    

Rank NetworkTopology::Node::get_Rank(void) const
{
    return _rank;
}

const set< NetworkTopology::Node * > &
NetworkTopology::Node::get_Children(void) const
{
    return _children;
}

unsigned int NetworkTopology::Node::get_NumChildren(void) const
{
    return (unsigned int)_children.size();
}
        
void NetworkTopology::Node::set_Parent( Node * p )
{
    _parent = p;
}

Rank NetworkTopology::Node::get_Parent( void) const
{
    return _parent->get_Rank();
}

void NetworkTopology::Node::add_Child( Node * c )
{
    
    mrn_dbg( 5, mrn_printf( FLF, stderr, "Adding child %s to parent %s\n",
                           c->get_HostName().c_str(), this->get_HostName().c_str() ) );
    _is_backend=false;
    _children.insert(c);
}

bool NetworkTopology::Node::is_BackEnd(void) const
{
    return _is_backend;
}

double NetworkTopology::Node::get_WRSKey(void) const
{
    return _wrs_key;
}

double NetworkTopology::Node::get_RandKey(void) const
{
    return _rand_key;
}

double NetworkTopology::Node::get_AdoptionScore(void) const
{
    return _adoption_score;
}

unsigned int NetworkTopology::Node::get_DepthIncrease( Node * iorphan )
const 
{
    unsigned int depth_increase=0;

    if( iorphan->_subtree_height + 1 > _subtree_height  ) {
        depth_increase = iorphan->_subtree_height + 1 - _subtree_height;
    }

    mrn_dbg(5, mrn_printf( FLF, stderr,
                           "\n\tOrphan[%s:%d] sub tree height: %d\n"
                           "\tNode:[%s:%d] sub tree height: %d.\n"
                           "\tDepth increase: %u\n",
                           iorphan->_hostname.c_str(), iorphan->_rank,
                           iorphan->_subtree_height,
                           _hostname.c_str(), _rank, _subtree_height,
                           depth_increase ));

    return depth_increase;
}

unsigned int NetworkTopology::Node::get_Proximity( Node *iorphan )
{
    set<Node*>::const_iterator iter;

    mrn_dbg(5, mrn_printf( FLF, stderr,
                           "Computing proximity: [%s:%d] <-> [%s:%d] ...\n",
                           iorphan->_hostname.c_str(), iorphan->_rank,
                           _hostname.c_str(), _rank ));

    //Find closest common ascendant to both current node and orphan
    Node * cur_ascendant = this;
    Node * common_ascendant = NULL;
    unsigned int node_ascendant_distance=0;
    do{
        mrn_dbg(5, mrn_printf( FLF, stderr,
                               "Is \"%s:%d\" a common ascendant? ", 
                               cur_ascendant->_hostname.c_str(),
                               cur_ascendant->_rank ));
        if( iorphan->_ascendants.find( cur_ascendant ) 
            != iorphan->_ascendants.end() ) {
            common_ascendant = cur_ascendant;
            mrn_dbg(5, mrn_printf(0,0,0, stderr, "yes!\n"));
            break;
        }
        mrn_dbg(5, mrn_printf(0,0,0, stderr, "no.\n"));
        cur_ascendant = cur_ascendant->_parent;
        node_ascendant_distance++;
    } while( (common_ascendant == NULL) && (cur_ascendant != NULL) );

    if( common_ascendant == NULL ) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "no common ascendant found\n"));
        return (unsigned int)-1;
    }

    //Find distance between orphan and common ascendant
    cur_ascendant = iorphan;
    unsigned int orphan_ascendant_distance=0;
    mrn_dbg(5, mrn_printf( FLF, stderr, "Ascend from orphan to common ascendant[%s:%d]:\n"
                           "\t[%s:%d]", 
                           common_ascendant->_hostname.c_str(),
                           common_ascendant->_rank,
                           cur_ascendant->_hostname.c_str(),
                           cur_ascendant->_rank ));

    while( cur_ascendant != common_ascendant ) {
        cur_ascendant = cur_ascendant->_parent;
        mrn_dbg(5, mrn_printf( 0,0,0, stderr, " <- [%s:%d]",
                               cur_ascendant->_hostname.c_str(),
                               cur_ascendant->_rank ));
        orphan_ascendant_distance++;
    }

    mrn_dbg(5, mrn_printf( 0,0,0, stderr, "\n\n"));
    mrn_dbg(5, mrn_printf( FLF, stderr,
                           "\t[%s:%d]<->[%s:%d]: %d\n\t[%s:%d]<->[%s:%d]: %d\n",
                           common_ascendant->_hostname.c_str(),
                           common_ascendant->_rank, _hostname.c_str(), _rank,
                           node_ascendant_distance,
                           common_ascendant->_hostname.c_str(),
                           common_ascendant->_rank,
                           iorphan->_hostname.c_str(), iorphan->_rank,
                           orphan_ascendant_distance ));

    return node_ascendant_distance + orphan_ascendant_distance;
}

unsigned int NetworkTopology::Node::find_SubTreeHeight(void)
{
    if( _children.empty() ) {
        _subtree_height = 0;
    }
    else{
        unsigned int max_height=0, cur_height;

        set < Node * > ::iterator iter;
        for( iter=_children.begin(); iter!=_children.end(); iter++ ){
            cur_height = (*iter)->find_SubTreeHeight( );
            max_height = (cur_height > max_height ? cur_height : max_height);
        }
        _subtree_height = max_height+1;
    }

    return _subtree_height;
}

void NetworkTopology::compute_AdoptionScores( vector<Node*> iadopters,
                                              Node *iorphan )
{
    compute_TreeStatistics();

    //fprintf( stdout, "computing scores for %u adopters\n", iadopters.size() );
    for( unsigned int i=0; i<iadopters.size(); i++ ) {
        iadopters[i]->compute_AdoptionScore( iorphan, _min_fanout, _max_fanout,
                                             _depth );
    }
}

const float WFANOUT=1.0;
const float WDEPTH=1.0;
const float WPROXIMITY=0.5;
void NetworkTopology::Node::compute_AdoptionScore( Node * iorphan,
                                                   unsigned int imin_fanout,
                                                   unsigned int imax_fanout,
                                                   unsigned int idepth )
{
    mrn_dbg(5, mrn_printf( FLF, stderr,
                           "Computing [%s:%d]'s score for adopting [%s:%d] ...\n",
                           _hostname.c_str(), _rank,
                           iorphan->_hostname.c_str(),
                           iorphan->_rank ));
    //fprintf( stdout, "Computing [%s:%d]'s score for adopting [%s:%d] ...\n",
             //_hostname.c_str(), _rank,
             //iorphan->_hostname.c_str(),
             //iorphan->_rank );
    
    unsigned int depth_increase = get_DepthIncrease( iorphan );
    unsigned int proximity = get_Proximity( iorphan );
    unsigned int fanout = (unsigned int)_children.size();

    double fanout_score;

    if( imax_fanout == imin_fanout )
        fanout_score = 1;
    else
        fanout_score = (double)(imax_fanout - fanout) /
            (double)(imax_fanout - imin_fanout);
    
    double depth_increase_score = (double)( (idepth-1) - depth_increase ) /
        (double)(idepth-1);

    double proximity_score = (double)( 2 * idepth - 1 - proximity ) /
        (double)( 2 * idepth - 1 - 2 ) ;

    _adoption_score =
        (WFANOUT * fanout_score  ) +
        (WDEPTH * depth_increase_score ) +
        (WPROXIMITY * proximity_score );

    //_wrs_key = pow( _rand_key, 1/_adoption_score );
    _wrs_key = pow( drand48(), 1/_adoption_score );

    //fprintf(stdout, "[%s:%d]: (%u: %.2lf) + (%u: %.2lf) (%u: %.2lf): %.2lf (key:%.2lf)\n",
            //_hostname.c_str(), _rank,
            //fanout, fanout_score,
            //depth_increase, depth_increase_score,
            //proximity, proximity_score,
            //_adoption_score, _wrs_key );
    //fprintf( stdout, "\t max_fan: %u, min_fan: %u, depth: %u\n", imin_fanout, imax_fanout, idepth );
}

bool NetworkTopology::Node::remove_Child( NetworkTopology::Node * c ) 
{
    _children.erase( c );

    return true;
}


bool NetworkTopology::new_Node( const string &host, Port port, 
                                Rank rank, bool is_backend )
{
    mrn_dbg( 5, mrn_printf( FLF, stderr, "Creating back node[%d] %s:%d\n",
                            rank, host.c_str(), port ) );
    Node* node = new Node( host, port, rank, is_backend);
    _nodes[ rank ] = node;
    
    if( is_backend ) {
        string host_copy = host;
        mrn_dbg( 5, mrn_printf( FLF, stderr, "Adding node[%d] as backend\n", rank ) );
        _backend_nodes.insert( node );
        _network->insert_EndPoint( host_copy, port, rank );

    }
    return true;
}   


NetworkTopology::NetworkTopology( Network *inetwork,  string &ihostname, Port iport, 
                                  Rank irank, bool iis_backend /*=false*/ )
    : _network(inetwork),
      _root( new Node( ihostname, iport, irank, iis_backend ) ),
      _router( new Router( inetwork ) ),
      _serial_graph(NULL)
{
    _nodes[ irank ] = _root;
    mrn_dbg( 3, mrn_printf(FLF, stderr,
                           "Added rank %u to node list (%p) size: %u\n",
                           irank, &_nodes, _nodes.size() ));
}

NetworkTopology::NetworkTopology( Network *inetwork, SerialGraph & isg )
    : _network(inetwork), _root( NULL ), _router( new Router( inetwork ) ),
      _serial_graph(NULL)
{
    string sg_str = isg.get_ByteArray();
    //fprintf(stderr, "Resetting topology to \"%s\"\n", sg_str.c_str() );
    reset( sg_str );
}

NetworkTopology::~NetworkTopology(void)
{
    string empty(NULL_STRING);
    reset( empty, false ); // deletes the nodes

    _sync.Lock();

    _network = NULL;
    if( _serial_graph != NULL ) {
        delete _serial_graph;
        _serial_graph = NULL;
    }
    if( _router != NULL ) {
        delete _router;
        _router = NULL;
    }

    _sync.Unlock();
}

void NetworkTopology::remove_SubGraph( Node * inode )
{
    // assumes we are holding the lock
    mrn_dbg_func_begin();

    //remove all children subgraphs
    set < Node * > ::iterator iter;
    for( iter=inode->_children.begin(); iter!=inode->_children.end(); iter++ ){
        remove_SubGraph( *iter );
        remove_Node( *iter );
    }
    inode->_children.clear();

    mrn_dbg_func_end();
}

bool NetworkTopology::add_SubGraph( Rank irank, SerialGraph & isg, bool iupdate /*=false*/)
{
    _sync.Lock();
    bool retval;

    mrn_dbg( 3, mrn_printf( FLF, stderr, "Node[%d] adding subgraph \"%s\"\n",
                            irank, isg.get_ByteArray().c_str() ));

    Node * node = find_Node( irank );

    if( node == NULL ) {
        retval = false;
    }
    else {
        retval = add_SubGraph( node, isg, false );
        if( iupdate )
            update_Router_Table();
    }

    _sync.Unlock();
    return retval;
}

void NetworkTopology::update_Router_Table( )
{
    mrn_dbg_func_begin();
    _sync.Lock();
    _router->update_Table();
    _sync.Unlock();
    mrn_dbg_func_end();
}

bool NetworkTopology::add_SubGraph( Node * inode, SerialGraph & isg, bool iupdate )
{
    // assumes we are holding the lock
    mrn_dbg_func_begin();

    _parent_nodes.insert( inode );
 
    //search for root of subgraph
    Node * node = find_NodeHoldingLock( isg.get_RootRank() );
    if( node == NULL ) {
        //Not found! allocate
        string node_type = ( isg.is_RootBackEnd() ? "backend" : "internal" );
        mrn_dbg( 5, mrn_printf( FLF, stderr, "Creating %s node[%d] %s:%d\n",
                                node_type.c_str(), isg.get_RootRank(), 
                                isg.get_RootHostName().c_str(), 
                                isg.get_RootPort() ) );
        node = new Node( isg.get_RootHostName(), isg.get_RootPort(), 
                         isg.get_RootRank(), isg.is_RootBackEnd() );
        _nodes[ isg.get_RootRank() ] = node;
    }

    if( node->is_BackEnd() ) {
        string name = node->get_HostName();
        Rank r = node->get_Rank();
        set< Node* >::iterator niter = _backend_nodes.find( node );
        if( niter == _backend_nodes.end() ) {
            mrn_dbg( 5, mrn_printf( FLF, stderr, "Adding node[%d] as backend\n", r ));
            _backend_nodes.insert( node );
            _network->insert_EndPoint( name, node->get_Port(), r );
        }
    }

    mrn_dbg( 5, mrn_printf( FLF, stderr, "Adding node[%d] as child of node[%d]\n",
                            node->get_Rank(), inode->get_Rank() ));
    //add root of new subgraph as child of input node
    set_Parent( node->get_Rank(), inode->get_Rank(), iupdate );

    SerialGraph *cur_sg;
    isg.set_ToFirstChild( );
    for( cur_sg = isg.get_NextChild( ); cur_sg; cur_sg = isg.get_NextChild( ) ) {
        add_SubGraph( node, *cur_sg, iupdate );
    }

    mrn_dbg_func_end();
    return true;
}

NetworkTopology::Node * NetworkTopology::find_Node( Rank irank ) const
{
    NetworkTopology::Node* ret = NULL;
    _sync.Lock();
    ret = find_NodeHoldingLock( irank );
    _sync.Unlock();
    return ret;
}


NetworkTopology::Node* NetworkTopology::find_NodeHoldingLock( Rank irank ) const
{
    // assumes we are holding the lock
    map< Rank, NetworkTopology::Node* >::const_iterator iter =
        _nodes.find( irank );

    if( iter == _nodes.end() )
        return NULL;

    return (*iter).second;
}


bool NetworkTopology::remove_Node( NetworkTopology::Node *inode )
{
    // we better be holding the lock!!

    mrn_dbg_func_begin();

    if( _root == inode ){
        _root=NULL;
    }

    //remove from orphans, back-ends, parents list
    _nodes.erase( inode->_rank );
    _orphans.erase( inode );

    if( inode->is_BackEnd() ) {
        _backend_nodes.erase( inode );
        _network->remove_EndPoint( inode->_rank );
    }
    else
        _parent_nodes.erase( inode );


    //remove node as its children's parent, and set children as orphans
    set < Node * >::iterator iter;
    for( iter=inode->_children.begin(); iter!=inode->_children.end(); iter++ ){
        if( (*iter)->_parent == inode ) {
            (*iter)->set_Parent( NULL );
            _orphans.insert( (*iter) );
        }
    }

    delete( inode );

    mrn_dbg_func_end();
    return true;
}

bool NetworkTopology::remove_Node( Rank irank, bool iupdate /* = false */ )
{
    _sync.Lock();

    NetworkTopology::Node *node_to_remove = find_Node( irank );
    if( node_to_remove == NULL ){
        _sync.Unlock();
        return false;
    }

    node_to_remove->_failed = true;

    //remove node as parent's child
    if( node_to_remove->_parent )
        node_to_remove->_parent->remove_Child( node_to_remove );

    bool retval = remove_Node( node_to_remove );

    if( iupdate )
        update_Router_Table();

    _sync.Unlock();
    return retval;
}

bool NetworkTopology::set_Parent( Rank ichild_rank, Rank inew_parent_rank, bool iupdate /*=false*/ )
{
    mrn_dbg_func_begin();
    _sync.Lock();

    NetworkTopology::Node *child_node = find_NodeHoldingLock( ichild_rank );
    if( child_node == NULL ){
        _sync.Unlock();
        mrn_dbg_func_end();
        return false;
    }

    NetworkTopology::Node *new_parent_node = find_NodeHoldingLock( inew_parent_rank );
    if( new_parent_node == NULL ){
        _sync.Unlock();
        mrn_dbg_func_end();
        return false;
    }

    if( child_node->_parent != NULL ) {
        if( child_node->_parent == new_parent_node ) {
            _sync.Unlock();
            mrn_dbg_func_end();
            return true;
        }
        child_node->_parent->remove_Child( child_node );
    }

    child_node->set_Parent( new_parent_node );
    new_parent_node->add_Child( child_node );
    remove_Orphan( child_node->get_Rank() );

    child_node->_ascendants = new_parent_node->_ascendants;
    child_node->_ascendants.insert( new_parent_node );

    if( iupdate )
        update_Router_Table();

    _sync.Unlock();
    mrn_dbg_func_end();
    return true;
}

bool NetworkTopology::remove_Orphan( Rank r )
{
    // assumes we are holding the lock
    NetworkTopology::Node * node = find_NodeHoldingLock(r);
    if( !node )
        return false;

    _orphans.erase( node );

    return true;
}

void NetworkTopology::print_DOTSubTree( NetworkTopology::Node * inode, FILE * f ) const
{
    set < NetworkTopology::Node * >::iterator iter;

    string parent_str, child_str;
    char rank_str[128];

    sprintf(rank_str, "%u", inode->_rank );
    parent_str = inode->_hostname + string(":")
        + string(rank_str);

    //fprintf(stderr, "Printing node[%d]\n", inode->_rank );
    if( inode->_children.empty() ) {
        //fprintf(stderr, "\"%s\";\n", parent_str.c_str() );
        fprintf(f, "\"%s\";\n", parent_str.c_str() );
    }
    else {
        for( iter = inode->_children.begin();
             iter != inode->_children.end();
             iter++ ) {
            sprintf(rank_str, "%u", (*iter)->_rank );
            child_str = (*iter)->_hostname +
                string(":") + string(rank_str);
            fprintf(f, "\"%s\" -> \"%s\";\n", parent_str.c_str(),
                    child_str.c_str() );
            //fprintf(stderr, "\"%s\" -> \"%s\";\n", parent_str.c_str(),
                    //child_str.c_str() );
            print_DOTSubTree( *iter, f );
        }
    }
}

void NetworkTopology::print_TopologyFile( const char * filename ) const
{
    map < Rank, NetworkTopology::Node * >::const_iterator iter;
    set < NetworkTopology::Node * >::const_iterator iter2;

    FILE * f = fopen(filename, "w" );
    if( f == NULL ){
        perror("fopen()");
        return;
    }

    _sync.Lock();
    for( iter = _nodes.begin(); iter != _nodes.end(); iter++ ) {
        if( !(*iter).second->_children.empty() ) {
            fprintf( f, "%s:%u => \n", (*iter).second->get_HostName().c_str(),
                     (*iter).second->get_Rank() );

            for( iter2 = (*iter).second->_children.begin();
                 iter2 != (*iter).second->_children.end();
                 iter2++ ) {
                fprintf( f, "\t%s:%u\n", (*iter2)->get_HostName().c_str(),
                         (*iter2)->get_Rank() );
            }
            fprintf( f, "\t;\n\n" );
        }
    } 
    _sync.Unlock();
    fclose( f );
}

void NetworkTopology::print_DOTGraph( const char * filename ) const
{

    FILE * f = fopen(filename, "w" );
    if( f == NULL ){
        perror("fopen()");
        return;
    }

    fprintf( f, "digraph G{\n" );

    _sync.Lock();
    print_DOTSubTree( _root, f );

    set < NetworkTopology::Node * >::const_iterator iter;
    for( iter = _orphans.begin(); iter != _orphans.end(); iter++ ) {
        print_DOTSubTree( *iter, f);
    } 
    _sync.Unlock();
    fprintf( f, "}\n" );
    fclose( f );

}

void NetworkTopology::print( FILE * f ) const
{
    map < Rank, NetworkTopology::Node * >::const_iterator iter;

    string cur_parent_str, cur_child_str;
    char rank_str[128];

    mrn_dbg(5, mrn_printf(0,0,0, f, "\n***NetworkTopology***\n" ));

    _sync.Lock();

    for( iter = _nodes.begin(); iter != _nodes.end(); iter++ ) {
        NetworkTopology::Node * cur_node = (*iter).second;

        //Only print on lhs if root or have children
        if( cur_node == _root || !(cur_node->_children.empty()) ){
            sprintf(rank_str, "%u", cur_node->_rank);
            cur_parent_str = cur_node->_hostname + string(":")
                + string(rank_str);
            mrn_dbg(5, mrn_printf(0,0,0, f, "%s =>\n", cur_parent_str.c_str() ));

            set < Node * > ::iterator set_iter;
            for( set_iter=cur_node->_children.begin();
                 set_iter!=cur_node->_children.end(); set_iter++ ){
                sprintf(rank_str, "%u", (*set_iter)->_rank);
                cur_child_str = (*set_iter)->_hostname +
                    string(":") + string(rank_str);
                mrn_dbg(5, mrn_printf(0,0,0, f, "\t%s\n", cur_child_str.c_str() ) );
            }
            mrn_dbg(5, mrn_printf(0,0,0, f, "\n"));
        } 
    }

    _sync.Unlock();
}

std::string NetworkTopology::get_TopologyString(void)
{
    std::string topol;
    _sync.Lock();

    if( _serial_graph != NULL )
        delete _serial_graph;
    _serial_graph = new SerialGraph(NULL_STRING);

    serialize( _root );

    topol = _serial_graph->get_ByteArray();

    _sync.Unlock();

    return topol;
}

bool NetworkTopology::in_Topology( std::string ihostname, Port iport, Rank irank )
{
    bool found = false;
    _sync.Lock(); 
    Node* tmp = find_NodeHoldingLock(irank);
    if( NULL != tmp ) {
        if( ihostname.compare(tmp->_hostname) == 0 ) {
            if( (iport == UnknownPort) || 
                (tmp->_port == UnknownPort) || 
                (iport == tmp->_port) )
                found=true;
        } 
    }
    _sync.Unlock();
    return found;
}

std::string NetworkTopology::get_LocalSubTreeString(void)
{
    std::string retval;

    _sync.Lock();

    if( _serial_graph != NULL )
        delete _serial_graph;
    _serial_graph = new SerialGraph(NULL_STRING);
    serialize( _root );

    std::string localhost = _network->get_LocalHostName();
    SerialGraph* my_subgraph = _serial_graph->get_MySubTree( localhost,
                                                             _network->get_LocalPort(),
                                                             _network->get_LocalRank() );
    _sync.Unlock();

    if( my_subgraph != NULL ) {
        retval = my_subgraph->get_ByteArray();
        delete my_subgraph;
    }

    return retval;
}

void NetworkTopology::serialize(Node * inode)
{
    _sync.Lock();

    if( inode->is_BackEnd() ){
        // Leaf node, just add my name to serial representation and return
        _serial_graph->add_Leaf( inode->get_HostName(), inode->get_Port(),
                                 inode->get_Rank() );
        _sync.Unlock();
        return;
    }
    else{
        //Starting new sub-tree component in graph serialization:
        _serial_graph->add_SubTreeRoot( inode->get_HostName(), inode->get_Port(),
                                        inode->get_Rank() );
    }

    set < Node * > ::iterator iter;
    for( iter=inode->_children.begin(); iter!=inode->_children.end(); iter++ ){
         serialize( *iter );
    }	 

    //Ending sub-tree component in graph serialization:
    _serial_graph->end_SubTree();

    _sync.Unlock();
}

void NetworkTopology::get_BackEndNodes( set<NetworkTopology::Node*> &nodes ) const
{
    _sync.Lock();
    nodes = _backend_nodes;
    _sync.Unlock();
}

void NetworkTopology::get_ParentNodes( set<NetworkTopology::Node*> &nodes ) const
{
    _sync.Lock();
    nodes = _parent_nodes;
    _sync.Unlock();
}

void NetworkTopology::get_OrphanNodes( set<NetworkTopology::Node*> &nodes ) const
{
    _sync.Lock();
    nodes = _orphans;
    _sync.Unlock();
}

size_t NetworkTopology::num_BackEndNodes(void) const
{
    size_t n_be;
    _sync.Lock();
    n_be = _backend_nodes.size();
    _sync.Unlock();
    return n_be;
}

size_t NetworkTopology::num_InternalNodes(void) const
{
    size_t n_cp;
    _sync.Lock();
    n_cp = _nodes.size() - (_backend_nodes.size() + 1); // +1 for root
    _sync.Unlock();
    return n_cp;
}

struct lt_rank {
    bool operator()(const NetworkTopology::Node* n1, const NetworkTopology::Node* n2) const
    {
        return n1->get_Rank() < n2->get_Rank();
    }
};

struct lt_random {
    bool operator()(const NetworkTopology::Node* n1, const NetworkTopology::Node* n2) const
    {
        if( fabs(n1->get_RandKey() - n2->get_RandKey()) < .000001 )
            return n1->get_Rank() > n2->get_Rank();

        return n1->get_RandKey() > n2->get_RandKey();
    }
};

struct lt_wrs {
    bool operator()(const NetworkTopology::Node* n1, const NetworkTopology::Node* n2) const
    {
        if( fabs(n1->get_WRSKey() - n2->get_WRSKey()) < .000001 )
            return n1->get_Rank() > n2->get_Rank();
        return n1->get_WRSKey() > n2->get_WRSKey();
    }
};

struct lt_weight {
    bool operator()(const NetworkTopology::Node* n1, const NetworkTopology::Node* n2) const
    {
        if( fabs(n1->get_AdoptionScore() - n2->get_AdoptionScore()) < .000001 )
            return n1->get_Rank() > n2->get_Rank();
        return n1->get_AdoptionScore() > n2->get_AdoptionScore();
    }
};

NetworkTopology::Node * NetworkTopology::find_NewParent( Rank ichild_rank,
                                                         unsigned int inum_attempts,
                                                         ALGORITHM_T ialgorithm )
{
    static vector<Node*> potential_adopters;
    Node * adopter=NULL;

    mrn_dbg_func_begin();

    _sync.Lock();

    if( inum_attempts > 0 ) {
        //previously computed list, but failed to contact new parent

        if( inum_attempts >= potential_adopters.size() ) {
            return NULL;
        }

        adopter = potential_adopters[ inum_attempts ];

        mrn_dbg(5, mrn_printf(FLF, stderr, "Returning: node[%d]:%s:%d\n",
                                  adopter->get_Rank(),
                                  adopter->get_HostName().c_str(),
                                  adopter->get_Port() ));
        _sync.Unlock();
        return adopter;
    }

    Node * orphan = find_Node( ichild_rank );
    if( orphan == NULL ) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "Node for orphan is missing??\n"));
        _sync.Unlock();
        return NULL;
    }

    //compute list of potential adopters
    potential_adopters.clear();
    find_PotentialAdopters( orphan, _root, potential_adopters );
    if( potential_adopters.empty() ) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "No Adopters left :(\n"));
        _sync.Unlock();
        return NULL;
    }

    if( ialgorithm == ALG_RANDOM ) {
        //randomly choose a parent
        sort( potential_adopters.begin(), potential_adopters.end(), lt_random() );
        adopter = potential_adopters[0];
    }
    else if( ialgorithm == ALG_WRS ) {
        //use weighted random selection to choose a parent
        compute_AdoptionScores( potential_adopters, orphan );
        sort( potential_adopters.begin(), potential_adopters.end(), lt_wrs() );
        adopter = potential_adopters[0];
    }
    else if ( ialgorithm == ALG_SORTED_RR ) {
        //use sorted round robin to choose a parent
        compute_AdoptionScores( potential_adopters, orphan );
        sort( potential_adopters.begin(), potential_adopters.end(), lt_weight() );

        //get sorted vector of siblings
        vector< Node * > siblings;
        set< Node * >::const_iterator iter;
        for( iter=orphan->_parent->_children.begin(); 
             iter!=orphan->_parent->_children.end();  iter++ ){
            siblings.push_back( *iter );
        }
        sort( siblings.begin(), siblings.end(), lt_rank() );

        //find local rank in siblings list
        unsigned int my_idx=0;
        for( unsigned int i=0; i<siblings.size(); i++ ) {
            if( siblings[i]->get_Rank() == ichild_rank ) {
                my_idx = i;
                break;
            }
        }
        adopter = potential_adopters[ my_idx % potential_adopters.size() ];
    }


    //fprintf(stderr, "%u potential_adopters:\n", potential_adopters.size() );
    //for( unsigned int i=0; i<potential_adopters.size(); i++ ){
    //fprintf( stderr, "\tnode[%u]: k=%lf, score:%lf\n",
    //potential_adopters[i]->get_Rank(),
    //potential_adopters[i]->get_WRSKey(),
    //potential_adopters[i]->get_AdoptionScore() );
    //}

    if( adopter ) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "Returning: node[%d]:%s:%d\n",
                              adopter->get_Rank(),
                              adopter->get_HostName().c_str(),
                              adopter->get_Port() ));
    }

    _sync.Unlock();
    mrn_dbg_func_end();
    return adopter;
}

void NetworkTopology::find_PotentialAdopters( Node * iorphan,
                                              Node * ipotential_adopter,
                                              vector<Node*> &opotential_adopters )
{
    _sync.Lock();

    mrn_dbg(5, mrn_printf(FLF, stderr,
                          "Can Node[%d]:%s:%d (%s) adopt Orphan[%d]:%s:%d ...",
                          ipotential_adopter->get_Rank(),
                          ipotential_adopter->get_HostName().c_str(),
                          ipotential_adopter->get_Port(),
                          ( ipotential_adopter->is_BackEnd() ? "be" : "int" ),
                          iorphan->get_Rank(),
                          iorphan->get_HostName().c_str(),
                          iorphan->get_Port() ));
    
    //fprintf(stderr, "Can Node[%d]:%s:%d (%s) adopt Orphan[%d]:%s:%d ...",
            //ipotential_adopter->get_Rank(),
            //ipotential_adopter->get_HostName().c_str(),
            //ipotential_adopter->get_Port(),
            //( ipotential_adopter->is_BackEnd() ? "be" : "int" ),
            //orphan->get_Rank(),
            //orphan->get_HostName().c_str(),
            //orphan->get_Port() );
    //stop at backends or the orphaned node's parent
    if( ( iorphan->_parent == ipotential_adopter ) ||
        ipotential_adopter->is_BackEnd() ) {
        _sync.Unlock();
        mrn_dbg(5, mrn_printf(0,0,0, stderr, "no!\n" ));
        //fprintf(stderr, "no!\n" );
        return;
    }

    //fprintf(stderr, "yes!\n" );
    mrn_dbg(5, mrn_printf(0,0,0, stderr, "yes!\n" ));
    
    opotential_adopters.push_back( ipotential_adopter );

    set < Node * > ::iterator iter;
    for( iter=ipotential_adopter->_children.begin();
         iter!=ipotential_adopter->_children.end(); iter++ ){
        find_PotentialAdopters( iorphan, *iter, opotential_adopters );
    }

    _sync.Unlock();
}

bool NetworkTopology::reset( string itopology_str, bool iupdate /* =true */ )
{
    mrn_dbg( 5, mrn_printf( FLF, stderr, "Reseting topology to \"%s\"\n",
                            itopology_str.c_str() ));
    _sync.Lock();

    if( _serial_graph != NULL )
        delete _serial_graph;
    _serial_graph = new SerialGraph( itopology_str );
    _root = NULL;

    map<Rank, Node*>::iterator iter = _nodes.begin();
    for( ; iter != _nodes.end(); iter++ )
        delete iter->second ;

    _nodes.clear();
    _orphans.clear();
    _backend_nodes.clear();

    if( itopology_str == NULL_STRING ) {
        _sync.Unlock();
        return true;
    }

    _serial_graph->set_ToFirstChild();
    
    mrn_dbg( 5, mrn_printf( FLF, stderr, "Root: %s:%u:%hu\n",
                            _serial_graph->get_RootHostName().c_str(),
                            _serial_graph->get_RootRank(),
                            _serial_graph->get_RootPort() ));

    _root = new Node( _serial_graph->get_RootHostName(),
                      _serial_graph->get_RootPort(),
                      _serial_graph->get_RootRank(),
                      _serial_graph->is_RootBackEnd() );
    _nodes[ _serial_graph->get_RootRank() ] = _root;

#if 0
    if( _network )
        _network->set_FailureManager ( new CommunicationNode(_root->get_HostName(),
                                                             FAILURE_REPORTING_PORT,
                                                             UnknownRank) );
#endif     

    SerialGraph *cur_sg = _serial_graph->get_NextChild();
    for( ; cur_sg; cur_sg = _serial_graph->get_NextChild() ) {
        if( ! add_SubGraph(_root, *cur_sg, false) ){
            mrn_dbg( 1, mrn_printf(FLF, stderr, "add_SubTreeRoot() failed\n" ));
            _sync.Unlock();
            return false;
        }
        delete cur_sg;
    }

    if( iupdate ) {
        if( _network )
            update_Router_Table();
    }
    _sync.Unlock();

    return true;
}

void NetworkTopology::get_Leaves( vector< Node * > &oleaves ) const
{
    // A convenience function for helping with the BE attach case
    if( _root->get_NumChildren() == 0 ) {
        mrn_dbg(3, mrn_printf(FLF, stderr, "adding root node to leaves\n"));
        oleaves.push_back( _root );
    }
    else
        get_LeafDescendants( _root, oleaves ); 
}

void NetworkTopology::get_LeafDescendants( Node *inode,
                                           vector< Node * > &odescendants ) const
{
    _sync.Lock();

    set < Node * > ::iterator iter;
    for( iter=inode->_children.begin(); iter!=inode->_children.end(); iter++ ){
        if( (*iter)->get_NumChildren() )
            get_LeafDescendants( (*iter), odescendants );
        else {
            mrn_dbg(3, mrn_printf(FLF, stderr, "adding leaf node[%d] to descendants\n",
                                  (*iter)->get_Rank() ));
            odescendants.push_back( (*iter) );
        }
    }

    _sync.Unlock();
}

void NetworkTopology::get_Descendants( Node *inode,
                                       vector< Node * > &odescendants ) const
{
    _sync.Lock();

    set < Node * > ::iterator iter;
    for( iter=inode->_children.begin(); iter!=inode->_children.end(); iter++ ){
        mrn_dbg(3, mrn_printf(FLF, stderr, "adding node[%d] to descendants\n",
                              (*iter)->get_Rank() ));
        odescendants.push_back( (*iter) );

        get_Descendants( (*iter), odescendants );
    }

    _sync.Unlock();
}

unsigned int NetworkTopology::get_TreeDepth(void) const
{
    return _root->find_SubTreeHeight();
}

void NetworkTopology::compute_TreeStatistics(void)
{
    _sync.Lock();

    _max_fanout=0;
    _depth=0;
    _min_fanout=(unsigned int)-1;

    _depth=_root->find_SubTreeHeight( );

    set< Node * > ::const_iterator iter;
    for( iter=_parent_nodes.begin(); iter!=_parent_nodes.end(); iter++ ) {
        _max_fanout = ( (*iter)->_children.size() > _max_fanout ?
                        (unsigned int)(*iter)->_children.size() : _max_fanout );
        _min_fanout = ( (*iter)->_children.size() < _min_fanout ?
                        (unsigned int)(*iter)->_children.size() : _min_fanout );
    }

    _avg_fanout = ( (double)(_nodes.size()-1) ) / ( (double)_parent_nodes.size() );

    double diff=0,sum_of_square=0;
    for( iter=_parent_nodes.begin(); iter!=_parent_nodes.end(); iter++ ) {
        diff = _avg_fanout - (double)(*iter)->_children.size();
        sum_of_square += (diff*diff);
    }

    _var_fanout = sum_of_square / (double)_parent_nodes.size();    
    _stddev_fanout = sqrt( _var_fanout );

    _sync.Unlock();
}

void NetworkTopology::get_TreeStatistics( unsigned int &onum_nodes,
                                          unsigned int &odepth,
                                          unsigned int &omin_fanout, 
                                          unsigned &omax_fanout,
                                          double &oavg_fanout,
                                          double &ostddev_fanout )
{
    compute_TreeStatistics();

    _sync.Lock();

    onum_nodes = (unsigned int)_nodes.size();
    odepth = _depth;
    omax_fanout = _max_fanout;
    omin_fanout = _min_fanout;
    oavg_fanout = _avg_fanout;
    ostddev_fanout = _stddev_fanout;

    _sync.Unlock();
}

unsigned int NetworkTopology::get_NumNodes() const
{
    _sync.Lock();

    unsigned int curr_size = (unsigned int)_nodes.size();

    _sync.Unlock();

    return curr_size;
}

PeerNodePtr NetworkTopology::get_OutletNode( Rank irank ) const
{
    return _router->get_OutletNode( irank );
}

bool NetworkTopology::node_Failed( Rank irank ) const 
{
    Node * node = find_Node( irank );
    if( node == NULL  ) {
        mrn_dbg( 5, mrn_printf(FLF, stderr, 
                               "rank %u not found, assuming failed\n", irank) );
        return true;
    }

    return node->failed();
}

/***************************************************
 * TopologyLocalInfo
 **************************************************/

Rank TopologyLocalInfo::get_Rank() const
{
    if( local_node != NULL )
        return local_node->get_Rank();

    return UnknownRank;
}

unsigned int TopologyLocalInfo::get_NumChildren() const
{
    if( local_node != NULL )
        return local_node->get_NumChildren();

    return 0;
}

unsigned int TopologyLocalInfo::get_NumSiblings() const
{
    if( local_node != NULL ) {
        if( local_node->_parent != NULL )
            return local_node->_parent->get_NumChildren() - 1;
    }
    return 0;
}

unsigned int TopologyLocalInfo::get_NumDescendants() const
{
    if( (local_node != NULL) && (topol != NULL) ) {
        std::vector<NetworkTopology::Node*> descendants;
        topol->get_Descendants( local_node, descendants );
        return (unsigned int)descendants.size();
    }
    return 0;
}

unsigned int TopologyLocalInfo::get_NumLeafDescendants() const
{
    if( (local_node != NULL) && (topol != NULL) ) {
        std::vector<NetworkTopology::Node*> descendants;
        topol->get_LeafDescendants( local_node, descendants );
        return (unsigned int)descendants.size();
    }
    return 0;
}

unsigned int TopologyLocalInfo::get_RootDistance() const
{
    if( (local_node != NULL) && (topol != NULL) ) {
        unsigned int hops = 0;
        NetworkTopology::Node* curr = local_node;
        NetworkTopology::Node* root = topol->get_Root();
        while( curr != root ) {
            hops++;
            curr = curr->_parent;
        }
        return hops;
    }
    return 0;
}

unsigned int TopologyLocalInfo::get_MaxLeafDistance() const
{
    if( local_node != NULL )
        return local_node->find_SubTreeHeight();

    return 0;
}

const NetworkTopology* TopologyLocalInfo::get_Topology() const
{
    return topol;
}

const Network* TopologyLocalInfo::get_Network() const
{
    if( topol != NULL ) {
        return topol->_network;
    }
    return NULL;
}

/******** TOPOLOGY UPDATE METHODS ********/

bool NetworkTopology::send_updates_buffer()
{
    mrn_dbg_func_begin();

    if( _network->is_ShuttingDown() ) {
        _sync.Lock();
        _updates_buffer.clear();
        _sync.Unlock();
        mrn_dbg_func_end();
        return true;
    }

    _sync.Lock();
    unsigned int vuc_size = (unsigned int)_updates_buffer.size();
  
    if( vuc_size ) {
         
        Stream *s = _network->get_Stream( TOPOL_STRM_ID ); // topol prop stream
        if( s != NULL ) {

            int* type_arr = (int*) malloc( sizeof(int) * vuc_size );
            char** host_arr = (char**) malloc( sizeof(char*) * vuc_size );
            Rank* prank_arr = (Rank*) malloc( sizeof(Rank) * vuc_size );
            Rank* crank_arr = (Rank*) malloc( sizeof(Rank) * vuc_size );
            Port* cport_arr = (Port*) malloc( sizeof(Port) * vuc_size );
            
            int i = 0;
            std::vector< NetworkTopology::update_contents* >::iterator it;
            for( it = _updates_buffer.begin(); it != _updates_buffer.end() ; it++, i++ ) {
                type_arr[i] = (*it)->type;
                prank_arr[i] = (*it)->par_rank;
                host_arr[i] = (*it)->chld_host;
                crank_arr[i] = (*it)->chld_rank;
                cport_arr[i] = (*it)->chld_port;
                free( *it );
            } 
    
            //broadcast all topology updates
            mrn_dbg( 5, mrn_printf(FLF, stderr, "sending %d updates\n", vuc_size) );
            s->send( PROT_TOPO_UPDATE, "%ad %aud %aud %as %auhd", 
                     type_arr, vuc_size, 
                     prank_arr, vuc_size, 
                     crank_arr, vuc_size, 
                     host_arr, vuc_size, 
                     cport_arr, vuc_size );
            s->flush();

            _updates_buffer.clear();
            _sync.Unlock();

            free( type_arr );
            free( prank_arr );
            free( crank_arr );
            free( cport_arr );

            for( unsigned int u=0; u < vuc_size; u++ )
                free( host_arr[u] );
            free( host_arr );

            mrn_dbg_func_end();
            return true;
        }
    }
    _sync.Unlock();
    
    mrn_dbg_func_end();
    return false;
}

void NetworkTopology::insert_updates_buffer( update_contents* uc )
{
    _sync.Lock();
    _updates_buffer.push_back(uc);
    _sync.Unlock();
}

void NetworkTopology::update_addBackEnd( Rank par_rank, Rank chld_rank, 
                                         char* chld_host, Port chld_port, 
                                         bool upstream )
{
    if( _network->is_ShuttingDown() )
        return;

    mrn_dbg(5, mrn_printf(FLF, stderr, "Adding backend node[%d] as child of node[%d]\n",
                          chld_rank, par_rank));

    Node* n = find_Node( chld_rank );
    if (n == NULL) {

        // create node
        new_Node( chld_host, chld_port, chld_rank, true );

        // set its parent
        if( ! set_Parent(chld_rank, par_rank, false) ) {
            mrn_dbg(1, mrn_printf(FLF, stderr,
                                   "set parent for %s:%d failed\n",
                                   chld_host, chld_rank));
        }
        mrn_dbg(5, mrn_printf(FLF, stderr, "topology after add %s\n", 
                              get_TopologyString().c_str()));
    }
    else
        mrn_dbg(5, mrn_printf(FLF, stderr, "node already present\n"));

    // add it as endpoint for topology update strm
    Stream* topol_strm = _network->get_Stream( TOPOL_STRM_ID );
    if( topol_strm != NULL ) {
        topol_strm->add_Stream_EndPoint( chld_rank );
    }

    // FE: do callback only after state has been updated
    if( upstream && _network->is_LocalNodeFrontEnd() ) {
        update_contents* ub = (update_contents*) malloc( sizeof(update_contents) );
        ub->type = TOPO_NEW_BE;
        ub->chld_rank = chld_rank;
        ub->chld_port = chld_port;
        ub->chld_host = strdup(chld_host);
        ub->par_rank = par_rank;
        insert_updates_buffer( ub );

        TopologyEvent::TopolEventData* ted;
        ted = new TopologyEvent::TopolEventData( chld_rank, par_rank );
        TopologyEvent *te = new TopologyEvent( TopologyEvent::TOPOL_ADD_BE, ted );
        _network->_evt_mgr->add_Event( te );
    }    
}

void NetworkTopology::update_addInternalNode( Rank par_rank, Rank chld_rank, 
                                              char* chld_host, Port chld_port, 
                                              bool upstream )
{
    if( _network->is_ShuttingDown() )
        return;

    mrn_dbg( 5, mrn_printf(FLF, stderr, 
                           "Adding internal node[%d] as child of node[%d]\n",
                           chld_rank, par_rank) );

    Node* n = find_Node( chld_rank );
    if( n == NULL ) {

        // create node
        new_Node( chld_host, chld_port, chld_rank, false ); 

        // set its parent
        if( ! set_Parent(chld_rank, par_rank, false) ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "set parent for %s:%d failed\n", 
                                   chld_host, chld_rank) );
        }
        mrn_dbg( 5, mrn_printf(FLF, stderr, "topology after add: %s\n", 
                               get_TopologyString().c_str()) );
    }
    else
        mrn_dbg( 5, mrn_printf(FLF, stderr, "node already present in topology\n") );
    
    // FE: do callback only after state has been updated
    if( upstream && _network->is_LocalNodeFrontEnd() ) {
        update_contents* ub = (update_contents*) malloc( sizeof(update_contents) );
        ub->type = TOPO_NEW_CP;
        ub->chld_rank = chld_rank;
        ub->chld_port = chld_port;
        ub->chld_host = strdup(chld_host);
        ub->par_rank = par_rank;
        insert_updates_buffer( ub );

        TopologyEvent::TopolEventData* ted;
        ted = new TopologyEvent::TopolEventData( chld_rank, par_rank );
        TopologyEvent *te = new TopologyEvent( TopologyEvent::TOPOL_ADD_CP, ted );
        _network->_evt_mgr->add_Event( te );
    }
}

void NetworkTopology::update_removeNode( Rank par_rank, Rank failed_chld_rank, 
                                         bool upstream )
{
    if( _network->is_ShuttingDown() )
        return;

    mrn_dbg(5, mrn_printf(FLF, stderr, "Removing node[%d]\n", failed_chld_rank));

    if( par_rank == _network->get_LocalRank() )
        _network->remove_Node( failed_chld_rank, true );
    else
        remove_Node( failed_chld_rank, false );

    mrn_dbg( 5, mrn_printf(FLF, stderr, "topology after remove: %s\n", 
                               get_TopologyString().c_str()) );

    // FE: do callback only after state has been updated
    if( upstream && _network->is_LocalNodeFrontEnd() ) {
        
        update_contents* ub = (update_contents*) malloc( sizeof(update_contents) );
        ub->type = TOPO_REMOVE_RANK;
        ub->chld_rank = failed_chld_rank;
        ub->chld_port = UnknownPort;
        ub->chld_host = strdup(NULL_STRING); //ugh, this should be fixed
        ub->par_rank = par_rank;
        insert_updates_buffer( ub );
        
        TopologyEvent::TopolEventData* ted;
        ted = new TopologyEvent::TopolEventData( failed_chld_rank, par_rank );
        TopologyEvent *te = new TopologyEvent( TopologyEvent::TOPOL_REMOVE_NODE, 
                                               ted );
        _network->_evt_mgr->add_Event( te );
    }
}

void NetworkTopology::update_changeParent( Rank par_rank, Rank chld_rank, 
                                           bool upstream )
{
    if( _network->is_ShuttingDown() )
        return;

    mrn_dbg(5, mrn_printf(FLF, stderr, "Changing parent of node[%d] to node[%d]\n",
                          chld_rank, par_rank));

    _network->change_Parent( chld_rank, par_rank );

    // FE: do callback only after state has been updated
    if( upstream && _network->is_LocalNodeFrontEnd()) {

        update_contents* ub = (update_contents*) malloc( sizeof(update_contents) );
        ub->type = TOPO_CHANGE_PARENT ;
        ub->chld_rank = chld_rank;
        ub->chld_port = UnknownPort;
        ub->chld_host = strdup(NULL_STRING); //ugh, this should be fixed
        ub->par_rank = par_rank;
        insert_updates_buffer( ub );

        TopologyEvent::TopolEventData* ted;
        ted = new TopologyEvent::TopolEventData( chld_rank, par_rank );
        TopologyEvent *te = new TopologyEvent( TopologyEvent::TOPOL_CHANGE_PARENT, 
                                               ted );
        _network->_evt_mgr->add_Event( te );
    }
}

void NetworkTopology::update_changePort( Rank rank, Port port, 
                                         bool upstream )
{
    if( _network->is_ShuttingDown() )
        return;

    if( port == UnknownPort )
        return;

    Node* update_node = find_Node( rank );
    mrn_dbg( 5, mrn_printf(FLF, stderr, "Changing port of node[%d] from %hu to %hu\n", 
                           rank, update_node->get_Port(), port) );
   
    //Actual port update on the local network topology's
    update_node->set_Port( port );

    // FE: do callback only after state has been updated
    if( upstream && _network->is_LocalNodeFrontEnd() ) {
        update_contents* ub = (update_contents*) malloc( sizeof(update_contents) );
        ub->type = TOPO_CHANGE_PORT;
        ub->chld_rank = rank;
        ub->chld_port = port;
        ub->chld_host = strdup(NULL_STRING); //ugh, this should be fixed
        ub->par_rank = UnknownRank;
        insert_updates_buffer( ub );
    }
}

void NetworkTopology::update_TopoStreamPeers( vector< Rank >& new_nodes )
{
    Stream* topol_strm = _network->get_Stream( TOPOL_STRM_ID );
    if( topol_strm != NULL ) {
        for( unsigned int i=0; i < new_nodes.size(); i++ ) {
            PeerNodePtr outlet = _network->get_OutletNode( new_nodes[i] );
            if( outlet != NULL )
                topol_strm->add_Stream_Peer( outlet->get_Rank() );
            else
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "No outlet for recently added backend %d\n", 
                                       new_nodes[i]) );
        }
    }
}

} // namespace MRN
