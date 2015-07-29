/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "mrnet/NetworkTopology.h"
#include "mrnet/MRNet.h"

#include "Router.h"
#include "PeerNode.h"
#include "utils.h"

#include <assert.h>

namespace MRN {

bool Router::update_Table()
{
    mrn_dbg_func_begin();

    _sync.Lock();

    NetworkTopology * net_topo = _network->get_NetworkTopology( );

    //clear the map
    _table.clear();

    //get local node from network topology
    NetworkTopology::Node * local_node = net_topo->find_Node( _network->get_LocalRank() );

    mrn_dbg( 5, mrn_printf(FLF, stderr, "local_node: %p\n", local_node) );

    if( local_node != NULL ) {
   
        //for each child, get descendants and put child as outlet node
        std::set< NetworkTopology::Node * > children;
        std::vector< NetworkTopology::Node * > descendants;

        children = local_node->get_Children();
        
        std::set< NetworkTopology::Node * >::iterator iter;
        for( iter=children.begin(); iter!=children.end(); iter++ ){
            mrn_dbg( 5, mrn_printf(FLF, stderr, "Looking up peer node[%d]\n",
                                   (*iter)->get_Rank()) );
            PeerNodePtr cur_outlet = _network->get_PeerNode( (*iter)->get_Rank() );
            if( cur_outlet == PeerNode::NullPeerNode ) {
                mrn_dbg( 5, mrn_printf(FLF, stderr, "PeerNode[%d] doesn't exist\n",
                                       (*iter)->get_Rank()) );
                continue;
            }

            _table[ (*iter)->get_Rank() ] = cur_outlet;

            mrn_dbg( 5, mrn_printf(FLF, stderr, "Getting descendants of node[%d]\n",
                                   (*iter)->get_Rank()) );
            net_topo->get_Descendants( (*iter), descendants );

            for( unsigned int j=0; j<descendants.size(); j++ ){
                mrn_dbg( 5, mrn_printf(FLF, stderr,
                                       "Setting child[%d] as outlet for node[%d]\n",
                                       (*iter)->get_Rank(),
                                       descendants[j]->get_Rank()) );
                _table[ descendants[j]->get_Rank() ] = cur_outlet;
            }
            descendants.clear();
        }
    }
    // else, local rank not in new topology, likely has not yet been added

    _sync.Unlock();
    mrn_dbg_func_end();
    return true;
}

PeerNodePtr Router::get_OutletNode( Rank irank ) const
{
    _sync.Lock();
    std::map< Rank, PeerNodePtr >::const_iterator iter;

    iter = _table.find( irank );

    if( iter == _table.end() ){
        _sync.Unlock();
        return PeerNode::NullPeerNode;
    }

    _sync.Unlock();
    return (*iter).second;
}

}
