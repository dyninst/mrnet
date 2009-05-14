/**************************************************************************
 * Copyright 2003-2009   James Jolly, Michael J. Brim, Barton P. Miller   *
 *                Detailed MRNet usage rights in "LICENSE" file.          *
 **************************************************************************/

#include "mrnet/MRNet.h"
#include "MultFilTypes.h"

using namespace MRN;

int main(int argc, char **argv)
{
    Stream * stream = NULL;
    PacketPtr p;
    int tag = 0, num_iters = 0;

    Network * net = new Network( argc, argv );
    uint node_rank = net->get_LocalRank();
    fprintf( stderr, "backend on node %d waiting for start message...\n", node_rank );
    
    int first_term = 1;
    int second_term = 0;
    int new_first_term, new_second_term;
    int data[10] = { 1, 1, 2, 3, 5, 8, 13, 21, 34, 55 };

    do{
        if ( net->recv(&tag, p, &stream) != 1){
            fprintf(stderr, "stream::recv() failure\n");
            return -1;
        }

        switch(tag){

        case PROT_SUM:
            p->unpack( "%d", &num_iters );
            fprintf( stdout, "Backend %d: Processing PROT_SUM num_iters=%d\n", 
                     node_rank, num_iters );
            
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
            fprintf( stdout, "Backend %d: Processing PROT_EXIT ...\n", node_rank);
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
            fprintf(stderr, "Backend %d: Unknown Protocol: %d\n", node_rank, tag);
            break;
        }
    } while ( tag != PROT_EXIT );
    sleep(10); // wait for network termination

    return 0;
}
