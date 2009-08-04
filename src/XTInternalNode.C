/******************************************************************************
 *  Copyright � 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                   Detailed MRNet usage rights in "LICENSE" file.           *
 ******************************************************************************/

#include "XTInternalNode.h"
#include "utils.h"

namespace MRN
{

/*======================================================*/
/*  InternalNode CLASS METHOD DEFINITIONS            */
/*======================================================*/
XTInternalNode::XTInternalNode( Network* inetwork,
                                std::string const& ihostname,
                                Rank irank,
                                std::string const& iphostname,
                                Port ipport,
                                Rank iprank,
                                int listeningSocket,
                                Port listeningPort )
  : CommunicationNode( ihostname, listeningPort, irank ),
    ParentNode( inetwork, ihostname, irank, listeningSocket, listeningPort ),
    ChildNode( inetwork, ihostname, irank, iphostname, ipport, iprank ),
    InternalNode( inetwork,
                  ihostname,
                  irank, 
                  iphostname, 
                  ipport, 
                  iprank,
                  listeningSocket,
                  listeningPort )
{
    // nothing else to do
}


XTInternalNode::~XTInternalNode( void )
{
}

int
XTInternalNode::PropagateSubtreeReports( void )
{
    int retval = 0;

    if( waitfor_SubTreeReports() == -1 ) {
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
    return retval;
}

} // namespace MRN