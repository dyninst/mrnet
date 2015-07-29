/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <sstream>

#ifndef os_windows
#include "mrnet_config.h"
#endif
#include "ParsedGraph.h"
#include "Protocol.h"
#include "utils.h"
#include "xplat/Tokenizer.h"
#include "xplat/NetUtils.h"

namespace MRN
{

ParsedGraph * parsed_graph=NULL;
Rank ParsedGraph::_next_node_rank=0;

ParsedGraph::Node::Node( const char *ihostname, Rank ilocal_rank )
    : _local_rank(ilocal_rank), _rank(UnknownRank), _parent(NULL), _visited(false)
{
    std::string hostname( ihostname );
    XPlat::NetUtils::GetNetworkName( hostname, _hostname );
}

void ParsedGraph::Node::remove_Child( Node * c ) 
{
    std::vector < Node * >::iterator iter;

    for( iter=_children.begin(); iter!=_children.end(); iter++ ) {
        if( *iter == c ){
            _children.erase( iter );
            break;
        }
    } 
}

void ParsedGraph::Node::print_Node( FILE * ifd, unsigned int idepth )
{
    if( !_children.empty() ) {
        for( unsigned int i=0; i<idepth; i++ ){
            fprintf( ifd, " " );
        }

        fprintf( ifd, "<%u,%s:%u> => ",
                 _rank, _hostname.c_str(), _local_rank );
        for( unsigned int i=0; i<_children.size(); i++ ){
            fprintf( ifd, "<%u,%s:%u> ",
                     _children[i]->_rank,
                     _children[i]->_hostname.c_str(),
                     _children[i]->_local_rank );
        }
        fprintf( ifd, ";\n" );
        for( unsigned int i=0; i<_children.size(); i++ ){
            _children[i]->print_Node( ifd, idepth+1 );
        }
    }
}

/***************************************************
 * ParsedGraph
 **************************************************/

ParsedGraph::~ParsedGraph(void)
{ 
    _next_node_rank = 0;
    _root = NULL;

    std::map< std::string, Node * >::iterator niter = _nodes.begin();
    for( ; niter != _nodes.end(); niter++ )
        delete niter->second;
    _nodes.clear();

}

void ParsedGraph::add_Node(Node* new_node)
{
    char local_rank_str[128];
    sprintf(local_rank_str, "%d", new_node->get_LocalRank());

    if(new_node){
        std::string key = new_node->get_HostName() + ":"
            + std::string(local_rank_str);
        _nodes[key] = new_node;
    }
}

ParsedGraph::Node * ParsedGraph::find_Node( char * ihostname, Rank ilocal_rank )
{
    char local_rank_str[128];
    sprintf(local_rank_str, "%d", ilocal_rank);

    std::string hostname;
    XPlat::NetUtils::GetNetworkName( std::string(ihostname), hostname );

    std::string key = hostname + ":" + std::string(local_rank_str);
    std::map<std::string, Node*>::iterator iter = _nodes.find(key);
    if( iter == _nodes.end() ){
        return NULL;
    }
    return (*iter).second;
}

bool ParsedGraph::validate()
{
    unsigned int visited_nodes = preorder_traversal( _root );
    if( visited_nodes != _nodes.size() ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Failure: "
                              "visited nodes(%d) != total nodes(%d)!\n",
                              visited_nodes, _nodes.size() ));
        error( ERR_TOPOLOGY_NOTCONNECTED, UnknownRank, "parsed graph not connected" );
        _fully_connected = false;
    }

    fflush(stdout);
    return ( _cycle_free && _fully_connected );
}

