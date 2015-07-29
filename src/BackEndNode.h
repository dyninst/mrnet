/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__backendnode_h)
#define __backendnode_h 1

#include <string>

#include "ChildNode.h"
#include "Message.h"
#include "Protocol.h"
#include "mrnet/Stream.h"

namespace MRN
{
class Network;

class BackEndNode: public virtual ChildNode
{
 public:
    BackEndNode( Network * inetwork, 
                 std::string imy_hostname, Rank imy_rank,
                 std::string iphostname, Port ipport, Rank iprank );
    virtual ~BackEndNode(void);

    virtual int proc_DataFromParent( PacketPtr ) const;

    int proc_newStream( PacketPtr ) const;
    int proc_deleteStream(PacketPtr) const;
    int proc_newFilter( PacketPtr ) const;
    int proc_FilterParams( FilterType, PacketPtr &ipacket ) const;
};

} // namespace MRN

#endif /* __backendnode_h */
