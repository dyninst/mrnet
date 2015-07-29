/******************************************************************************
 *  Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                   Detailed MRNet usage rights in "LICENSE" file.           *
 ******************************************************************************/

#ifndef RSH_ParentNode_h
#define RSH_ParentNode_h

#include "ParentNode.h"

namespace MRN
{

class RSHParentNode : public virtual ParentNode
{
public:
    RSHParentNode();
    virtual ~RSHParentNode();

    int proc_PacketFromChildren( PacketPtr cur_packet );

    int proc_LaunchSubTree( PacketPtr ipacket );

    int launch_InternalNode( std::string ihostname, Rank irank,
                             std::string icommnode_exe ) const;
    int launch_Application( std::string ihostname, Rank irank,
                            std::string &ibackend_exe,
                            std::vector <std::string> &ibackend_args) const;

    int send_LaunchInfo( PeerNodePtr child_node ) const;

private:
    PacketPtr _launch_pkt;
};

} // namespace MRN

#endif // RSH_ParentNode_h
