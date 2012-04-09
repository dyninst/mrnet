/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#ifndef os_windows
#include <unistd.h>
#endif

#include "mrnet_lightweight/MRNet.h"
#include "test_arrays_lightweight.h"

int main(int argc, char **argv)
{
    Stream_t * stream;
    Packet_t* pkt = (Packet_t*)malloc(sizeof(Packet_t));
    Network_t * net;
    int tag=0;
    uint32_t recv_array_len=0;
    void * recv_array=NULL;
    int success=1;
    Rank my_rank;

    assert(pkt);

    net = Network_CreateNetworkBE( argc, argv );

    my_rank = Network_get_LocalRank(net);

    do{
        if ( Network_recv(net, &tag, pkt, &stream) != 1){
            fprintf(stderr, "BE(%d): net::recv() failure\n", my_rank);
        }

        recv_array=NULL;
        switch(tag){
        case PROT_CHAR:
            //fprintf(stdout, "BE(%d): Processing PROT_CHAR_ARRAY ...\n", my_rank);
            if( Packet_unpack(pkt, "%ac", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::unpack(%%ac) failure\n", my_rank);
                success=0;
            }
            if( Stream_send(stream,tag, "%ac", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::send(%%ac) failure\n", my_rank);
                success=0;
            }
            break;
        case PROT_UCHAR:
            //fprintf(stdout, "BE(%d): Processing PROT_UCHAR_ARRAY ...\n", my_rank);
            if( Packet_unpack(pkt,  "%auc", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::unpack(%%auc) failure\n", my_rank);
                success=0;
            }
            if( Stream_send(stream,tag, "%auc", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::send(%%auc) failure\n", my_rank);
                success=0;
            }
            break;
        case PROT_INT:
            //fprintf(stdout, "BE(%d): Processing PROT_INT_ARRAY ...\n", my_rank);
            if( Packet_unpack(pkt,  "%ad", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::unpack(%%ad) failure\n", my_rank);
                success=0;
            }
            if( Stream_send(stream,tag, "%ad", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::send(%%ad) failure\n", my_rank);
                success=0;
            }
            break;
        case PROT_UINT:
            //fprintf(stdout, "BE(%d): Processing PROT_UINT_ARRAY ...\n", my_rank);
            if( Packet_unpack(pkt,  "%aud", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::unpack(%%aud) failure\n", my_rank);
                success=0;
            }
            if( Stream_send(stream,tag, "%aud", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::send(%%aud) failure\n", my_rank);
                success=0;
            }
            break;
        case PROT_SHORT:
            //fprintf(stdout, "BE(%d): Processing PROT_SHORT_ARRAY ...\n", my_rank);
            if( Packet_unpack(pkt,  "%ahd", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::unpack(%%ahd) failure\n", my_rank);
                success=0;
            }
            if( Stream_send(stream,tag, "%ahd", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::send(%%ahd) failure\n", my_rank);
                success=0;
            }
            break;
        case PROT_USHORT:
            //fprintf(stdout, "BE(%d): Processing PROT_USHORT_ARRAY ...\n", my_rank);
            if( Packet_unpack(pkt,  "%auhd", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::unpack(%%auhd) failure\n", my_rank);
                success=0;
            }
            if( Stream_send(stream,tag, "%auhd", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::send(%%auhd) failure\n", my_rank);
                success=0;
            }
            break;
        case PROT_LONG:
            //fprintf(stdout, "BE(%d): Processing PROT_LONG_ARRAY ...\n", my_rank);
            if( Packet_unpack(pkt,  "%ald", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::unpack(%%ald) failure\n", my_rank);
                success=0;
            }
            if( Stream_send(stream,tag, "%ald", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::send(%%ald) failure\n", my_rank);
                success=0;
            }
            break;
        case PROT_ULONG:
            //fprintf(stdout, "BE(%d): Processing PROT_ULONG_ARRAY ...\n", my_rank);
            if( Packet_unpack(pkt,  "%auld", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::unpack(%%auld) failure\n", my_rank);
                success=0;
            }
            if( Stream_send(stream,tag, "%auld", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::send(%%auld) failure\n", my_rank);
                success=0;
            }
            break;
        case PROT_FLOAT:
            //fprintf(stdout, "BE(%d): Processing PROT_FLOAT_ARRAY ...\n", my_rank);
            if( Packet_unpack(pkt,  "%af", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::unpack(%%af) failure\n", my_rank);
                success=0;
            }
            if( Stream_send(stream,tag, "%af", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::send(%%af) failure\n", my_rank);
                success=0;
            }
            break;
        case PROT_DOUBLE:
            //fprintf(stdout, "BE(%d): Processing PROT_DOUBLE_ARRAY ...\n", my_rank);
            if( Packet_unpack(pkt,  "%alf", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::unpack(%%alf) failure\n", my_rank);
                success=0;
            }
            if( Stream_send(stream,tag, "%alf", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::send(%%alf) failure\n", my_rank);
                success=0;
            }
            break;
        case PROT_EXIT:
            //fprintf(stdout, "BE(%d): Processing PROT_EXIT ...\n", my_rank);
            break;
        default:
            fprintf(stderr, "BE(%d): Unknown Protocol: %d\n", my_rank, tag);
            exit(-1);
        }

        //fflush(stdout);
        if( tag != PROT_EXIT ) {
            if( Stream_flush(stream) == -1 ) {
                fprintf(stderr, "BE(%d): stream::flush() failure\n", my_rank);
                return -1;
            }
        }

    } while( tag != PROT_EXIT );

    if( pkt != NULL )
        free(pkt); 

    // wait for final teardown packet from FE; this will cause
    // us to exit
    Network_waitfor_ShutDown(net);
    if( net != NULL )
        delete_Network_t(net);

    return 0;
}
