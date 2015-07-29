/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
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
    mrn_dbg( 3, mrn_printf(FLF, stderr, "me[%u]: %s:%hu, parent[%u]: %s:%hu\n",
                           irank, ihostname.c_str(), ParentNode::_port,
                           iprank, iphostname.c_str(), ipport) );

    ParentNode::_network->set_LocalHostName( ParentNode::_hostname  );
    ParentNode::_network->set_LocalPort( ParentNode::_port );
    ParentNode::_network->set_LocalRank( ParentNode::_rank );
    ParentNode::_network->set_InternalNode( this );

    // set initial topology to just this process
    ParentNode::_network->set_NetworkTopology( new NetworkTopology(inetwork, 
                                               ParentNode::_hostname,
								               ParentNode::_port,
								               ParentNode::_rank) );
   
    // create topology update stream
    ParentNode::_network->new_Stream( TOPOL_STRM_ID, NULL, 0, 
                                      TFILTER_TOPO_UPDATE, 
                                      SFILTER_TIMEOUT, 
                                      TFILTER_TOPO_UPDATE_DOWNSTREAM );

    //establish data connection w/ parent
    PeerNodePtr parent = ParentNode::_network->get_ParentNode();
    if( init_newChildDataConnection(parent) == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                               "init_newChildDataConnection() failed\n") );
    }
     
    //start event detection thread
    if( EventDetector::start( ParentNode::_network ) == false ){
        mrn_dbg( 1, mrn_printf(FLF, stderr, 
                               "EventDetector::start() failed\n") );
    }
     
    mrn_dbg_func_end();
}

InternalNode::~InternalNode( void )
{
}

int InternalNode::proc_DataFromParent( PacketPtr ipacket ) const
{
    std::vector< PacketPtr > packets, reverse_packets;
    int retval = 0;
    PerfDataMgr* pdm = NULL;

    mrn_dbg_func_begin();

    unsigned int strm_id = ipacket->get_StreamId();
    Stream * strm = ParentNode::_network->get_Stream( strm_id );
    if( NULL != strm )
        pdm = strm->get_PerfData();
    else if( strm_id > CTL_STRM_ID ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, 
                               "stream %u lookup failed\n", strm_id) );
        return -1;
    }

    if( NULL != pdm ) {
        if( pdm->is_Enabled(PERFDATA_MET_ELAPSED_SEC, PERFDATA_CTX_PKT_INT_DATAPAR) )
            ipacket->start_Timer(PERFDATA_PKT_TIMERS_INT_DATAPAR);
    }

    if( strm_id < CTL_STRM_ID ) {
        // fast-path for BE specific stream ids
        // TODO: check id less than max BE rank
        packets.push_back( ipacket );
    }
    else
        strm->push_Packet( ipacket, packets, reverse_packets, false );

    if( ! packets.empty() ) {
        // deliver all packets to all child nodes
        if( ParentNode::_network->send_PacketsToChildren( packets ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, 
                                   "send_PacketToChildren() failed\n") );
            retval = -1;
        }
    }
    if( ! reverse_packets.empty() ) {
        if( ParentNode::_network->send_PacketsToParent( packets ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, 
                                   "parent.send() failed()\n") );
            retval = -1;
        }
    }

    if( NULL != pdm ) {
        if( pdm->is_Enabled( PERFDATA_MET_ELAPSED_SEC, PERFDATA_CTX_PKT_INT_DATAPAR))
            ipacket->stop_Timer(PERFDATA_PKT_TIMERS_INT_DATAPAR);
    }
 
    mrn_dbg_func_end();
    return retval;
}

int InternalNode::proc_DataFromChildren( PacketPtr ipacket ) const
{
    std::vector< PacketPtr > packets, reverse_packets;
    int retval = 0;
    PerfDataMgr* pdm = NULL;

    mrn_dbg_func_begin();

    unsigned int strm_id = ipacket->get_StreamId();
    Stream* strm = ParentNode::_network->get_Stream( strm_id );
    if( NULL != strm )
        pdm = strm->get_PerfData();
    else if( strm_id > CTL_STRM_ID ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, 
                               "stream %u lookup failed\n", strm_id) );
        return -1;
    }

    if( NULL != pdm ) {
        if( pdm->is_Enabled(PERFDATA_MET_ELAPSED_SEC, PERFDATA_CTX_PKT_INT_DATACHILD) )
            ipacket->start_Timer( PERFDATA_PKT_TIMERS_INT_DATACHILD );
    }

    if( strm_id < CTL_STRM_ID ) {
        // fast-path for BE specific stream ids
        // TODO: check id less than max BE rank
        packets.push_back( ipacket );
    }
    else
        strm->push_Packet( ipacket, packets, reverse_packets, true );

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

    if( NULL != pdm ) {
        if( pdm->is_Enabled(PERFDATA_MET_ELAPSED_SEC, PERFDATA_CTX_PKT_INT_DATACHILD) )
            ipacket->stop_Timer(PERFDATA_PKT_TIMERS_INT_DATACHILD);
    }

    mrn_dbg_func_end();
    return retval;
}

} // namespace MRN
