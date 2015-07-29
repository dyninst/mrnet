/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/
#ifndef RSH_BackEndNode_h
#define RSH_BackEndNode_h

#include "RSHChildNode.h"
#include "BackEndNode.h"

namespace MRN
{
class Network;

class RSHBackEndNode : public RSHChildNode,
                        public BackEndNode 
{

public:
    RSHBackEndNode(Network * inetwork, 
                std::string imy_hostname, Rank imy_rank,
                std::string iphostname, Port ipport, Rank iprank );
    virtual ~RSHBackEndNode(void);
};

} // namespace MRN

#endif /* __backendnode_h */
