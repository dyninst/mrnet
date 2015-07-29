/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
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
    RSHParentNode(),
    FrontEndNode( inetwork, ihostname, irank )
{
}

RSHFrontEndNode::~RSHFrontEndNode( void  )
{
}

} // namespace MRN
