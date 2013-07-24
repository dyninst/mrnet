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
#include "test_NativeFilters_lightweight.h"

int main(int argc, char **argv)
{
    Stream_t * stream;
    Packet_t* pkt = (Packet_t*)malloc(sizeof(Packet_t));
    Network_t * net;
    int tag, success;
    DataType typ;

    assert(pkt);

    net = Network_CreateNetworkBE( argc, argv );

    do{
        if( Network_recv(net, &tag, pkt, &stream) != 1 ){
            fprintf(stderr, "stream_recv() failure\n");
            break;
        }

        success = 1;

        Packet_unpack(pkt, "%d", &typ );

        switch(tag){
        case PROT_SUM:
            switch(typ) {
            case CHAR_T:
                fprintf( stdout, "Processing CHAR_SUM ...\n");
                if( Stream_send(stream, tag, "%c", CHARVAL) == -1 ){
                    fprintf(stderr, "stream_send(%%c) failure\n");
                    success=0;
                }
                break;
            case UCHAR_T:
                fprintf( stdout, "Processing UCHAR_SUM ...\n");
                if( Stream_send(stream, tag, "%uc", UCHARVAL) == -1 ){
                    fprintf(stderr, "stream_send(%%uc) failure\n");
                    success=0;
                }
                break;
            case INT16_T:
                fprintf( stdout, "Processing INT16_SUM ...\n");
                if( Stream_send(stream, tag, "%hd", INT16VAL) == -1 ){
                    fprintf(stderr, "stream_send(%%hd) failure\n");
                    success=0;
                }
                break;
            case UINT16_T:
                fprintf( stdout, "Processing UINT16_SUM ...\n");
                if( Stream_send(stream, tag, "%uhd", UINT16VAL) == -1 ){
                    fprintf(stderr, "stream_send(%%uhd) failure\n");
                    success=0;
                }
                break;
            case INT32_T:
                fprintf( stdout, "Processing INT32_SUM ...\n");
                if( Stream_send(stream, tag, "%d", INT32VAL) == -1 ){
                    fprintf(stderr, "stream_send(%%d) failure\n");
                    success=0;
                }
                break;
            case UINT32_T:
                fprintf( stdout, "Processing UINT32_SUM ...\n");
                if( Stream_send(stream, tag, "%ud", UINT32VAL) == -1 ){
                    fprintf(stderr, "stream_send(%%ud) failure\n");
                    success=0;
                }
                break;
            case INT64_T:
                fprintf( stdout, "Processing INT64_SUM ...\n");
                if( Stream_send(stream, tag, "%ld", INT64VAL) == -1 ){
                    fprintf(stderr, "stream_send(%%ld) failure\n");
                    success=0;
                }
                break;
            case UINT64_T:
                fprintf( stdout, "Processing UINT64_SUM ...\n");
                if( Stream_send(stream, tag, "%uld", UINT64VAL) == -1 ){
                    fprintf(stderr, "stream_send(%%uld) failure\n");
                    success=0;
                }
                break;
            case FLOAT_T:
                fprintf( stdout, "Processing FLOAT_SUM ...\n");
                if( Stream_send(stream, tag, "%f", FLOATVAL) == -1 ){
                    fprintf(stderr, "stream_send(%%f) failure\n");
                    success=0;
                }
                break;
            case DOUBLE_T:
                fprintf( stdout, "Processing DOUBLE_SUM ...\n");
                if( Stream_send(stream, tag, "%lf", DOUBLEVAL) == -1 ){
                    fprintf(stderr, "stream_send(%%lf) failure\n");
                    success=0;
                }
                break;
            default:
                break;
            }
            if( success ){
                if( Stream_flush(stream) == -1 ){
                    fprintf(stderr, "stream_flush() failure\n");
                }
            }
            break;
        case PROT_EXIT:
            fprintf( stdout, "Processing PROT_EXIT ...\n");
            break;
        default:
            fprintf(stdout, "Unknown Protocol: %d\n", tag);
            break;
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
