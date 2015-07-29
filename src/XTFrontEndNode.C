/******************************************************************************
 *  Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                   Detailed MRNet usage rights in "LICENSE" file.           *
 ******************************************************************************/

#include "XTFrontEndNode.h"

namespace MRN
{

XTFrontEndNode::XTFrontEndNode( Network* inetwork,
                                std::string const& ihostname,
                                Rank irank,
                                int listeningSocket,
                                Port listeningPort )
  : CommunicationNode( ihostname, listeningPort, irank ),
    ParentNode( inetwork, ihostname, irank, 
                listeningSocket, listeningPort ),
    XTParentNode(),
    FrontEndNode( inetwork, ihostname, irank )
{
}

XTFrontEndNode::~XTFrontEndNode( void  )
{
}

} // namespace MRN
