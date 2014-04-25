/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "mrnet/MRNet.h"
#include "XTParentNode.h"

namespace MRN
{

XTParentNode::XTParentNode(void)
{
}

XTParentNode::~XTParentNode(void)
{
}

int 
XTParentNode::proc_PortUpdates( PacketPtr ipacket ) const
{
    // nothing to do, we assign static ports
    return 0;
}

} // namespace MRN
