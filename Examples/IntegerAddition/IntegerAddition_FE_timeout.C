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

    if( (argc != 4) && (argc != 5) ){
        fprintf(stderr, "Usage: %s <topology file> <backend_exe> <so_file> [<num net instances>]\n", argv[0]);
        exit(-1);
    }
    const char * topology_file = argv[1];
    const char * backend_exe = argv[2];
    const char * so_file = argv[3];
    const char * dummy_argv=NULL;

    int nets = 1;
    if( argc == 5 )
        nets = atoi( argv[4] );

    int n = 0;
    while( n++ < nets ) {

        if( nets > 1 )
            fprintf(stdout, "\n\n---------- Network Instance %d ----------\n\n", n);

        // This Network() cnstr instantiates the MRNet internal nodes, according to the
        // organization in "topology_file," and the application back-end with any
        // specified cmd line args
        Network * net = Network::CreateNetworkFE( topology_file, backend_exe, &dummy_argv );
        if( net->has_Error() ) {
            net->perror("Network creation failed");
            exit(-1);
        }

        // Make sure path to "so_file" is in LD_LIBRARY_PATH
        int filter_id = net->load_FilterFunc( so_file, "IntegerAdd" );
        if( filter_id == -1 ){
            fprintf( stderr, "Network::load_FilterFunc() failure\n");
            delete net;
            return -1;
        }

        // A Broadcast communicator contains all the back-ends
        Communicator * comm_BC = net->get_BroadcastCommunicator( );

        // Create a stream that will use the Integer_Add filter for aggregation
        Stream * add_stream = net->new_Stream( comm_BC, filter_id,
                                               SFILTER_TIMEOUT );
	add_stream->set_FilterParameters( FILTER_UPSTREAM_SYNC, "%ud", 250 );

        int num_backends = int(comm_BC->get_EndPoints().size());

        // Broadcast a control message to back-ends to send us "num_iters"
        // waves of integers
        tag = PROT_SUM;
        unsigned int num_iters=5;
        if( add_stream->send( tag, "%d %d", send_val, num_iters ) == -1 ){
            fprintf( stderr, "stream::send() failure\n");
            return -1;
        }
        if( add_stream->flush( ) == -1 ){
            fprintf( stderr, "stream::flush() failure\n");
            return -1;
        }

 
	// Collect responses
	
        // Clear events up until this point
        net->clear_Events();

        // Request notification of data events
        int data_fd = net->get_EventNotificationFd( Event::DATA_EVENT );
        int max_fd = data_fd + 1;

        // We expect "num_iters" aggregated responses from all back-ends
        unsigned int i=0;
        int total_sum = 0;
        int expected_total = 0;
        for( ; i < num_iters; i++ )
            expected_total += i * send_val * num_backends;

        do {
            // Wait using one second intervals
            struct timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            // Initialize read fd set
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(data_fd, &readfds);

            fprintf(stderr, "FE: Waiting for data to arrive...\n");
            fflush(stderr);
            retval = select(max_fd, &readfds, NULL, NULL, &timeout);
            if( retval < 0 )
                perror("select");
            else if(retval > 0) {

                fprintf(stderr, "FE: Checking for data ... ");
	
                if( FD_ISSET(data_fd, &readfds) ) {

                    fprintf(stderr, "found\n");

                    // Handle data events
                    net->clear_EventNotificationFd( Event::DATA_EVENT );
                    while(1) {
                        retval = add_stream->recv(&tag, p, false);
                        if( retval == -1){
                            //recv error
                            return -1;
                        }
                        else if( retval == 0 )
                            break;

                        if( p->unpack( "%d", &recv_val ) == -1 ){
                            fprintf( stderr, "stream::unpack() failure\n");
                            return -1;
                        }

                        total_sum += recv_val;
                    }
                }
                else
                    fprintf(stderr, "not found\n");
                fflush(stderr);
                
            
                // Clear events up until this point
                net->clear_Events();
            }
            else {
                fprintf(stderr, "FE: Timed out.\n");
                fflush(stderr);
            }
        } while( total_sum < expected_total );

        if( total_sum == expected_total )
            fprintf(stderr, "FE: Success\n");
	else
	    fprintf(stderr, "FE: Failure: total sum %d != expected sum %d.\n",
	            total_sum, expected_total);

        delete add_stream;

        // Tell back-ends to exit
        Stream * ctl_stream = net->new_Stream( comm_BC, TFILTER_MAX,
                                               SFILTER_WAITFORALL );
        if(ctl_stream->send(PROT_EXIT, "") == -1){
            fprintf( stderr, "stream::send(exit) failure\n");
            return -1;
        }
        if(ctl_stream->flush() == -1){
            fprintf( stderr, "stream::flush() failure\n");
            return -1;
        }
        retval = ctl_stream->recv(&tag, p);
        if( retval == -1){
            //recv error
            fprintf( stderr, "stream::recv() failure\n");
            return -1;
        }
        delete ctl_stream;
        if( tag == PROT_EXIT ) {
            net->close_EventNotificationFd( Event::DATA_EVENT );

            // The Network destructor will cause all internal and leaf tree nodes to exit
            delete net;
        }

        sleep(5);

    }


    return 0;
}
