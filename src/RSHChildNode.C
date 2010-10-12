/****************************************************************************
 * Copyright © 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
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
    
    if( _network->is_LocalNodeParent() ) {
        ParentNode* parent = _network->get_LocalParentNode();
        if( parent->get_numChildrenExpected() ) {
            // I have children, forward request
            if( parent->proc_PortUpdates( ipacket ) == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr, "send_PacketToChildren() failed\n" ));
                return -1;
            }
            // success, I'm done
            mrn_dbg_func_end();
            return 0;
        }
        // else, leaf commnode, so fall through
    }
    // else, i'm a back-end or a leaf commnode, so send a port update packet

    Stream *s = _network->get_Stream(2); // waitforall port update stream
        
    int type = NetworkTopology::TOPO_CHANGE_PORT ;  
    char *host_arr = strdup("NULL"); // ugh, this needs to be fixed
    Rank send_iprank = UnknownRank;
    Rank send_myrank = _network->get_LocalRank();
    Port send_port = _network->get_LocalPort();

    if( _network->is_LocalNodeBackEnd() ) 
        s->send( PROT_TOPO_UPDATE, "%ad %aud %aud %as %auhd", 
                 &type, 1, 
                 &send_iprank, 1, 
                 &send_myrank, 1, 
                 &host_arr, 1, 
                 &send_port, 1 );
    else
        s->send_internal( PROT_TOPO_UPDATE, "%ad %aud %aud %as %auhd", 
                          &type, 1, 
                          &send_iprank, 1, 
                          &send_myrank, 1, 
                          &host_arr, 1, 
                          &send_port, 1 );
    s->flush();
    free(host_arr);

    mrn_dbg_func_end();
    return 0;
}

int RSHChildNode::proc_PacketFromParent( PacketPtr cur_packet )
{
  int retval = 0;

  switch ( cur_packet->get_Tag() ) {
    case PROT_PORT_UPDATE:
        if( proc_PortUpdate(cur_packet) == -1 ) {
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
