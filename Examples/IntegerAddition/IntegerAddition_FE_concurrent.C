/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "mrnet/MRNet.h"
#include "IntegerAddition.h"

using namespace MRN;

int main(int argc, char **argv)
{
    int send_val=32, recv_val=0;
    int tag, retval;
    PacketPtr p;

    if( argc != 4 ) {
        fprintf(stderr, 
                "Usage: %s <topology file> <backend_exe> <so_file>\n", argv[0]);
        exit(-1);
    }
    const char * topology_file = argv[1];
    const char * backend_exe = argv[2];
    const char * so_file = argv[3];
    const char * dummy_argv=NULL;


    // This Network() cnstr instantiates the MRNet internal nodes, according to
    // the organization in "topology_file," and the application back-end with
    // any specified cmd line args
    Network * net = Network::CreateNetworkFE( topology_file, 
                                              backend_exe, &dummy_argv );
    if( net->has_Error() ) {
        net->perror("[1] Network creation failed");
        exit(-1);
    }
    Network * net2 = Network::CreateNetworkFE( topology_file, 
                                               backend_exe, &dummy_argv );
    if( net2->has_Error() ) {
        net2->perror("[2] Network creation failed");
        exit(-1);
    }

    // Make sure path to "so_file" is in LD_LIBRARY_PATH
    int filter_id = net->load_FilterFunc( so_file, "IntegerAdd" );
    if( filter_id == -1 ) {
        fprintf( stderr, "[1] Network::load_FilterFunc() failure\n");
        delete net;
        delete net2;
        return -1;
    }
    int filter_id2 = net2->load_FilterFunc( so_file, "IntegerAdd" );
    if( filter_id2 == -1 ) {
        fprintf( stderr, "[2] Network::load_FilterFunc() failure\n");
        delete net;
        delete net2;
        return -1;
    }


    // A Broadcast communicator contains all the back-ends
    Communicator * comm_BC = net->get_BroadcastCommunicator();
    Communicator * comm_BC2 = net2->get_BroadcastCommunicator();

    // Create a stream that will use the Integer_Add filter for aggregation
    Stream * add_stream = net->new_Stream( comm_BC, filter_id,
                                           SFILTER_WAITFORALL );
    Stream * add_stream2 = net2->new_Stream( comm_BC2, filter_id2,
                                             SFILTER_WAITFORALL );

    int num_backends = int(comm_BC->get_EndPoints().size());

    // Broadcast a control message to back-ends to send us "num_iters"
    // waves of integers
    tag = PROT_SUM;
    unsigned int num_iters=5;
    if( add_stream->send(tag, "%d %d", send_val, num_iters) == -1 ) {
        fprintf( stderr, "[1] stream::send() failure\n");
        return -1;
    }
    if( add_stream->flush() == -1 ) {
        fprintf( stderr, "[1] stream::flush() failure\n");
        return -1;
    }
    
    if( add_stream2->send(tag, "%d %d", send_val, num_iters) == -1 ) {
        fprintf( stderr, "[2] stream::send() failure\n");
        return -1;
    }
    if( add_stream2->flush() == -1 ) {
        fprintf( stderr, "[2] stream::flush() failure\n");
        return -1;
    }

    // We expect "num_iters" aggregated responses from all back-ends
    for( unsigned int i=0; i<num_iters; i++ ) {

        retval = add_stream->recv(&tag, p);
        assert( retval != 0 ); //shouldn't be 0, either error or block till data
        if( retval == -1) {
            //recv error
            fprintf( stderr, "stream::recv() failure\n");
            return -1;
        }

        if( p->unpack("%d", &recv_val) == -1 ) {
            fprintf( stderr, "stream::unpack() failure\n");
            return -1;
        }
        int expected_val = num_backends * i * send_val;
        if( recv_val != expected_val ) {
            fprintf(stderr, 
            "[1] Iteration %d: Failure! recv_val(%d) != %d*%d*%d=%d (send_val*i*num_backends)\n",
                    i, recv_val, send_val, i, num_backends, expected_val );
        }
        else {
            fprintf(stdout, 
            "[1] Iteration %d: Success! recv_val(%d) == %d*%d*%d=%d (send_val*i*num_backends)\n",
                    i, recv_val, send_val, i, num_backends, expected_val );
        }

        retval = add_stream2->recv(&tag, p);
        assert( retval != 0 ); //shouldn't be 0, either error or block till data
        if( retval == -1 ) {
            //recv error
            fprintf( stderr, "stream::recv() failure\n");
            return -1;
        }

        if( p->unpack("%d", &recv_val) == -1 ) {
            fprintf( stderr, "stream::unpack() failure\n");
            return -1;
        }
        if( recv_val != expected_val ) {
            fprintf(stderr, 
            "[2] Iteration %d: Failure! recv_val(%d) != %d*%d*%d=%d (send_val*i*num_backends)\n",
                    i, recv_val, send_val, i, num_backends, expected_val );
        }
        else {
            fprintf(stdout, 
            "[2] Iteration %d: Success! recv_val(%d) == %d*%d*%d=%d (send_val*i*num_backends)\n",
                    i, recv_val, send_val, i, num_backends, expected_val );
        }
    }
    delete add_stream;
    delete add_stream2;

    // Tell back-ends to exit
    Stream * ctl_stream = net->new_Stream( comm_BC, TFILTER_MAX,
                                           SFILTER_WAITFORALL );
    if( ctl_stream->send(PROT_EXIT, NULL) == -1 ) {
        fprintf( stderr, "[1] stream::send(exit) failure\n");
        return -1;
    }
    if( ctl_stream->flush() == -1 ) {
        fprintf( stderr, "[1] stream::flush() failure\n");
        return -1;
    }
    retval = ctl_stream->recv( &tag, p );
    if( retval == -1 ) {
        //recv error
        fprintf( stderr, "[1] stream::recv() failure\n");
        return -1;
    }
    delete ctl_stream;

    Stream * ctl_stream2 = net2->new_Stream( comm_BC2, TFILTER_MAX,
                                             SFILTER_WAITFORALL );
    if( ctl_stream2->send(PROT_EXIT, NULL) == -1 ) {
        fprintf( stderr, "[2] stream::send(exit) failure\n");
        return -1;
    }
    if( ctl_stream2->flush() == -1 )  {
        fprintf( stderr, "[2] stream::flush() failure\n");
        return -1;
    }
    retval = ctl_stream2->recv( &tag, p );
    if( retval == -1 ) {
        //recv error
        fprintf( stderr, "[2] stream::recv() failure\n");
        return -1;
    }
    delete ctl_stream2;

    if( tag == PROT_EXIT ) {
        // The Network destructor will cause all internal and leaf tree nodes to exit
        delete net;
        delete net2;
    }

    return 0;
}
