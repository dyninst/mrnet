/****************************************************************************
 * Copyright © 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
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
                            std::string const& ihostname, Rank irank,
                            std::string const& iphostname, Port ipport, Rank iprank,
                            int listeningSocket,
                            Port listeningPort )
  : CommunicationNode( ihostname, listeningPort, irank ),
    ParentNode( inetwork, ihostname, irank, listeningSocket, listeningPort ),
    ChildNode( inetwork, ihostname, irank, iphostname, ipport, iprank ),
    InternalNode( inetwork, ihostname, irank, iphostname, ipport, iprank, listeningSocket, listeningPort ),
    RSHParentNode( inetwork, ihostname, irank ),
    RSHChildNode( inetwork, ihostname, irank, iphostname, ipport, iprank )
{
    //request subtree information
    if( request_SubTreeInfo() == -1 ){
        mrn_dbg( 1, mrn_printf(FLF, stderr, "request_SubTreeInfo() failed\n" ));
        ParentNode::error( ERR_INTERNAL, ParentNode::_rank, "request_SubTreeInfofailed\n" );
        ChildNode::error( ERR_INTERNAL, ParentNode::_rank, "request_SubTreeInfofailed\n" );
        return;
    }
}


RSHInternalNode::~RSHInternalNode( void )
{
}


int
RSHInternalNode::proc_PacketFromParent( PacketPtr cur_packet )
{
    int retval = 0;

    switch( cur_packet->get_Tag() )
    {
    case PROT_NEW_SUBTREE:
        mrn_dbg(3, mrn_printf(FLF, stderr, "Processing PROT_NEW_SUBTREE\n"));

        if( proc_newSubTree( cur_packet ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "proc_newSubTree() failed\n" ));
            retval = -1;
        }
        mrn_dbg(5, mrn_printf(FLF, stderr, "Waiting for subtrees to report ... \n" ));
        if( ! waitfor_SubTreeReports() ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "waitfor_SubTreeReports() failed\n" ));
            retval = -1;
        }
        mrn_dbg(5, mrn_printf(FLF, stderr, "Subtrees reported\n" ));

        //must send reports upwards
        if( send_NewSubTreeReport( ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                        "send_newSubTreeReport() failed\n" ));
            retval = -1;
        }
        break;

    default:
        retval = InternalNode::proc_PacketFromParent( cur_packet );
    }
    return retval;
}

} // namespace MRN
