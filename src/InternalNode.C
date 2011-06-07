/****************************************************************************
 *  Copyright 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
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
    ParentNode::_network->set_NetworkTopology( new NetworkTopology(inetwork, 
                                                                   ParentNode::_hostname,
								   ParentNode::_port,
								   ParentNode::_rank) );
   
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
     
    NetworkTopology* nt = ParentNode::_network->get_NetworkTopology();  
    if( nt != NULL ) {
    
        if( ! nt->in_Topology(ihostname, listeningPort, irank) ) {
            // not already in topology => internal node attach case 
            mrn_dbg( 5, mrn_printf(FLF, stderr, 
                                   "Internal node not in the topology\n") );

            // send topology update for new CP
            Stream* s = ParentNode::_network->new_Stream( TOPOL_STRM_ID, NULL, 0, 
                                                          TFILTER_TOPO_UPDATE, 
                                                          SFILTER_TIMEOUT, 
                                              TFILTER_TOPO_UPDATE_DOWNSTREAM );
            int type = NetworkTopology::TOPO_NEW_CP; 
            char *host_arr = strdup( ihostname.c_str() );
            s->send_internal( PROT_TOPO_UPDATE,"%ad %aud %aud %as %auhd", 
                              &type, 1, 
                              &iprank, 1, 
                              &irank, 1, 
                              &host_arr, 1, 
                              &listeningPort, 1 );
            free(host_arr);
        } 
        else
            mrn_dbg( 5, mrn_printf(FLF, stderr,
                                   "Internal node already in the topology\n") );
    } 
 
    mrn_dbg_func_end();
}

InternalNode::~InternalNode( void )
{
}

int InternalNode::proc_DataFromParent( PacketPtr ipacket ) const
{
    int retval = 0;
    std::vector< PacketPtr > packets, reverse_packets;

    mrn_dbg_func_begin();

    unsigned int strm_id = ipacket->get_StreamId();
    if( strm_id < CTL_STRM_ID ) {
        // fast-path for BE specific stream ids
        // TODO: check id less than max BE rank
        packets.push_back( ipacket );
    }
    else {

        Stream *stream = ParentNode::_network->get_Stream( strm_id );
        if( stream == NULL ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "stream %d lookup failed\n",
                                   ipacket->get_StreamId( ) ));
            return -1;
        }

        stream->push_Packet( ipacket, packets, reverse_packets, false );
    }

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

    std::vector< PacketPtr > packets, reverse_packets;

    unsigned int strm_id = ipacket->get_StreamId();
    if( strm_id < CTL_STRM_ID ) {
        // fast-path for BE specific stream ids
        // TODO: check id less than max BE rank
        packets.push_back( ipacket );
    }
    else {

        Stream *stream = ParentNode::_network->get_Stream( strm_id );
        if( stream == NULL ){
            mrn_dbg( 1, mrn_printf(FLF, stderr, "stream %d lookup failed\n",
                                   ipacket->get_StreamId( ) ));
            return -1;
        }

        stream->push_Packet( ipacket, packets, reverse_packets, true );
    }

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

} // namespace MRN
