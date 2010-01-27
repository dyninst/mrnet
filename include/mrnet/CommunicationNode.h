/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#ifndef __communicationnode_h
#define __communicationnode_h 1

#include <string>
#include "mrnet/Types.h"
#include "mrnet/Error.h"

namespace MRN
{

class CommunicationNode {

 public:

    // BEGIN MRNET API

    std::string get_HostName( ) const;
    Port get_Port( ) const;
    Rank get_Rank( ) const;

    // END MRNET API

    ~CommunicationNode() {}
    CommunicationNode( std::string const& ihostname, Port iport, Rank irank );

    struct ltnode {
        bool operator()(const CommunicationNode* n1, const CommunicationNode* n2) const
        {
            return n1->get_Rank() < n2->get_Rank();
        }
    };

 protected:
    std::string _hostname;
    Port _port;
    Rank _rank;
    
};

} /* namespace MRN */

#endif /* __communicationnode_h */
