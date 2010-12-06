/****************************************************************************
 * Copyright © 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/
#ifndef RSH_ChildNode_h
#define RSH_ChildNode_h

#include "ChildNode.h"

namespace MRN
{

class RSHChildNode : public virtual ChildNode
{
 public:
    RSHChildNode( Network* inetwork,
                  std::string const& ihostname,
                  Rank irank,
                  std::string const& iphostname, 
                  Port ipport, 
                  Rank iprank );

    virtual ~RSHChildNode(void);
};

} // namespace MRN

#endif /* __childnode_h */
