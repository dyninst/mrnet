/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "EventDetector.h"
#include "InternalNode.h"
#include "Router.h"
#include "utils.h"
#include "mrnet/MRNet.h"
#include "SerialGraph.h"

namespace MRN
{

/*======================================================*/
/*  InternalNode CLASS METHOD DEFINITIONS            */
/*======================================================*/
InternalNode::InternalNode( Network * inetwork,
                            std::string const& ihostname, Rank irank,
                            std::string const& iphostname, Port ipport, Rank iprank,
                            int listeningSocket, Port listeningPort )
  : CommunicationNode( ihostname, listeningPort, irank ),
    ParentNode( inetwork, ihostname, irank, listeningSocket, listeningPort ),
    ChildNode( inetwork, ihostname, irank, iphostname, ipport, iprank )
{
    mrn_dbg( 3, mrn_printf(FLF, stderr, "Local[%u]: %s:%u, parent[%u]: %s:%u\n",
                           ParentNode::_rank, ParentNode::_hostname.c_str(),
                           ParentNode::_port,
                           iprank, iphostname.c_str(), ipport ));

    ParentNode::_network->set_LocalHostName( ParentNode::_hostname  );
    ParentNode::_network->set_LocalPort( ParentNode::_port );
    ParentNode::_network->set_LocalRank( ParentNode::_rank );
    ParentNode::_network->set_InternalNode( this );

    //new topo prop code
    ParentNode::_network->set_NetworkTopology(new NetworkTopology (inetwork, 
                                                                   ParentNode::_hostname,
								   ParentNode::_port,
								   ParentNode::_rank ) );
   
//    ParentNode::_network->new_Stream( 1, NULL, 0, TFILTER_TOPO_UPDATE, SFILTER_TIMEOUT, TFILTER_TOPO_UPDATE_DOWNSTREAM );
  //  Stream *st = ParentNode::_network->_streams[1];


    //PacketPtr packet( new Packet(1, PROT_SET_FILTERPARAMS_UPSTREAM_SYNC, "%ud", 250));
    //if( packet->has_Error() ) {
     //  mrn_dbg(1, mrn_printf(FLF, stderr, "new packet fail when trying to construct packet for set filter parameters \n"));
   // }

    //st->set_FilterParams( FILTER_UPSTREAM_SYNC, packet ); 

    //establish data connection w/ parent
    if( init_newChildDataConnection( ParentNode::_network->get_ParentNode() ) == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                               "init_newChildDataConnection() failed\n" ));
        return;
    }

    //start event detection thread
    if( EventDetector::start( ParentNode::_network ) == false ){
        mrn_dbg( 1, mrn_printf(FLF, stderr, "start_EventDetectionThread() failed\n" ));
        ParentNode::error( ERR_INTERNAL, ParentNode::_rank, "start_EventDetectionThread failed\n" );
        ChildNode::error( ERR_INTERNAL, ParentNode::_rank, "start_EventDetectionThread failed\n" );
        return;
    }

    mrn_dbg( 5, mrn_printf(FLF, stderr, "InternalNode:%p\n", this ));
    mrn_dbg_func_end();
}




InternalNode::~InternalNode( void )
{
}

int InternalNode::proc_DataFromParent( PacketPtr ipacket ) const
{
    int retval = 0;

    mrn_dbg_func_begin();

    Stream *stream = ParentNode::_network->get_Stream( ipacket->get_StreamId( ) );
    if( stream == NULL ){
        mrn_dbg( 1, mrn_printf(FLF, stderr, "stream %d lookup failed\n",
                               ipacket->get_StreamId( ) ));
        return -1;
    }

    std::vector< PacketPtr > packets, reverse_packets;

    stream->push_Packet( ipacket, packets, reverse_packets, false );  // packet going to children

    if( ! packets.empty() ) {
        // deliver all packets to all child nodes
        if( ParentNode::_network->send_PacketsToChildren( packets ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n" ));
            retval = -1;
        }
    }
    if( ! reverse_packets.empty() ) {
        if( ParentNode::_network->send_PacketsToParent( packets ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "parent.send() failed()\n" ));
            retval = -1;
        }
    }

    mrn_dbg_func_end();
    return retval;
}

int InternalNode::proc_DataFromChildren( PacketPtr ipacket ) const
{
    int retval = 0;

    mrn_dbg_func_begin();

    Stream *stream = ParentNode::_network->get_Stream( ipacket->get_StreamId( ) );
    if( stream == NULL ){
        mrn_dbg( 1, mrn_printf(FLF, stderr, "stream %d lookup failed\n",
                               ipacket->get_StreamId( ) ));
        return -1;
    }

    std::vector< PacketPtr > packets, reverse_packets;

    stream->push_Packet( ipacket, packets, reverse_packets, true );

    if( ! packets.empty() ) {
        if( ParentNode::_network->send_PacketsToParent( packets ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "parent.send() failed()\n" ));
            retval = -1;
        }
    }
    if( ! reverse_packets.empty() ) {
        if( ParentNode::_network->send_PacketsToChildren( reverse_packets ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketsToChildren() failed\n" ));
            retval = -1;
        }
    }

    mrn_dbg_func_end();
    return retval;
}

int InternalNode::proc_FailureReportFromParent( PacketPtr ipacket ) const
{
    Rank failed_rank;

    ipacket->unpack( "%uhd", &failed_rank ); 

    //update local topology
    ParentNode::_network->remove_Node( failed_rank );

    //propagate to children as necessary
    ParentNode::_network->send_PacketToChildren( ipacket );

    return 0;
}

int InternalNode::proc_NewParentReportFromParent( PacketPtr ipacket ) const
{
    Rank child_rank, parent_rank;

    ipacket->unpack( "%ud &ud", &child_rank, &parent_rank ); 

    //update local topology
    ParentNode::_network->change_Parent( child_rank, parent_rank );

    //propagate to children except on incident channel
    if( ParentNode::_network->send_PacketToChildren( ipacket, false ) == -1 ){
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                               "send_PacketToChildren() failed()\n" ));
        return -1;
    }

    if( ipacket->get_InletNodeRank() != ParentNode::_network->get_ParentNode()->get_Rank() ) {
        //propagate to parent
        if( ParentNode::_network->send_PacketToParent( ipacket ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "parent.send() failed()\n" ));
            return -1;
        }
    }

    return 0;
}

}                               // namespace MRN
