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

    virtual int proc_PortUpdate(PacketPtr ipacket ) const;
    virtual bool ack_PortUpdate() const;
    virtual ~RSHChildNode(void);

protected:
    virtual int proc_PacketFromParent( PacketPtr cur_packet );
};

} // namespace MRN

#endif /* __childnode_h */
