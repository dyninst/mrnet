/******************************************************************************
 *  Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                   Detailed MRNet usage rights in "LICENSE" file.           *
 ******************************************************************************/

#include "XTBackEndNode.h"

namespace MRN
{

/*=====================================================*/
/*  BackEndNode CLASS METHOD DEFINITIONS            */
/*=====================================================*/

XTBackEndNode::XTBackEndNode( Network * inetwork, 
                              std::string imyhostname, Rank imyrank,
                              std::string iphostname, Port ipport, Rank iprank )
  : CommunicationNode( imyhostname, UnknownPort, imyrank ),
    ChildNode( inetwork, 
               imyhostname, imyrank, 
               iphostname, ipport, iprank ),
    BackEndNode( inetwork, 
                 imyhostname, imyrank, 
                 iphostname, ipport, iprank )
{
}

XTBackEndNode::~XTBackEndNode(void)
{
}

} // namespace MRN
