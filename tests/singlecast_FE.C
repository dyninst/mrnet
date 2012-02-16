/****************************************************************************
 * Copyright © 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <cstdlib>
#include <cassert>
#include <iostream>
#include <string>

#include "singlecast.h"

using namespace std;
using namespace MRN;

int main( int argc, char* argv[] )
{
    int ret = 0;

    // parse the command line
    if( argc != 3 ) {
        cerr << "Usage: " << argv[0]
             << " <topology file> <backend exe>"
             << endl;
        return -1;
    }
    
    string topology_file = argv[1];
    string backend_exe = argv[2];
    const char *be_args = NULL;

    // instantiate the network
    Network* net = Network::CreateNetworkFE( topology_file.c_str(), 
                                             backend_exe.c_str(), &be_args );
    if( net->has_Error() ) {
        delete net;
        return -1;
    }

    // get a broadcast communicator
    Communicator * bcComm = net->get_BroadcastCommunicator();
    Stream* stream = net->new_Stream( bcComm, TFILTER_SUM );
    assert( bcComm != NULL );
    assert( stream != NULL );

    const set< CommunicationNode* >& bes = bcComm->get_EndPoints();
    unsigned int nBackends = (unsigned int)bes.size();
    cout << "FE: broadcast communicator has " 
         << nBackends << " backends" << endl;

    if( (stream->send( SC_GROUP, "" ) == -1) ||
        (stream->flush() == -1) ) {
        cerr << "FE: failed to broadcast stream init message" << endl;
    }

    PacketPtr pkt;
    int rret, tag = 0;
    unsigned int val, send_val = 1;
    

    set< CommunicationNode* >::const_iterator iter = bes.begin();
    for( ; iter != bes.end() ; iter++ ) {
        Rank be_rank = (*iter)->get_Rank();
        if( (-1 == net->send( be_rank, SC_SINGLE, "%ud", send_val )) ||
            (-1 == net->flush()) ) {
            std::cerr << "FE: send() failed" << std::endl;
            return -1;
        }
        
        Stream *be_strm = NULL;
        rret = net->recv( &tag, pkt, &be_strm );
        if( rret == -1 ) {
            std::cerr << "FE: recv() failed" << std::endl;
            return -1;
        }
        if( tag == SC_SINGLE ) {
            pkt->unpack( "%ud", &val );
            if( val != send_val ) {
                std::cerr << "FE: expected BE to send value " << send_val 
                      << ", got " << val << std::endl;
            }
        }
        else {
            std::cerr << "FE: unexpected tag " << tag 
                      << " received instead of SC_SINGLE\n";
        }

        rret = be_strm->recv( &tag, pkt );
        if( rret == -1 ) {
            std::cerr << "FE: be_stream recv() failed" << std::endl;
            return -1;
        }
        if( tag == SC_SINGLE ) {
            pkt->unpack( "%ud", &val );
            if( val != be_rank ) {
                std::cerr << "FE: expected BE to send value " << be_rank 
                      << ", got " << val << std::endl;
            }
        }
        else {
            std::cerr << "FE: unexpected tag " << tag 
                      << " received instead of SC_SINGLE\n";
        }

        cout << "FE: successfully received value and rank on BE stream " 
             << be_rank << endl;
    }

    rret = stream->recv( &tag, pkt );
    if( rret == -1 ) {
        std::cerr << "FE: recv() failed" << std::endl;
        return -1;
    }
    if( tag == SC_GROUP ) {
        unsigned int sum;
        pkt->unpack( "%ud", &sum );
        if( sum != nBackends ) {
            std::cerr << "FE: unexpected reduction value " << sum 
                      << " seen, expected " << nBackends << std::endl;
        }
    }
    else {
        std::cerr << "FE: unexpected tag " << tag 
                  << " received instead of SC_GROUP\n";
    }

    // tell back-ends to go away
    if( (stream->send( SC_EXIT, "" ) == -1) ||
        (stream->flush() == -1) ) {
        cerr << "FE: failed to broadcast termination message" << endl;
    }
    delete stream;

    // delete the net
    cout << "FE: deleting net" << endl;
    delete net;

    return ret;
}