unsigned int ParsedGraph::preorder_traversal( Node * node )
{
    unsigned int nodes_visited=0;

    if( node->visited() == true ){
        mrn_dbg(1, mrn_printf(FLF, stderr, "%s:%u: Node is own ancestor",
                              node->_hostname.c_str(), node->_local_rank ) );
        error( ERR_TOPOLOGY_CYCLE, UnknownRank, "%s:%u: Node is own ancestor",
               node->_hostname.c_str(), node->_local_rank );
        _cycle_free=false;
        return 0;
    }
    else{
        node->visit();
        nodes_visited++;

        if(node->_children.size() == 0){
            return nodes_visited;
        }
    }

    for(unsigned int i=0; i<node->_children.size(); i++){
        nodes_visited+=preorder_traversal(node->_children[i]);
    }

    return nodes_visited;
}



void ParsedGraph::assign_NodeRanks( bool iassign_backend_ranks )
{
    std::map < std::string, Node * >::iterator iter;

    if( iassign_backend_ranks ) {
        //Rank backend nodes 1st
        for( iter = _nodes.begin(); iter != _nodes.end(); iter++ ) {
            Node * cur_node = (*iter).second;

            if( cur_node->_children.empty() )
                cur_node->_rank = _next_node_rank++;
            else 
                cur_node->_rank = UnknownRank; 
        }
    } 

    //Rank the unranked nodes
    for( iter = _nodes.begin(); iter != _nodes.end(); iter++ ) {
        Node * cur_node = (*iter).second;

        if( cur_node->_rank == UnknownRank ) {
            cur_node->_rank = _next_node_rank++;
        }
    } 
}

std::string ParsedGraph::get_SerializedGraphString( bool have_backends )
{
    _serial_graph = NULL_STRING;
    serialize( _root, have_backends );
    return _serial_graph.get_ByteArray();
}

SerialGraph & ParsedGraph::get_SerializedGraph( bool have_backends )
{
    _serial_graph = NULL_STRING;
    serialize( _root, have_backends );
    return _serial_graph;
}

void ParsedGraph::serialize( Node * inode, bool have_backends )
{
    if( inode->_children.size() == 0 ) {
        if( have_backends ) {
            // Leaf node, just add my name to serial representation and return
            _serial_graph.add_Leaf(inode->get_HostName(), UnknownPort,
                                   inode->get_Rank() );
            return;
        }
    }

    //Starting new sub-tree component in graph serialization:
    _serial_graph.add_SubTreeRoot(inode->get_HostName(), UnknownPort,
                                  inode->get_Rank() );

    for( unsigned int i=0; i < inode->_children.size(); i++ )
        serialize( inode->_children[i], have_backends );

    //Ending sub-tree component in graph serialization:
    _serial_graph.end_SubTree();
}

void ParsedGraph::print_DOTGraph( const char * filename )
{
    std::map < std::string, Node * >::iterator iter;

    FILE * f = fopen(filename, "w" );
    if( f == NULL ){
        perror("fopen()");
        return;
    }

    fprintf( f, "digraph G{\n" );

    std::string cur_parent_str, cur_child_str;
    char rank_str[128], local_rank_str[128];
    for( iter = _nodes.begin(); iter != _nodes.end(); iter++ ) {
        Node * cur_node = (*iter).second;
        sprintf(local_rank_str, "%u", cur_node->_local_rank);
        sprintf(rank_str, "%u", cur_node->_rank);
        cur_parent_str = std::string(rank_str) + std::string("<")
            + cur_node->_hostname + std::string(":")
            + std::string(local_rank_str) + std::string(">");

        for( unsigned int i=0; i<cur_node->_children.size(); i++ ){
            sprintf(local_rank_str, "%u", cur_node->_children[i]->_local_rank);
            sprintf(rank_str, "%u", cur_node->_children[i]->_rank);
            cur_child_str = std::string(rank_str) + std::string("<")
                + cur_node->_children[i]->_hostname + std::string(":")
                + std::string(local_rank_str) + std::string(">");
            fprintf(f, "\"%s\" -> \"%s\";\n", cur_parent_str.c_str(),
                    cur_child_str.c_str() );
        }
    } 

    fprintf( f, "}\n" );
    fclose( f );
}


} // namespace MRN
