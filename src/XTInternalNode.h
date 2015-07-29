/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/
#ifndef XT_InternalNode_h
#define XT_InternalNode_h

#include "InternalNode.h"

namespace MRN
{

class XTInternalNode : public InternalNode
{
 public:
    XTInternalNode( Network* inetwork,
                    std::string const& ihostname,
                    Rank irank,
                    std::string const& iphostname,
                    Port ipport,
                    Rank iprank,
                    int listeningSocket,
                    Port listeningPort );
    virtual ~XTInternalNode(void);

    int PropagateSubtreeReports( void );
};

} // namespace MRN
#endif // XT_InternalNode_h
