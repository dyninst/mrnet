/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__frontendnode_h)
#define __frontendnode_h 1

#include "ParentNode.h"
#include "Message.h"
#include "Protocol.h"

namespace MRN
{

class FrontEndNode: public virtual ParentNode {
 public:
    FrontEndNode(Network *, std::string const& ihostname, Rank irank);
    virtual ~FrontEndNode(void);
    virtual int proc_DataFromChildren(PacketPtr ipacket ) const;
    int proc_NewParentReportFromParent( PacketPtr ipacket ) const;
};

} // namespace MRN
#endif /* __frontendnode_h */
