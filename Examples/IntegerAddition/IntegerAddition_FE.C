/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "mrnet/MRNet.h"
#include "IntegerAddition.h"

using namespace MRN;

bool saw_failure = false;
void Failure_Callback( Event* evt, void* )
{
    if( (evt->get_Class() == Event::TOPOLOGY_EVENT) &&
        (evt->get_Type() == TopologyEvent::TOPOL_REMOVE_NODE) )
        saw_failure = true;
}

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

        saw_failure = false;

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

        if( ! net->set_FailureRecovery(false) ) {
            fprintf( stdout, "Failed to disable failure recovery\n" );
            delete net;
            return -1;
        }
        bool cbrett = net->register_EventCallback( Event::TOPOLOGY_EVENT,
                                                   TopologyEvent::TOPOL_REMOVE_NODE,
                                                   Failure_Callback, NULL );
        if( cbrett == false ) {
            fprintf( stdout, "Failed to register callback for node failure topology event\n" );
            delete net;
            return -1;
        }
        
        // Make sure path to "so_file" is in LD_LIBRARY_PATH
        int filter_id = net->load_FilterFunc( so_file, "IntegerAdd" );
        if( filter_id == -1 ){
            fprintf( stderr, "Network::load_FilterFunc() failure\n" );
            delete net;
            return -1;
        }

        // A Broadcast communicator contains all the back-ends
        Communicator * comm_BC = net->get_BroadcastCommunicator( );

        // Create a stream that will use the Integer_Add filter for aggregation
        Stream * add_stream = net->new_Stream( comm_BC, filter_id,
                                               SFILTER_WAITFORALL );
        
        int num_backends = int(comm_BC->get_EndPoints().size());

        // Broadcast a control message to back-ends to send us "num_iters"
        // waves of integers
        tag = PROT_SUM;
        unsigned int num_iters=5;
        if( add_stream->send( tag, "%d %d", send_val, num_iters ) == -1 ){
            fprintf( stderr, "stream::send() failure\n" );
            return -1;
        }
        if( add_stream->flush( ) == -1 ){
            fprintf( stderr, "stream::flush() failure\n" );
            return -1;
        }

        // We expect "num_iters" aggregated responses from all back-ends
        for( unsigned int i=0; i < num_iters; i++ ){

            retval = add_stream->recv(&tag, p);
            if( retval == 0 ) {
                //shouldn't be 0, either error or block for data, unless a failure occured
                fprintf( stderr, "stream::recv() returned zero\n" );
                if( saw_failure ) break;
                return -1;
            }
            if( retval == -1 ) {
                //recv error
                fprintf( stderr, "stream::recv() unexpected failure\n" );
                if( saw_failure ) break;
                return -1;
            }

            if( p->unpack( "%d", &recv_val ) == -1 ){
                fprintf( stderr, "stream::unpack() failure\n" );
                return -1;
            }

            int expected_val = num_backends * i * send_val;
            if( recv_val != expected_val ){
                fprintf(stderr, "FE: Iteration %d: Failure! recv_val(%d) != %d*%d*%d=%d (send_val*i*num_backends)\n",
                        i, recv_val, send_val, i, num_backends, expected_val );
            }
            else{
                fprintf(stdout, "FE: Iteration %d: Success! recv_val(%d) == %d*%d*%d=%d (send_val*i*num_backends)\n",
                        i, recv_val, send_val, i, num_backends, expected_val );
            }
        }

        if( saw_failure ) {
            fprintf( stderr, "FE: a network process has failed, killing network\n" );
            delete net;
        }
        else {
            delete add_stream;

            // Tell back-ends to exit
            Stream * ctl_stream = net->new_Stream( comm_BC, TFILTER_MAX,
                                                   SFILTER_WAITFORALL );
            if(ctl_stream->send(PROT_EXIT, "") == -1){
                fprintf( stderr, "stream::send(exit) failure\n" );
                return -1;
            }
            if(ctl_stream->flush() == -1){
                fprintf( stderr, "stream::flush() failure\n" );
                return -1;
            }
            retval = ctl_stream->recv(&tag, p);
            if( retval == -1){
                //recv error
                fprintf( stderr, "stream::recv() failure\n" );
                return -1;
            }
            delete ctl_stream;
            if( tag == PROT_EXIT ) {
                // The Network destructor will cause all internal and leaf tree nodes to exit
                delete net;
            }
        }

        sleep(5);
    }


    return 0;
}
