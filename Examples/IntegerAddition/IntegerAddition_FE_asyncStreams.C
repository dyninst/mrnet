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
    int send_val2=16, recv_val2=0;
    int tag, retval;
    PacketPtr p;

    if( argc != 4 ){
        fprintf(stderr, "Usage: %s <topology file> <backend_exe> <so_file>\n", argv[0]);
        exit(-1);
    }
    const char * topology_file = argv[1];
    const char * backend_exe = argv[2];
    const char * so_file = argv[3];
    const char * dummy_argv=NULL;

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
                                           SFILTER_WAITFORALL );
    Stream * add_stream2 = net->new_Stream( comm_BC, filter_id,
                                            SFILTER_WAITFORALL );

    int num_backends = int(comm_BC->get_EndPoints().size());

    tag = PROT_SUM;
    unsigned int num_iters=5;
    // Broadcast a control message to back-ends to send us "num_iters"
    // waves of integers
    if( add_stream->send( tag, "%d %d", send_val, num_iters ) == -1 ){
        fprintf( stderr, "stream::send() failure\n");
        return -1;
    }
    if( add_stream->flush( ) == -1 ){
        fprintf( stderr, "stream::flush() failure\n");
        return -1;
    }
    if( add_stream2->send( tag, "%d %d", send_val2, num_iters ) == -1 ){
        fprintf( stderr, "stream::send() failure\n");
        return -1;
    }
    if( add_stream2->flush( ) == -1 ){
        fprintf( stderr, "stream::flush() failure\n");
        return -1;
    }

    // Clear events up until this point
    net->clear_Events();

    // Request notification of data events
    int data_fd = add_stream->get_DataNotificationFd();
    int data_fd2 = add_stream2->get_DataNotificationFd();
    int max_fd = data_fd;
    if( data_fd2 > data_fd )
        max_fd = data_fd2;
    max_fd++;

    // We expect "num_iters" aggregated responses from all back-ends
    unsigned int i=0;
    while( i < (num_iters * 2) ) {

        // Wait using five second intervals
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        // Initialize read fd set
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(data_fd, &readfds);
        FD_SET(data_fd2, &readfds);

        fprintf(stderr, "FE: Waiting for data to arrive...\n");
	fflush(stderr);
        retval = select(max_fd, &readfds, NULL, NULL, &timeout);
        if( retval < 0 )
            perror("select");
        else if(retval > 0) {

            fprintf(stderr, "FE: Checking for data ... ");
	
            if( FD_ISSET(data_fd, &readfds) ) {

                fprintf(stderr, "found on stream 1\n");

                // Handle data events
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

                    int expected_val = num_backends * i * send_val;
                    if( recv_val != expected_val ){
                        fprintf(stderr, "Iteration %d: Failure! recv_val(%d) != %d*%d*%d=%d (send_val*i*num_backends)\n",
                                i, recv_val, send_val, i, num_backends, expected_val );
                        fflush(stderr);
                    }
                    else{
                        fprintf(stderr, "Iteration %d: Success! recv_val(%d) == %d*%d*%d=%d (send_val*i*num_backends)\n",
                                i, recv_val, send_val, i, num_backends, expected_val );
                        fflush(stderr);
                    }
                    i++;
                }
                
            }
            if( FD_ISSET(data_fd2, &readfds) ) {

                fprintf(stderr, "found on stream 2\n");

                // Handle data events
                while(1) {
                    retval = add_stream2->recv(&tag, p, false);
                    if( retval == -1){
                        //recv error
                        return -1;
                    }
                    else if( retval == 0 )
                        break;

                    if( p->unpack( "%d", &recv_val2 ) == -1 ){
                        fprintf( stderr, "stream::unpack() failure\n");
                        return -1;
                    }

                    int expected_val = num_backends * (i-num_iters) * send_val2;
                    if( recv_val2 != expected_val ){
                        fprintf(stderr, "Iteration %d: Failure! recv_val(%d) != %d*%d*%d=%d (send_val*i*num_backends)\n",
                                (i-num_iters), recv_val2, send_val2, (i-num_iters), num_backends, expected_val );
                        fflush(stderr);
                    }
                    else{
                        fprintf(stderr, "Iteration %d: Success! recv_val(%d) == %d*%d*%d=%d (send_val*i*num_backends)\n",
                                (i-num_iters), recv_val2, send_val2, (i-num_iters), num_backends, expected_val );
                        fflush(stderr);
                    }
                    i++;
                }
            } 
            fflush(stderr);
                
            // Clear events up until this point
            net->clear_Events();
        }
        else {
            fprintf(stderr, "FE: Timed out.\n");
            fflush(stderr);
        }
    }
    delete add_stream;
    delete add_stream2;

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
        // The Network destructor will cause all internal and leaf tree nodes to exit
        delete net;
    }

    return 0;
}
