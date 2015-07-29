/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__communicator_h)
#define __communicator_h 1

#include <set>
#include <string>

#include "mrnet/CommunicationNode.h"
#include "mrnet/Types.h"
#include "xplat/Mutex.h"

namespace MRN
{

class Network;

class Communicator {

    friend class Stream;
    friend class Network;

 public:

    // BEGIN MRNET API

    bool add_EndPoint( Rank irank );
    bool add_EndPoint( CommunicationNode* );

    const std::set< CommunicationNode* >& get_EndPoints() const;

    unsigned size(void) const;

    // END MRNET API

 private:
    Network * _network;
    std::set<CommunicationNode *> _back_ends;
    XPlat::Mutex _mutex;

    Communicator( Network * );
    Communicator( Network *, Communicator &);
    Communicator( Network *, const std::set< CommunicationNode* >& );
    Communicator( Network *, const std::set< Rank >& );

    Rank* get_Ranks(void) const;

    bool remove_EndPoint( Rank irank );
};


} // namespace MRN

#endif /* __communicator_h */
