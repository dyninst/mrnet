/**************************************************************************
 * Copyright 2003-2009   James Jolly, Michael J. Brim, Barton P. Miller   *
 *                Detailed MRNet usage rights in "LICENSE" file.          *
 **************************************************************************/

#include "mrnet/MRNet.h"
#include "HeteroTest.h"

using namespace MRN;

int main(int argc, char **argv)
{
    Stream * stream( NULL );
    PacketPtr p;
    int tag( 0 );
    
    char * message;

    Network * net = new Network( argc, argv );
    uint node_rank = net->get_LocalRank( );
    fprintf( stderr, "backend %d waiting for start message...\n", node_rank );

    do{
        if( net->recv(&tag, p, &stream) != 1 ){
            fprintf(stderr, "stream::recv( ) failure\n");
            return -1;
        }

        if( tag == PROT_TEST ){
            p->unpack( "%s", &message );
            fprintf( stdout, "backend %d got %s\n", node_rank, message );
            if( stream->send(tag, "%s", message) == -1 ){
                fprintf(stderr, "stream::send(%%d) failure\n");
                return -1;
            }
            if( stream->flush( ) == -1 ){
                fprintf(stderr, "stream::flush( ) failure\n");
                return -1;
            }
            fprintf( stdout, "backend %d sent %s\n", node_rank, message );
            break;
        } else {
            fprintf(stderr, "backend %d: unknown protocol: %d\n", node_rank, tag);
        }

    } while( tag != PROT_EXIT );

    sleep(3); // wait for network termination
    return 0;
}
