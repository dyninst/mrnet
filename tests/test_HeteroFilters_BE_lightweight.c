/**************************************************************************
 * Copyright 2003-2009   James Jolly, Michael J. Brim, Barton P. Miller   *
 *                Detailed MRNet usage rights in "LICENSE" file.          *
 **************************************************************************/

#ifndef os_windows
#include <unistd.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include "mrnet/MRNet.h"
#include "test_HeteroFilters_lightweight.h"

int main(int argc, char **argv)
{
    Stream_t * stream = (Stream_t*)malloc(sizeof(Stream_t));
    Packet_t* p = (Packet_t*)malloc(sizeof(Packet_t));
    int tag = 0 ;
    
    char * message;

    Network_t * net = Network_CreateNetworkBE( argc, argv );
    Rank node_rank = net->local_rank;

    do{
        if( Network_recv(net, &tag, p, &stream) != 1 ){
            fprintf(stderr, "stream_recv( ) failure\n");
            return -1;
        }

        switch(tag) {
        case PROT_TEST:
            Packet_unpack(p,  "%s", &message );
            fprintf( stdout, "BE %d: Processing PROT_TEST - got '%s'\n", node_rank, message );
            if( Stream_send(stream,tag, "%s", message) == -1 ){
                fprintf(stderr, "stream_send(%%d) failure\n");
                tag = PROT_EXIT;
            }
            if( Stream_flush( stream) == -1 ){
                fprintf(stderr, "stream_flush( ) failure\n");
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
    
    Network_recv(net, &tag, p, &stream);
    
    return 0;
}
