/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdlib.h>
#include <time.h>

#include "mrnet/MRNet.h"
#include "test_common.h"
#include "test_DynamicFilters.h"

using namespace MRN;
using namespace MRN_test;

int main(int argc, char **argv)
{
    Stream * stream;
    PacketPtr buf;
    int tag;

    srandom( unsigned(time(NULL)) ); //arbitrary seed to random()

    Network * net = Network::CreateNetworkBE( argc, argv );

    do{
        if ( net->recv(&tag, buf, &stream) != 1){
            fprintf(stderr, "stream::recv() failure\n");
        }

        switch(tag){
        case PROT_COUNT:
            fprintf( stdout, "BE: Processing PROT_COUNT ...\n");
            if( stream->send(tag, "%d", 1) == -1 ){
                fprintf(stderr, "stream::send(%%d) failure\n");
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
            if( stream->send(tag, "%d %d", odd, even) == -1 ){
                fprintf(stderr, "stream::send(%%d) failure\n");
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
            if( stream->flush( ) == -1 ){
                fprintf(stderr, "stream::flush() failure\n");
            }
        }
    } while ( tag != PROT_EXIT );

    if( stream != NULL )
        delete stream;

    // FE delete net will shut us down, wait for it
    net->waitfor_ShutDown();
    if( net != NULL )
        delete net;

    return 0;
}
