/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/
#ifndef XT_FrontEndNode_h
#define XT_FrontEndNode_h

#include "XTParentNode.h"
#include "FrontEndNode.h"

namespace MRN
{

class XTFrontEndNode : public XTParentNode,
                       public FrontEndNode
{
public:
    XTFrontEndNode(Network* n,
                   std::string const& ihostname,
                   Rank irank,
                   int listeningSocket,
                   Port listeningPort );
    virtual ~XTFrontEndNode(void);
};

} // namespace MRN

#endif // XT_FrontEndNode_h
