/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
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
    RSHChildNode();
    virtual ~RSHChildNode();
};

} // namespace MRN

#endif /* __childnode_h */
