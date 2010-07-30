/******************************************************************************
 *  Copyright © 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                   Detailed MRNet usage rights in "LICENSE" file.           *
 ******************************************************************************/

#include "RSHBackEndNode.h"
#include "utils.h"

namespace MRN
{

/*=====================================================*/
/*  BackEndNode CLASS METHOD DEFINITIONS            */
/*=====================================================*/

RSHBackEndNode::RSHBackEndNode( Network * inetwork, 
                          std::string imyhostname, Rank imyrank,
                          std::string iphostname, Port ipport, Rank iprank )
  : CommunicationNode( imyhostname, UnknownPort, imyrank ),
    ChildNode( inetwork, imyhostname, imyrank, iphostname, ipport, iprank ),
    RSHChildNode( inetwork, imyhostname, imyrank, iphostname, ipport, iprank ),
    BackEndNode( inetwork, imyhostname, imyrank, iphostname, ipport, iprank )
{
   /*
   if( request_SubTreeInfo() == -1 ){
       mrn_dbg( 1, mrn_printf(FLF, stderr, "request_SubTreeInfo() failed\n" ));
       return;
    } */
}

RSHBackEndNode::~RSHBackEndNode(void)
{
}
	
/*
int
RSHBackEndNode::proc_PacketFromParent( PacketPtr cur_packet )
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

        //must send reports upwards
        if( send_SubTreeInitDoneReport( ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                        "send_newSubTreeReport() failed\n" ));
            retval = -1;
        }
        break;

    default:
        retval = RSHChildNode::proc_PacketFromParent( cur_packet );
    }
    return retval;
}
*/

} // namespace MRN
