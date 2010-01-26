/****************************************************************************
 * Copyright © 2003-2008 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#ifndef os_windows
#include <unistd.h>
#endif

#include "mrnet/MRNet.h"
#include "test_NativeFilters_lightweight.h"

int main(int argc, char **argv)
{
    Stream_t * stream = (Stream_t*)malloc(sizeof(Stream_t));
    Packet_t* buf = (Packet_t*)malloc(sizeof(Packet_t));
    int tag;
    int success=1;

    Network_t * net = Network_CreateNetworkBE( argc, argv );
    DataType type;

    do{
        if ( Network_recv(net, &tag, buf, &stream) != 1){
            fprintf(stderr, "stream_recv() failure\n");
        }

        Packet_unpack(buf,  "%d", &type );

        switch(tag){
        case PROT_SUM:
            switch(type) {
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
        case PROT_EXIT:
            fprintf( stdout, "Processing PROT_EXIT ...\n");
            break;
        default:
            fprintf(stdout, "Unknown Protocol: %d\n", tag);
            break;
        }
        if( Stream_flush(stream) == -1 ){
            fprintf(stderr, "stream_flush() failure\n");
            success=0;
        }
    } while ( tag != PROT_EXIT );
    
    Network_recv(net, &tag, buf, &stream);
    return 0;
}
