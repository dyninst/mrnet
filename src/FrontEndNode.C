/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "EventDetector.h"
#include "FrontEndNode.h"
#include "Router.h"
#include "utils.h"
#include "mrnet/MRNet.h"

namespace MRN
{
/*===============================================*/
/*  FrontEndNode CLASS METHOD DEFINITIONS        */
/*===============================================*/
FrontEndNode::FrontEndNode( Network * inetwork, std::string const& ihostname, Rank irank )
    : CommunicationNode( ihostname, UnknownPort, irank ),
      ParentNode( inetwork, ihostname, irank )
{
    mrn_dbg_func_begin();

    inetwork->set_LocalHostName( _hostname  );
    inetwork->set_LocalPort( _port );
    inetwork->set_LocalRank( _rank );
    inetwork->set_FrontEndNode( this );
    inetwork->set_NetworkTopology( new NetworkTopology( inetwork, _hostname, _port, _rank ));
#if 0
    inetwork->set_FailureManager( new CommunicationNode( _hostname, _port, _rank ) );
#endif
    
    mrn_dbg( 5, mrn_printf(FLF, stderr, "starting EventDetectionThread\n" ));
    if( EventDetector::start( inetwork ) == false ) {
        error( ERR_INTERNAL, _rank, "EventDetector::start() failed" );
        return;
    }
    mrn_dbg_func_end();
}

FrontEndNode::~FrontEndNode(  )
{
}

int FrontEndNode::proc_DataFromChildren( PacketPtr ipacket ) const
{
    mrn_dbg_func_begin();

    std::vector < PacketPtr > packets, reverse_packets;

    unsigned int strm_id = ipacket->get_StreamId();
    Stream *stream = _network->get_Stream( strm_id );
    if( stream == NULL ){
        mrn_dbg( 1, mrn_printf(FLF, stderr, "stream %d lookup failed\n", strm_id) );
        return -1;
    }

    if( strm_id < CTL_STRM_ID ) {
        // fast-path for BE specific stream ids
        // TODO: check id less than max BE rank
        packets.push_back( ipacket );
    }
    else {
         stream->push_Packet( ipacket, packets, reverse_packets, true );
         mrn_dbg( 3, mrn_printf(FLF, stderr, 
                                "push_packet => %u packets, %u reverse_packets\n", 
                                packets.size(), reverse_packets.size()) );
    }

    if( ! packets.empty() ) {
        for( unsigned int i = 0; i < packets.size(); i++ ) {
            PacketPtr cur_packet( packets[i] );

            mrn_dbg( 3, mrn_printf(FLF, stderr, "Put packet in stream %d\n",
                                   strm_id) );
            stream->add_IncomingPacket( cur_packet );
        }
    }
    if( ! reverse_packets.empty() ) {
        if( _network->send_PacketsToChildren(reverse_packets) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketsToChildren() failed()\n" ));
            return -1;
        }
    }

    mrn_dbg_func_end();
    return 0;
}

int FrontEndNode::proc_NewParentReportFromParent( PacketPtr ipacket ) const
{
    Rank child_rank, parent_rank;

    ipacket->unpack( "%ud &ud", &child_rank, &parent_rank ); 

    //update local topology
    _network->change_Parent( child_rank, parent_rank );

    //propagate to children except on incident channel
    if( _network->send_PacketToChildren( ipacket, false ) == -1 ){
        mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed()\n" ));
        return -1;
    }

    return 0;
}

}    // namespace MRN
