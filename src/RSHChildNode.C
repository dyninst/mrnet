/****************************************************************************
 * Copyright © 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "mrnet/Network.h"
#include "RSHChildNode.h"

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


RSHChildNode::~RSHChildNode( void )
{
}


} // namespace MRN