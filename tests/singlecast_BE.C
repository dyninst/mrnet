/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <iostream>

#include "singlecast.h"

using namespace std;
using namespace MRN;

int main( int argc, char* argv[] )
{
    Stream *stream = NULL;
    Stream *grp_stream = NULL;
    Stream *be_stream = NULL;
    int tag = -1;
    unsigned int val = 0;
    PacketPtr pkt;

    // join the MRNet net
    Network * net = Network::CreateNetworkBE( argc, argv );

    bool done = false;
    while( !done ) {

        tag = 0;
        int rret = net->recv( &tag, pkt, &stream );
        if( rret == -1 ) {
            cerr << "BE: Network::recv() failed" << endl;
            break;
        }

        if( tag == SC_GROUP ) {
            grp_stream = stream;
        }
        else if( tag == SC_SINGLE ) {
            be_stream = stream;
            pkt->unpack( "%ud", &val );
            std::cout << "BE: sending val on BE stream" << std::endl;
            if( (be_stream->send(tag, "%ud", val) == -1) ||
                (be_stream->flush() == -1) ) {
                cerr << "BE: val send single failed" << endl;
            }
            val = 1;
        }
        else {
            cerr << "BE: unexpected tag " << tag << endl;
            done = true;
        }
        
        if( grp_stream && (val != 0) )
            done = true;
    }

    // send our value for the reduction
    std::cout << "BE: sending val on group stream" << std::endl;
    if( (grp_stream->send(SC_GROUP, "%ud", val) == -1) ||
        (grp_stream->flush() == -1) ) {
        cerr << "BE: val send group failed" << endl;
    }

    // cleanup
    // receive a go-away message
    tag = 0;
    int rret = net->recv( &tag, pkt, &stream );
    if( rret == -1) {
        cerr << "BE: failed to receive go-away tag" << endl;
    }
    else if( tag != SC_EXIT ) {
        cerr << "BE: received unexpected go-away tag " << tag << endl;
    }

    // wait for FE to delete network, which will shut us down
    net->waitfor_ShutDown();
    delete net;

    return 0;
}
