/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "test_basic_lightweight.h"
#include "mrnet_lightweight/MRNet.h"

int main(int argc, char **argv)
{
    Stream_t * stream;
    Packet_t* pkt = (Packet_t*)malloc(sizeof(Packet_t));
    Network_t * net;

    int tag;
    char recv_char;
    char recv_uchar;
    int16_t recv_short;
    uint16_t recv_ushort;
    int32_t recv_int;
    uint32_t recv_uint;
    int64_t recv_long;
    uint64_t recv_ulong;
    float recv_float;
    double recv_double;
    char * recv_string;
    int success=1;
    
    assert(pkt);

    if( argc != 6 ) {
        fprintf(stderr, "Usage: %s parent_hostname parent_port parent_rank my_hostname my_rank\n",
                argv[0]);
        exit( -1 );
    }
   
    net = Network_CreateNetworkBE( argc, argv );

    do {
        if ( Network_recv(net,  &tag, pkt, &stream) != 1 ) {
            fprintf(stderr, "BE: stream_recv() failure ... exiting\n");
            exit (-1);
        }

        switch(tag){
        case PROT_CHAR:
#if defined(DEBUG)
            fprintf( stderr, "BE: Processing PROT_CHAR ...\n");
#endif
            if( Packet_unpack(pkt, "%c", &recv_char ) == -1 ) {
                fprintf(stderr, "BE: stream_unpack(%%c) failure\n");
                success=0;
            }
            if( Stream_send(stream,  tag, "%c", recv_char ) == -1 ) {
                fprintf(stderr, "BE: stream_send(%%c) failure\n");
                success=0;
            }
            break;
        case PROT_INT:
#if defined(DEBUG)
            fprintf( stderr, "BE: Processing PROT_INT ...\n");
#endif
            if( Packet_unpack(pkt, "%d", &recv_int ) == -1 ) {
                fprintf(stderr, "BE: stream_unpack(%%d) failure\n");
                success=0;
            }
            if( Stream_send(stream,  tag, "%d", recv_int ) == -1 ) {
                fprintf(stderr, "BE: stream_send(%%d) failure\n");
                success=0;
            }
            break;
        case PROT_UINT:
#if defined(DEBUG)
            fprintf( stderr, "BE: Processing PROT_UINT ...\n");
#endif
            if( Packet_unpack(pkt, "%ud", &recv_uint ) == -1 ) {
                fprintf(stderr, "stream_unpack(%%ud) failure\n");
                success=0;
            }
            if( Stream_send(stream,  tag, "%ud", recv_uint ) == -1 ) {
                fprintf(stderr, "stream_send(%%ud) failure\n");
                success=0;
            }
            break;
        case PROT_SHORT:
#if defined(DEBUG)
            fprintf( stderr, "BE: Processing PROT_SHORT ...\n");
#endif
            if( Packet_unpack(pkt, "%hd", &recv_short ) == -1 ) {
                fprintf(stderr, "BE: stream_unpack(%%hd) failure\n");
                success=0;
            }
            if( Stream_send(stream,  tag, "%hd", recv_short ) == -1 ) {
                fprintf(stderr, "BE: stream_send(%%hd) failure\n");
                success=0;
            }
            break;
        case PROT_USHORT:
#if defined(DEBUG)
            fprintf( stderr, "BE: Processing PROT_USHORT ...\n");
#endif
            if( Packet_unpack(pkt, "%uhd", &recv_ushort ) == -1 ) {
                fprintf(stderr, "BE: stream_unpack(%%uhd) failure\n");
                success=0;
            }
            if( Stream_send(stream,  tag, "%uhd", recv_ushort ) == -1 ) {
                fprintf(stderr, "BE: stream_send(%%uhd) failure\n");
                success=0;
            }
            break;
        case PROT_LONG:
#if defined(DEBUG)
            fprintf( stderr, "BE: Processing PROT_LONG ...\n");
#endif
            if( Packet_unpack(pkt, "%ld", &recv_long ) == -1 ) {
                fprintf(stderr, "stream_unpack(%%ld) failure\n");
                success=0;
            }
            if( Stream_send(stream,  tag, "%ld", recv_long ) == -1 ) {
                fprintf(stderr, "stream_send(%%ld) failure\n");
                success=0;
            }
            break;
        case PROT_ULONG:
#if defined(DEBUG)
            fprintf( stderr, "BE: Processing PROT_ULONG ...\n");
#endif
            if( Packet_unpack(pkt, "%uld", &recv_ulong ) == -1 ) {
                fprintf(stderr, "stream_unpack(%%uld) failure\n");
                success=0;
            }
            if( Stream_send(stream,  tag, "%uld", recv_ulong ) == -1 ) {
                fprintf(stderr, "stream_send(%%uld) failure\n");
                success=0;
            }
            break;
        case PROT_FLOAT:
#if defined(DEBUG)
            fprintf( stderr, "BE: Processing PROT_FLOAT ...\n");
#endif
            if( Packet_unpack(pkt, "%f", &recv_float ) == -1 ) {
                fprintf(stderr, "BE: stream_unpack(%%f) failure\n");
                success=0;
            }
            if( Stream_send(stream,  tag, "%f", recv_float ) == -1 ) {
                fprintf(stderr, "BE: stream_send(%%f) failure\n");
                success=0;
            }
            break;
        case PROT_DOUBLE:
#if defined(DEBUG)
            fprintf( stderr, "BE: Processing PROT_DOUBLE ...\n");
#endif
            if( Packet_unpack(pkt, "%lf", &recv_double ) == -1 ) {
                fprintf(stderr, "stream_unpack(%%lf) failure\n");
                success=0;
            }
            if( Stream_send(stream,  tag, "%lf", recv_double ) == -1 ) {
                fprintf(stderr, "stream_send(%%lf) failure\n");
                success=0;
            }
            break;
        case PROT_STRING:
#if defined(DEBUG)
            fprintf( stderr, "BE: Processing PROT_STRING ...\n");
#endif
            if( Packet_unpack(pkt, "%s", &recv_string ) == -1 ) {
                fprintf(stderr, "BE: stream_unpack(%%s) failure\n");
                success=0;
            }
            if( Stream_send(stream,  tag, "%s", recv_string ) == -1 ) {
                fprintf(stderr, "BE: stream_send(%%s) failure\n");
                success=0;
            }
            if( Stream_flush(stream) == -1){
                fprintf(stderr, "BE: stream_flush() failure\n");
                return -1;
            }
            free(recv_string);
            break;
        case PROT_ALL:
#if defined(DEBUG)
            fprintf( stderr, "BE: Processing PROT_ALL ...\n");
#endif
            if( Packet_unpack(pkt, "%c %uc %hd %uhd %d %ud %ld %uld %f %lf %s",
                             &recv_char, &recv_uchar, &recv_short, &recv_ushort,
                             &recv_int, &recv_uint, &recv_long, &recv_ulong,
                             &recv_float, &recv_double, &recv_string ) == 1 ) {
                fprintf(stderr, "BE: stream_unpack(all) failure\n");
                success = 0;
            }
            if( Stream_send(stream,  tag, "%c %uc %hd %uhd %d %ud %ld %uld %f %lf %s",
                              recv_char, recv_uchar, recv_short, recv_ushort,
                              recv_int, recv_uint, recv_long, recv_ulong,
                              recv_float, recv_double, recv_string ) == 1 ) {
                fprintf(stderr, "BE: stream_send(all) failure\n");
                success=0;
            }
            break;
        case PROT_EXIT:
#if defined(DEBUG)
            fprintf( stderr, "BE: Processing PROT_EXIT ...\n");
#endif
            break;
        default:
            fprintf(stderr, "BE: Unknown Protocol: %d\n", tag);
            exit(-1);
        }
        if( Stream_flush(stream) == -1){
            fprintf(stderr, "BE: stream_flush() failure\n");
            return -1;
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
