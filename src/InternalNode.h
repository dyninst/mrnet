/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__internalnode_h)
#define __internalnode_h 1

#include <string>
#include <list>

#include "ParentNode.h"
#include "ChildNode.h"
#include "Message.h"
#include "Protocol.h"

namespace MRN
{

class Network;

class InternalNode: public virtual ParentNode, 
                    public virtual ChildNode
{
 public:
    InternalNode( Network * inetwork,
                  std::string const& ihostname, Rank irank,
                  std::string const& iphostname, Port ipport, Rank iprank,
                  int listeningSocket,
                  Port listeningPort );
    virtual ~InternalNode(void);

    void waitLoop() const;
    virtual int proc_DataFromParent( PacketPtr ipacket ) const;
    virtual int proc_DataFromChildren( PacketPtr ipacket ) const;
};

} // namespace MRN
#endif /* __internalnode_h */
