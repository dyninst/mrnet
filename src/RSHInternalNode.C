/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "utils.h"
#include "RSHInternalNode.h"

namespace MRN
{

/*======================================================*/
/*  InternalNode CLASS METHOD DEFINITIONS            */
/*======================================================*/
RSHInternalNode::RSHInternalNode( Network * inetwork,
                                  std::string const& ihostname, 
                                  Rank irank,
                                  std::string const& iphostname, 
                                  Port ipport, 
                                  Rank iprank,
                                  int listeningSocket,
                                  Port listeningPort )
  : CommunicationNode( ihostname, listeningPort, irank ),
    ParentNode( inetwork, 
                ihostname, irank, 
                listeningSocket, listeningPort ),
    ChildNode( inetwork, 
               ihostname, irank, 
               iphostname, ipport, iprank ),
    RSHParentNode(),
    RSHChildNode(),
    InternalNode( inetwork, 
                  ihostname, irank, 
                  iphostname, ipport, iprank, 
                  listeningSocket, listeningPort )
{
}

RSHInternalNode::~RSHInternalNode(void)
{
}

int RSHInternalNode::request_LaunchInfo(void) const
{
    mrn_dbg_func_begin();

    PacketPtr packet( new Packet(CTL_STRM_ID, PROT_LAUNCH_SUBTREE, NULL_STRING) );
    if( ! packet->has_Error() ) {
        RSHParentNode::_network->send_PacketToParent( packet );
        RSHParentNode::_network->flush_PacketsToParent();
    }
    else {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "new packet() failed\n") );
        return -1;
    }

    return 0;
}

int RSHInternalNode::proc_PacketFromParent( PacketPtr cur_packet )
{
    int retval = 0;
    bool success = true;

    switch( cur_packet->get_Tag() ) {
    case PROT_LAUNCH_SUBTREE:
        mrn_dbg(3, mrn_printf(FLF, stderr, "Processing PROT_LAUNCH_SUBTREE\n"));

        if( proc_LaunchSubTree( cur_packet ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_newSubTree() failed\n"));
            retval = -1;
        }
        mrn_dbg(5, mrn_printf(FLF, stderr, "Waiting for subtrees to report ...\n"));
        if( ! waitfor_SubTreeInitDoneReports() ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "waitfor_SubTreeInitDoneReports() failed\n"));
            retval = -1;
            success = false;
        }

        if(success)
            mrn_dbg(5, mrn_printf(FLF, stderr, "Subtrees have reported\n" ));
        else
            mrn_dbg(5, mrn_printf(FLF, stderr, "Subtrees failed to report\n" ));

        // must send reports upwards
        if( send_SubTreeInitDoneReport() == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                        "send_SubTreeInitDoneReport() failed\n" ));
            retval = -1;
        }
        break;

    default:
        retval = ChildNode::proc_PacketFromParent( cur_packet );
    }
    return retval;
}

} // namespace MRN
