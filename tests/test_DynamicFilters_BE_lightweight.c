/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#include "mrnet_lightweight/MRNet.h"
#include "test_DynamicFilters_lightweight.h"

int main(int argc, char **argv)
{
    Stream_t * stream;
    Packet_t* pkt = (Packet_t*)malloc(sizeof(Packet_t));
    Network_t * net;
    int tag;

    assert(pkt);
    
    srandom( (unsigned) time(NULL) ); //arbitrary seed to random()

    net = Network_CreateNetworkBE( argc, argv );

    do{
        if ( Network_recv(net,&tag, pkt, &stream) != 1){
            fprintf(stderr, "stream_recv() failure\n");
        }

        switch(tag){
        case PROT_COUNT:
            fprintf( stdout, "BE: Processing PROT_COUNT ...\n");
            if( Stream_send(stream,tag, "%d", 1) == -1 ){
                fprintf(stderr, "stream_send(%%d) failure\n");
            }
            break;
        case PROT_COUNTODDSANDEVENS:
            fprintf( stdout, "BE: Processing PROT_COUNTODDSANDEVENS ...\n");
            int odd, even;
            if( random() % 2 ){
                even=1, odd=0;
            }
            else{
                even=0, odd=1;
            }
            if( Stream_send(stream,tag, "%d %d", odd, even) == -1 ){
                fprintf(stderr, "stream_send(%%d) failure\n");
            }
            break;
        case PROT_EXIT:
            fprintf( stdout, "BE: Processing PROT_EXIT ...\n");
            break;
        default:
            fprintf(stdout, "BE: Unknown Protocol: %d\n", tag);
            break;
        }
        if( tag != PROT_EXIT ) {
            if( Stream_flush(stream ) == -1 ) {
                fprintf(stderr, "stream_flush() failure\n");
            }
        }
    } while ( tag != PROT_EXIT );

    if( pkt != NULL )
        free(pkt);

    // wait for final teardown packet from FE; this will cause
    // us to exit
    Network_waitfor_ShutDown(net);
    if( net != NULL )
        delete_Network_t(net);

    return 0;
}
