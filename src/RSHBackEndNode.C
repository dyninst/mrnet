/******************************************************************************
 *  Copyright © 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                   Detailed MRNet usage rights in "LICENSE" file.           *
 ******************************************************************************/

#include "RSHBackEndNode.h"

namespace MRN
{

/*=====================================================*/
/*  BackEndNode CLASS METHOD DEFINITIONS            */
/*=====================================================*/

RSHBackEndNode::RSHBackEndNode( Network * inetwork, 
                          std::string imyhostname, Rank imyrank,
                          std::string iphostname, Port ipport, Rank iprank )
  : CommunicationNode( imyhostname, UnknownRank, imyrank ),
    ChildNode( inetwork, imyhostname, imyrank, iphostname, ipport, iprank ),
    RSHChildNode( inetwork, imyhostname, imyrank, iphostname, ipport, iprank ),
    BackEndNode( inetwork, imyhostname, imyrank, iphostname, ipport, iprank )
{
}

RSHBackEndNode::~RSHBackEndNode(void)
{
}

} // namespace MRN
