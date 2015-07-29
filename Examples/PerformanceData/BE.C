/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "mrnet/MRNet.h"
#include "PerfData.h"

using namespace MRN;

int main(int argc, char **argv)
{
    Stream * stream=NULL;
    PacketPtr p;
    int tag=0, recv_val=0, num_iters=0;

    Network * net = Network::CreateNetworkBE( argc, argv );

    do {
        if( net->recv(&tag, p, &stream) != 1 ) {
            fprintf(stderr, "BE: stream::recv() failure\n");
            break;
        }

        switch(tag) {

        case PROT_SUM:
            p->unpack( "%d %d", &recv_val, &num_iters );

            // Send num_iters waves of integers
            for( int i=0; i < num_iters; i++ ) {
                if( stream->send( tag, "%d", recv_val*i ) == -1 ) {
                    fprintf(stderr, "BE: stream::send(%%d) failure\n");
                    tag = PROT_EXIT;
                    break;
                }
                if( stream->flush( ) == -1 ) {
                    fprintf(stderr, "BE: stream::flush() failure\n");
                    tag = PROT_EXIT;
                    break;
                }
            }
            break;

        case PROT_EXIT:
            break;

        default:
            fprintf(stderr, "BE: Unknown Protocol: %d\n", tag);
            tag = PROT_EXIT;
            break;
        }

        fflush(stdout);
        fflush(stderr);

    } while ( tag != PROT_EXIT );

    // FE delete of the net will cause us to exit, wait for it
    net->waitfor_ShutDown();
    delete net;

    return 0;
}
