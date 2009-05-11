/**************************************************************************
 * Copyright 2003-2008 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                Detailed MRNet usage rights in "LICENSE" file.          *
 **************************************************************************/

#include "mrnet/MRNet.h"
#include "MultFilTypes.h"

using namespace MRN;

int main(int argc, char **argv)
{
    Stream * stream = NULL;
    PacketPtr p;
    int tag = 0, recv_val = 0, num_iters = 0;

    Network * network = new Network( argc, argv );
    uint node_rank = network->get_LocalRank();
    fprintf( stderr, "backend on node %d waiting for start message...\n", node_rank );
    
    int first_term = 1;
    int second_term = 0;
    int new_first_term, new_second_term;
    int data[10] = { 1, 1, 2, 3, 5, 8, 13, 21, 34, 55 };

    do{
        if ( network->recv(&tag, p, &stream) != 1){
            fprintf(stderr, "stream::recv() failure\n");
            return -1;
        }

        switch(tag){

        case PROT_SUM:
            p->unpack( "%d %d", &recv_val, &num_iters );
            fprintf( stderr, "backend on node %d got start message with payload %d...\n", node_rank, num_iters );
            
            for( unsigned int i=0; i<num_iters; i++ ){
		new_first_term = first_term + second_term;
		new_second_term = first_term;
                first_term = new_first_term;
		second_term = new_second_term;

                fprintf( stdout, "BE sent %u\n", first_term);
                if( stream->send(tag, "%d", first_term) == -1 ){
                    fprintf(stderr, "stream::send(%%d) failure\n");
                    return -1;
                }
                if( stream->flush( ) == -1 ){
                    fprintf(stderr, "stream::flush() failure\n");
                    return -1;
                }
                fflush( stdout );
                sleep(3); // stagger sends
            }
            break;

        case PROT_EXIT:
            fprintf( stdout, "Processing PROT_EXIT ...\n");
            if( stream->send(tag, "%d", 0) == -1 ){
                fprintf(stderr, "stream::send(%%s) failure\n");
                return -1;
            }
            if( stream->flush( ) == -1 ){
                fprintf(stderr, "stream::flush() failure\n");
                return -1;
            }
            fflush( stdout );
            break;

        default:
            fprintf(stderr, "Unknown Protocol: %d\n", tag);
            break;
        }
    } while ( tag != PROT_EXIT );
    sleep(10); // wait for network termination

    return 0;
}
