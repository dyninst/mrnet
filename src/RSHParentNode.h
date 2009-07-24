/******************************************************************************
 *  Copyright © 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
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
    RSHParentNode( Network* inetwork, std::string const& ihostname, Rank irank );
    virtual ~RSHParentNode( void );

    int proc_newSubTree( PacketPtr ipacket );

    int launch_InternalNode( std::string ihostname, Rank irank,
                             std::string icommnode_exe ) const;
    int launch_Application( std::string ihostname, Rank irank,
                            std::string &ibackend_exe,
                            std::vector <std::string> &ibackend_args) const;

protected:
    virtual int proc_PacketFromChildren( PacketPtr p );

private:
    PacketPtr _initial_subtree_packet;

    int proc_SubTreeInfoRequest( PacketPtr ipacket ) const;
};

} // namespace MRN

#endif // RSH_ParentNode_h
