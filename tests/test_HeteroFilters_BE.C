/**************************************************************************
 * Copyright 2003-2009   James Jolly, Michael J. Brim, Barton P. Miller   *
 *                Detailed MRNet usage rights in "LICENSE" file.          *
 **************************************************************************/

#include "mrnet/MRNet.h"
#include "test_HeteroFilters.h"

using namespace MRN;

int main(int argc, char **argv)
{
    Stream * stream( NULL );
    PacketPtr p;
    int tag( 0 );
    
    char * message;

    Network * net = Network::CreateNetworkBE( argc, argv );
    Rank node_rank = net->get_LocalRank( );

    do{
        if( net->recv(&tag, p, &stream) != 1 ){
            fprintf(stderr, "stream::recv( ) failure\n");
            return -1;
        }

        switch(tag) {
        case PROT_TEST:
            p->unpack( "%s", &message );
            fprintf( stdout, "BE %d: Processing PROT_TEST - got '%s'\n", node_rank, message );
            if( stream->send(tag, "%s", message) == -1 ){
                fprintf(stderr, "stream::send(%%d) failure\n");
                tag = PROT_EXIT;
            }
            if( stream->flush( ) == -1 ){
                fprintf(stderr, "stream::flush( ) failure\n");
            }
            break;
        case PROT_EXIT:
            fprintf( stdout, "BE %d: Processing PROT_EXIT\n", node_rank);
            break;
        default:
            fprintf(stderr, "BE %d: Unknown Protocol: %d\n", node_rank, tag);
            tag = PROT_EXIT;
            break;
        }
        fflush(stdout);

    } while( tag != PROT_EXIT );

    // FE delete network will shut us down, so just go to sleep!!
    sleep(5);
    return 0;
}
