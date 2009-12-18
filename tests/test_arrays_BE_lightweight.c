/****************************************************************************
 * Copyright © 2003-2008 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <unistd.h>

#include "mrnet/MRNet.h"
#include "test_arrays_lightweight.h"

int main(int argc, char **argv){
    Stream_t * stream = (Stream_t*)malloc(sizeof(Stream_t));
    Packet_t* buf = (Packet_t*)malloc(sizeof(Packet_t));
    int tag=0, recv_array_len=0;
    void * recv_array=NULL;
    int success=1;

    Network_t * net = Network_CreateNetworkBE( argc, argv );

    do{
        if ( Network_recv(net, &tag, buf, &stream) != 1){
            fprintf(stderr, "stream::recv() failure\n");
        }

        recv_array=NULL;
        switch(tag){
        case PROT_CHAR:
            fprintf( stdout, "Processing PROT_CHAR_ARRAY ...\n");
            if( Packet_unpack(buf, "%ac", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "stream::unpack(%%ac) failure\n");
                success=0;
            }
            if( Stream_send(stream,tag, "%ac", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "stream::send(%%ac) failure\n");
                success=0;
            }
            break;
        case PROT_UCHAR:
            fprintf( stdout, "Processing PROT_UCHAR_ARRAY ...\n");
            if( Packet_unpack(buf,  "%auc", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "stream::unpack(%%auc) failure\n");
                success=0;
            }
            if( Stream_send(stream,tag, "%auc", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "stream::send(%%auc) failure\n");
                success=0;
            }
            break;
        case PROT_INT:
            fprintf( stdout, "Processing PROT_INT_ARRAY ...\n");
            if( Packet_unpack(buf,  "%ad", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "stream::unpack(%%ad) failure\n");
                success=0;
            }
            if( Stream_send(stream,tag, "%ad", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "stream::send(%%ad) failure\n");
                success=0;
            }
            break;
        case PROT_UINT:
            fprintf( stdout, "Processing PROT_UINT_ARRAY ...\n");
            if( Packet_unpack(buf,  "%aud", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "stream::unpack(%%aud) failure\n");
                success=0;
            }
            if( Stream_send(stream,tag, "%aud", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "stream::send(%%aud) failure\n");
                success=0;
            }
            break;
        case PROT_SHORT:
            fprintf( stdout, "Processing PROT_SHORT_ARRAY ...\n");
            if( Packet_unpack(buf,  "%ahd", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "stream::unpack(%%ahd) failure\n");
                success=0;
            }
            if( Stream_send(stream,tag, "%ahd", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "stream::send(%%ahd) failure\n");
                success=0;
            }
            break;
        case PROT_USHORT:
            fprintf( stdout, "Processing PROT_USHORT_ARRAY ...\n");
            if( Packet_unpack(buf,  "%auhd", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "stream::unpack(%%auhd) failure\n");
                success=0;
            }
            if( Stream_send(stream,tag, "%auhd", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "stream::send(%%auhd) failure\n");
                success=0;
            }
            break;
        case PROT_LONG:
            fprintf( stdout, "Processing PROT_LONG_ARRAY ...\n");
            if( Packet_unpack(buf,  "%ald", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "stream::unpack(%%ald) failure\n");
                success=0;
            }
            if( Stream_send(stream,tag, "%ald", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "stream::send(%%ald) failure\n");
                success=0;
            }
            break;
        case PROT_ULONG:
            fprintf( stdout, "Processing PROT_ULONG_ARRAY ...\n");
            if( Packet_unpack(buf,  "%auld", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "stream::unpack(%%auld) failure\n");
                success=0;
            }
            if( Stream_send(stream,tag, "%auld", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "stream::send(%%auld) failure\n");
                success=0;
            }
            break;
        case PROT_FLOAT:
            fprintf( stdout, "Processing PROT_FLOAT_ARRAY ...\n");
            if( Packet_unpack(buf,  "%af", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "stream::unpack(%%af) failure\n");
                success=0;
            }
            if( Stream_send(stream,tag, "%af", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "stream::send(%%af) failure\n");
                success=0;
            }
            break;
        case PROT_DOUBLE:
            fprintf( stdout, "Processing PROT_DOUBLE_ARRAY ...\n");
            if( Packet_unpack(buf,  "%alf", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "stream::unpack(%%alf) failure\n");
                success=0;
            }
            if( Stream_send(stream,tag, "%alf", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "stream::send(%%alf) failure\n");
                success=0;
            }
            break;
        case PROT_EXIT:
            fprintf( stdout, "Processing PROT_EXIT ...\n");
            break;
        default:
            fprintf(stdout, "Unknown Protocol: %d\n", tag);
            exit(-1);
        }
        if(Stream_flush(stream) == -1){
            fprintf(stdout, "stream::flush() failure\n");
            return -1;
        }

    } while( tag != PROT_EXIT );
#if 0
    // FE delete net will shut us down, so just go to sleep!!
    sleep(10);
#endif
    Network_recv(net, &tag, buf, &stream);
    return 0;
}
