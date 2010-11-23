/****************************************************************************
 * Copyright © 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <iostream>

#include "singlecast.h"

using namespace std;
using namespace MRN;

int main( int argc, char* argv[] )
{
    Stream *stream, *grp_stream, *be_stream;
    int tag;
    PacketPtr pkt;

    // join the MRNet net
    Network * net = Network::CreateNetworkBE( argc, argv );

    // participate in the broadcast/reduction roundtrip latency experiment
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

            // extract the value and send it back
            int ival = 0;
            pkt->unpack( "%d", &ival );

            // send our value for the reduction
            std::cout << "BE: sending val" << std::endl;
            if( (grp_stream->send(SC_GROUP, "%d", ival) == -1) ||
                (grp_stream->flush() == -1) ) {
                cerr << "BE: val send failed" << endl;
                break;
            }

            done = true;
        }
        else {
            cerr << "BE: unexpected tag " << tag << endl;
            done = true;
        }
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
