/******************************************************************************
 *  Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                   Detailed MRNet usage rights in "LICENSE" file.           *
 ******************************************************************************/

#ifndef XT_ParentNode_h
#define XT_ParentNode_h

#include "ParentNode.h"

namespace MRN
{

class XTParentNode : public virtual ParentNode
{
public:
    XTParentNode();
    virtual ~XTParentNode();

private:
    int proc_PortUpdates( PacketPtr ipacket ) const ;
};

} // namespace MRN

#endif // XT_ParentNode_h
