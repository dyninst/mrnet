/****************************************************************************
 * Copyright © 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "mrnet/Network.h"
#include "RSHChildNode.h"
#include "InternalNode.h"
#include "utils.h"
#include "mrnet/MRNet.h"


namespace MRN
{

RSHChildNode::RSHChildNode( Network* inetwork,
                            std::string const& ihostname,
                            Rank irank,
                            std::string const& iphostname,
                            Port ipport, 
                            Rank iprank )
  : CommunicationNode( ihostname, UnknownPort, irank ),
    ChildNode( inetwork, ihostname, irank, iphostname, ipport, iprank )
{
}

int RSHChildNode::proc_PortUpdate( PacketPtr ipacket ) const
{ 
    mrn_dbg_func_begin();
    Stream *s = _network->get_Stream(1);

    int type = TOPO_CHANGE_PORT ;  
    char *host_arr=strdup("NULL");
    uint32_t* send_iprank = (uint32_t*) malloc(sizeof(uint32_t));
    *send_iprank=1;
    uint32_t* send_myrank = (uint32_t*) malloc(sizeof(uint32_t));
    *send_myrank=_network->get_LocalRank();
    uint16_t* send_port = (uint16_t*)malloc(sizeof(uint16_t));
    *send_port = _network->get_LocalPort();
    
    if ( _network->is_LocalNodeInternal() )
       s->send_internal(PROT_TOPO_UPDATE,"%ad %aud %aud %as %auhd", &type, 1, send_iprank, 1, send_myrank, 1, &host_arr,1, send_port, 1);
    else
       s->send(PROT_TOPO_UPDATE,"%ad %aud %aud %as %auhd", &type, 1, send_iprank, 1, send_myrank, 1, &host_arr,1, send_port, 1);
    
    _network->flush_PacketsToParent();
    free(host_arr);

    if( _network->is_LocalNodeParent() ) {
       if( _network->get_LocalParentNode()->proc_PortUpdates( ipacket ) == -1 ) {
           mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n" ));
           return -1;
       }
    }
    else {
       // send ack to parent
       if( ! ack_PortUpdate() ) {
           mrn_dbg( 1, mrn_printf(FLF, stderr, "ack_TopologyReport() failed\n" ));
	   return -1;
       }
    }

    mrn_dbg_func_end();
    return 0;
}


bool RSHChildNode::ack_PortUpdate( void ) const
{
    mrn_dbg_func_begin();

    PacketPtr packet( new Packet( 0, PROT_PORT_UPDATE_ACK, "" ));

    if( !packet->has_Error( ) ) {
        if( ( _network->get_ParentNode()->send( packet ) == -1 ) ||
            ( _network->get_ParentNode()->flush(  ) == -1 ) ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "send/flush failed\n" ));
            return false;
        }
    }
    else {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "new packet() failed\n" ));
        return false;
    }

    mrn_dbg_func_end();
    return true;
}

int RSHChildNode::proc_PacketFromParent( PacketPtr cur_packet )
{
  int retval = 0;

  switch ( cur_packet->get_Tag() ) {
    case PROT_PORT_UPDATE:
      if( proc_PortUpdate( cur_packet ) == -1 ) {
          mrn_dbg( 1, mrn_printf(FLF, stderr,
                   "proc_PortUpdate() failed\n" ));
          retval = -1;
      }
      break;
    default:
      retval = ChildNode::proc_PacketFromParent( cur_packet );
      break;
  }
  return retval;
}


RSHChildNode::~RSHChildNode( void )
{
}


} // namespace MRN
