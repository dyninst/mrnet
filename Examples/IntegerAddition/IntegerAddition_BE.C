/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "mrnet/MRNet.h"
#include "IntegerAddition.h"

using namespace MRN;

int main(int argc, char **argv)
{
    Stream * stream = NULL;
    PacketPtr p;
    int rc, tag=0, recv_val=0, num_iters=0;

    Network * net = Network::CreateNetworkBE( argc, argv );

    do {
        
        rc = net->recv(&tag, p, &stream);
        if( rc == -1 ) {
            fprintf( stderr, "BE: Network::recv() failure\n" );
            break;
        }
        else if( rc == 0 ) {
            // a stream was closed
            continue;
        }
         
        switch(tag) {

        case PROT_SUM:
            p->unpack( "%d %d", &recv_val, &num_iters );

            // Send num_iters waves of integers
            for( int i=0; i<num_iters; i++ ) {
                fprintf( stdout, "BE: Sending wave %u ...\n", i );
                if( stream->send(tag, "%d", recv_val*i) == -1 ) {
                    fprintf( stderr, "BE: stream::send(%%d) failure in PROT_SUM\n" );
                    tag = PROT_EXIT;
                    break;
                }
                if( stream->flush() == -1 ) {
                    fprintf( stderr, "BE: stream::flush() failure in PROT_SUM\n" );
                    break;
                }
                fflush(stdout);
                sleep(2); // stagger sends
            }
            break;

        case PROT_EXIT:
            if( stream->send(tag, "%d", 0) == -1 ) {
                fprintf( stderr, "BE: stream::send(%%s) failure in PROT_EXIT\n" );
                break;
            }
            if( stream->flush( ) == -1 ) {
                fprintf( stderr, "BE: stream::flush() failure in PROT_EXIT\n" );
            }
            break;

        default:
            fprintf( stderr, "BE: Unknown Protocol: %d\n", tag );
            tag = PROT_EXIT;
            break;
        }

        fflush(stderr);

    } while( tag != PROT_EXIT );

    if( stream != NULL ) {
        while( ! stream->is_Closed() )
            sleep(1);

        delete stream;
    }

    // FE delete of the net will cause us to exit, wait for it
    net->waitfor_ShutDown();
    delete net;

    return 0;
}
