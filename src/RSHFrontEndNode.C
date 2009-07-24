/****************************************************************************
 * Copyright © 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "RSHFrontEndNode.h"

namespace MRN
{

RSHFrontEndNode::RSHFrontEndNode( Network* inetwork,
                                  std::string const& ihostname,
                                  Rank irank )
  : CommunicationNode( ihostname, UnknownPort, irank ),
    ParentNode( inetwork, ihostname, irank ),
    RSHParentNode( inetwork, ihostname, irank ),
    FrontEndNode( inetwork, ihostname, irank )
{
    // nothing else to do
}

RSHFrontEndNode::~RSHFrontEndNode( void  )
{
    // nothing else to do
}

} // namespace MRN
