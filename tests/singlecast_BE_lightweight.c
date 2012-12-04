/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "singlecast_lightweight.h"

int main( int argc, char* argv[] )
{
    Stream_t *stream = NULL;
    Stream_t *grp_stream = NULL;
    Stream_t *be_stream = NULL;
    int tag;
    Packet_t* pkt = (Packet_t*)malloc(sizeof(Packet_t));
    Network_t* net;
    int done;
    int rret;
    unsigned int val = 0;

    assert(pkt);
    
    // join the MRNet net
    net = Network_CreateNetworkBE( argc, argv );

    done = 0;
    while( !done ) {
        // receive the broadcast message
        tag = 0;
        rret = Network_recv(net,  &tag, pkt, &stream);
        if( rret == -1 ) {
            fprintf(stderr, "BE: Network_recv() failed\n");
            return -1;
        }

        if( tag == SC_GROUP ) {
            grp_stream = stream;
        }
        else if( tag == SC_SINGLE ) {
            be_stream = stream;
            Packet_unpack(pkt,  "%ud", &val );
            fprintf(stdout, "BE: sending val on BE stream\n");
            if( (Stream_send(be_stream,  tag, "%ud", val ) == -1) ||
                (Stream_flush(be_stream) == -1) ) {
                fprintf(stderr, "BE: val send single failed\n");
            }
            fflush(stdout);
            val = 1;
        }
        else {
            done = 1;
            fprintf(stderr, "BE: unexpected tag %d\n", tag);
        }

        if( grp_stream && (val != 0) )
            done = 1;
    }

    // send our value for the reduction
    fprintf(stdout, "BE: sending val on group stream\n");
    if( (Stream_send(grp_stream,  SC_GROUP, "%ud", val ) == -1) ||
        (Stream_flush(grp_stream) == -1) ) {
        fprintf(stderr, "BE: val send group failed\n");
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
