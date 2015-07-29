/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/
#ifndef XT_BackEndNode_h
#define XT_BackEndNode_h

#include "BackEndNode.h"

namespace MRN
{
class Network;

class XTBackEndNode : public BackEndNode 
{
public:
    XTBackEndNode(Network * inetwork, 
                std::string imy_hostname, Rank imy_rank,
                std::string iphostname, Port ipport, Rank iprank );
    virtual ~XTBackEndNode(void);
};

} // namespace MRN

#endif // XT_BackEndNode_h
