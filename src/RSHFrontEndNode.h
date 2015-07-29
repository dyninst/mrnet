/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/
#ifndef RSH_FrontEndNode_h
#define RSH_FrontEndNode_h

#include "RSHParentNode.h"
#include "FrontEndNode.h"

namespace MRN
{

class RSHFrontEndNode : public RSHParentNode,
                        public FrontEndNode
{
public:
    RSHFrontEndNode(Network* n,
                        std::string const& ihostname,
                        Rank irank);
    virtual ~RSHFrontEndNode(void);
};

} // namespace MRN

#endif // RSH_FrontEndNode_h
