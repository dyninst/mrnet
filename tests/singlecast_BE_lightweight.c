/****************************************************************************
 * Copyright © 2003-2008 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "singlecast_lightweight.h"

int main( int argc, char* argv[] )
{
    Stream_t *stream, *grp_stream, *be_stream;
    int tag;
    Packet_t* pkt = (Packet_t*)malloc(sizeof(Packet_t));
    int bCont;
    Network_t* net;
    int done;
    int rret;
    int nReductions = 0;
    int ival;
    int i;

    assert(pkt);
    
    // join the MRNet net
    net = Network_CreateNetworkBE( argc, argv );

    // participate in the broadcast/reduction roundtrip latency experiment
    done = false;
    while( !done ) {
        // receive the broadcast message
        tag = 0;
        rret = Network_recv(net,  &tag, pkt, &stream);
        if( rret == -1 ) {
            fprintf(stderr, "BE: Stream_recv() failed\n");
            return -1;
        }

        if( tag == SC_GROUP ) {
            grp_stream = stream;
        }
        else if( tag == SC_SINGLE ) {

            be_stream = stream;

            // extract the value and send it back
            ival = 0;
            Packet_unpack(pkt,  "%d", &ival );

            // send our value for the reduction
            fprintf(stdout, "BE: sending val\n");
            if( (Stream_send(grp_stream,  tag, "%d", ival ) == -1) ||
                (Stream_flush(grp_stream) == -1) ) {
                fprintf(stderr, "BE: val send failed\n");
                return -1;
            }
            
            done = true;
        }
        else {
            done = true;
            fprintf(stderr, "BE: unexpected tag %d\n", tag);
        }
    }


    // cleanup
    // receive a go-away message
    tag = 0;
    rret = Network_recv(net,  &tag, pkt, &stream);
    if( (rret != -1) && (tag != SC_EXIT) ) {
        fprintf(stderr, "BE: received unexpected go-away tag %d\n", tag);
    }

    if( pkt != NULL )
        free(pkt);    

    // wait for final teardown packet from FE; this will cause
    // us to exit
    Network_waitfor_ShutDown(net);
    if( net != NULL )
        delete_Network_t(net);

    return 0;
}
