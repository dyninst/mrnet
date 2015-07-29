/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "utils.h"
#include "mrnet/MRNet.h"

namespace MRN
{

Communicator::Communicator( Network * inetwork )
    : _network(inetwork)
{
}

Communicator::Communicator( Network * inetwork,
                            const std::set< CommunicationNode* >& iback_ends )
    : _network(inetwork), _back_ends( iback_ends )
{
}

Communicator::Communicator( Network * inetwork,
                            const std::set< Rank >& iranks )
    : _network(inetwork)
{
    _mutex.Lock();

    std::set<Rank>::const_iterator ci = iranks.begin();
    for( ; ci != iranks.end(); ci++ ) {
       CommunicationNode * new_endpoint = _network->get_EndPoint(*ci);
       _back_ends.insert( new_endpoint );
    }

    _mutex.Unlock();
}

Communicator::Communicator( Network * inetwork, Communicator &icomm )
    : _network(inetwork)
{
    _back_ends = icomm.get_EndPoints();
}

bool Communicator::add_EndPoint( Rank irank )
{
    CommunicationNode * new_endpoint = _network->get_EndPoint(irank);
    return add_EndPoint( new_endpoint );
}

bool Communicator::add_EndPoint( CommunicationNode * iendpoint )
{
    if( iendpoint == NULL )
        return false;

    _mutex.Lock();
    _back_ends.insert( iendpoint );
    _mutex.Unlock();
    return true;
}

bool Communicator::remove_EndPoint( Rank irank )
{
    CommunicationNode *cn = _network->get_EndPoint(irank);
    if( cn == NULL )
        return false;

    _mutex.Lock();
    _back_ends.erase( cn );
    _mutex.Unlock();
    return true;
}

const std::set< CommunicationNode* >& Communicator::get_EndPoints() const
{
    return _back_ends;
}

unsigned Communicator::size(void) const
{
    return (unsigned) _back_ends.size();
}

Rank* Communicator::get_Ranks(void) const
{
    Rank* ranks = NULL;
    unsigned nranks = size();
    if( nranks > 0 ) {

        ranks = new Rank[ _back_ends.size() ];
        assert( ranks );
        std::set< CommunicationNode* >::const_iterator iter = _back_ends.begin();
        for( unsigned i = 0; iter != _back_ends.end(); i++, iter++ ) {
            ranks[i] = (*iter)->get_Rank();
        }
    }
    return ranks;
}

} // namespace MRN
