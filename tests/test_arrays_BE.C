/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "mrnet/MRNet.h"
#include "test_common.h"
#include "test_arrays.h"

using namespace MRN;
using namespace MRN_test;

int main(int argc, char **argv){
    Stream * stream;
    PacketPtr buf;
    int tag=0;
    uint32_t recv_array_len=0;
    void * recv_array=NULL;
    bool success=true;

    Network * net = Network::CreateNetworkBE( argc, argv );

    Rank my_rank = net->get_LocalRank();

    do{
        if( net->recv(&tag, buf, &stream) == -1 ) {
            fprintf(stderr, "BE(%d): net::recv() failure\n", my_rank);
            tag = PROT_EXIT;
            break;
        }

        recv_array=NULL;
        switch(tag){
        case PROT_CHAR:
            //fprintf(stdout, "BE(%d): Processing PROT_CHAR_ARRAY ...\n", my_rank);
            if( buf->unpack("%ac", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::unpack(%%ac) failure\n", my_rank);
                success=false;
            }
            if( stream->send(tag, "%ac", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::send(%%ac) failure\n", my_rank);
                success=false;
            }
            break;
        case PROT_UCHAR:
            //fprintf(stdout, "BE(%d): Processing PROT_UCHAR_ARRAY ...\n", my_rank);
            if( buf->unpack( "%auc", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::unpack(%%auc) failure\n", my_rank);
                success=false;
            }
            if( stream->send(tag, "%auc", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::send(%%auc) failure\n", my_rank);
                success=false;
            }
            break;
        case PROT_INT:
            //fprintf(stdout, "BE(%d): Processing PROT_INT_ARRAY ...\n", my_rank);
            if( buf->unpack( "%ad", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::unpack(%%ad) failure\n", my_rank);
                success=false;
            }
            if( stream->send(tag, "%ad", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::send(%%ad) failure\n", my_rank);
                success=false;
            }
            break;
        case PROT_UINT:
            //fprintf(stdout, "BE(%d): Processing PROT_UINT_ARRAY ...\n", my_rank);
            if( buf->unpack( "%aud", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::unpack(%%aud) failure\n", my_rank);
                success=false;
            }
            if( stream->send(tag, "%aud", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::send(%%aud) failure\n", my_rank);
                success=false;
            }
            break;
        case PROT_SHORT:
            //fprintf(stdout, "BE(%d): Processing PROT_SHORT_ARRAY ...\n", my_rank);
            if( buf->unpack( "%ahd", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::unpack(%%ahd) failure\n", my_rank);
                success=false;
            }
            if( stream->send(tag, "%ahd", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::send(%%ahd) failure\n", my_rank);
                success=false;
            }
            break;
        case PROT_USHORT:
            //fprintf(stdout, "BE(%d): Processing PROT_USHORT_ARRAY ...\n", my_rank);
            if( buf->unpack( "%auhd", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::unpack(%%auhd) failure\n", my_rank);
                success=false;
            }
            if( stream->send(tag, "%auhd", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::send(%%auhd) failure\n", my_rank);
                success=false;
            }
            break;
        case PROT_LONG:
            //fprintf(stdout, "BE(%d): Processing PROT_LONG_ARRAY ...\n", my_rank);
            if( buf->unpack( "%ald", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::unpack(%%ald) failure\n", my_rank);
                success=false;
            }
            if( stream->send(tag, "%ald", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::send(%%ald) failure\n", my_rank);
                success=false;
            }
            break;
        case PROT_ULONG:
            //fprintf(stdout, "BE(%d): Processing PROT_ULONG_ARRAY ...\n", my_rank);
            if( buf->unpack( "%auld", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::unpack(%%auld) failure\n", my_rank);
                success=false;
            }
            if( stream->send(tag, "%auld", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::send(%%auld) failure\n", my_rank);
                success=false;
            }
            break;
        case PROT_FLOAT:
            //fprintf(stdout, "BE(%d): Processing PROT_FLOAT_ARRAY ...\n", my_rank);
            if( buf->unpack( "%af", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::unpack(%%af) failure\n", my_rank);
                success=false;
            }
            if( stream->send(tag, "%af", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::send(%%af) failure\n", my_rank);
                success=false;
            }
            break;
        case PROT_DOUBLE:
            //fprintf(stdout, "BE(%d): Processing PROT_DOUBLE_ARRAY ...\n", my_rank);
            if( buf->unpack( "%alf", &recv_array, &recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::unpack(%%alf) failure\n", my_rank);
                success=false;
            }
            if( stream->send(tag, "%alf", recv_array, recv_array_len) == -1 ){
                fprintf(stderr, "BE(%d): stream::send(%%alf) failure\n", my_rank);
                success=false;
            }
            break;
        case PROT_EXIT:
            //fprintf(stdout, "BE(%d): Processing PROT_EXIT ...\n", my_rank);
            break;
        default:
            fprintf(stderr, "BE(%d): Unknown Protocol: %d\n", my_rank, tag);
            exit(-1);
        }

        //fflush( stdout );
        if( tag != PROT_EXIT ) {
            if( stream->flush() == -1 ) {
                fprintf(stderr, "BE(%d): stream::flush() failure\n", my_rank);
                return -1;
            }
        }

    } while( tag != PROT_EXIT );

    if( stream != NULL )
        delete stream;

    // FE delete net will shut us down, so wait for it
    net->waitfor_ShutDown();
    if( net != NULL )
        delete net;

    return 0;
}
