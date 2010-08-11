/******************************************************************************
 *  Copyright © 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                   Detailed MRNet usage rights in "LICENSE" file.           *
 ******************************************************************************/

#include <iostream>
#include <sstream>
#include "XTFrontEndNode.h"

namespace MRN
{

XTFrontEndNode::XTFrontEndNode( Network* inetwork,
                                std::string const& ihostname,
                                Rank irank,
                                int listeningSocket,
                                Port listeningPort )
  : CommunicationNode( ihostname, listeningPort, irank ),
    ParentNode( inetwork, ihostname, irank, listeningSocket, listeningPort ),
    FrontEndNode( inetwork, ihostname, irank )
{
    // nothing else to do
}

XTFrontEndNode::~XTFrontEndNode( void  )
{
    // nothing else to do
}

} // namespace MRN
