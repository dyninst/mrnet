/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "xplat/NetUtils.h"
#include "mrnet/CommunicationNode.h"
#include "utils.h"

namespace MRN
{

/*===========================================================*/
/*  CommunicationNode CLASS METHOD DEFINITIONS            */
/*===========================================================*/
CommunicationNode::CommunicationNode(std::string const& ih, Port ip, Rank irank)
    : _port(ip), _rank(irank)
{
    XPlat::NetUtils::GetNetworkName(ih, _hostname );
    mrn_dbg( 5, mrn_printf(FLF, stderr,
                       "node[%u]:\"%s:%hu\"\n",
                       _rank, _hostname.c_str(), _port ));
}

std::string CommunicationNode::get_HostName( ) const
{
    return _hostname;
}

Port CommunicationNode::get_Port( ) const
{
    return _port;
}

Rank CommunicationNode::get_Rank( ) const
{
    return _rank;
}

} // namespace MRN
