/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "mrnet/MRNet.h"
#include "xplat/Mutex.h"
#include "header.h"

using namespace MRN;
using namespace std;

int num_attach_callbacks = 0;
int num_detach_callbacks = 0;

static XPlat::Mutex cb_lock;
void BE_Add_Callback( Event* evt, void* evt_data )
{
    if( (evt->get_Class() == Event::TOPOLOGY_EVENT) &&
        (evt->get_Type() == TopologyEvent::TOPOL_ADD_BE) ) {
        cb_lock.Lock();
        num_attach_callbacks++;
        cb_lock.Unlock();

        TopologyEvent::TopolEventData* ted = (TopologyEvent::TopolEventData*) evt_data;
        delete ted;
    }
}
void BE_Remove_Callback( Event* evt, void* evt_data )
{
    if( (evt->get_Class() == Event::TOPOLOGY_EVENT) &&
        (evt->get_Type() == TopologyEvent::TOPOL_REMOVE_NODE) ) {
        cb_lock.Lock();
        num_detach_callbacks++;
        cb_lock.Unlock();

        TopologyEvent::TopolEventData* ted = (TopologyEvent::TopolEventData*) evt_data;
        delete ted;
    }
}

void write_be_connections(vector< NetworkTopology::Node * >& leaves, unsigned num_be)
{
   FILE *f;
   const char* connfile = "./attachBE_connections";
   if ( (f = fopen(connfile, (const char *)"w+")) == NULL ) {
      perror("fopen");
      exit(-1);
   }

   unsigned num_leaves = unsigned(leaves.size());
   unsigned be_per_leaf = num_be / num_leaves;
   unsigned curr_leaf = 0;
   for(unsigned i=0; (i < num_be) && (curr_leaf < num_leaves); i++) {
       if( i && (i % be_per_leaf == 0) ) {
           // select next parent
           curr_leaf++;
           if( curr_leaf == num_leaves ) {
               // except when there is no "next"
               curr_leaf--;
           }
       }
       fprintf(stdout, "BE %d will connect to %s:%d:%d\n",
               i,
               leaves[curr_leaf]->get_HostName().c_str(),
               leaves[curr_leaf]->get_Port(),
               leaves[curr_leaf]->get_Rank() );
           
       fprintf(f, "%s %d %d %d\n", 
               leaves[curr_leaf]->get_HostName().c_str(), 
               leaves[curr_leaf]->get_Port(), 
               leaves[curr_leaf]->get_Rank(),
               i);
   }
   fclose(f);
}

int main(int argc, char **argv)
{
    Network * net = NULL;
    Communicator * comm_BC;
    Stream * stream;
    int32_t send_val=57, recv_val=0;

    if( (argc < 3) || (argc > 4) ) {
        fprintf( stderr, "Usage: %s topology_file num_backends [num_threads_per_be]\n", argv[0] );
        exit(-1);
    }
    char* topology_file = argv[1];
    unsigned int num_backends = atoi( argv[2] );

    unsigned int num_be_thrds = 1;
    if( argc == 4 )
        num_be_thrds = atoi( argv[3] );
    
    // If backend_exe (2nd arg) and backend_args (3rd arg) are both NULL,
    // then all nodes specified in the topology are internal tree nodes.
    net = Network::CreateNetworkFE( topology_file, NULL, NULL );
 

    bool cbrett = net->register_EventCallback( Event::TOPOLOGY_EVENT,
                                               TopologyEvent::TOPOL_ADD_BE,
                                               BE_Add_Callback, NULL );
    if(cbrett == false) {
        fprintf( stdout, "Failed to register callback for back-end add topology event\n");
        delete net;
        return -1;
    }
    cbrett = net->register_EventCallback( Event::TOPOLOGY_EVENT,
                                          TopologyEvent::TOPOL_REMOVE_NODE,
                                          BE_Remove_Callback, NULL );
    if(cbrett == false) {
        fprintf( stdout, "Failed to register callback for back-end remove topology event\n");
        delete net;
        return -1;
    }

    // Query net for topology object
    NetworkTopology * topology = net->get_NetworkTopology();
    vector< NetworkTopology::Node * > internal_leaves;
    topology->get_Leaves(internal_leaves);
    topology->print(stdout);

    // Write connection information to temporary file
    write_be_connections( internal_leaves, num_backends );

    // Wait for backends to attach
    unsigned int waitfor_count = num_backends * num_be_thrds;
    fprintf( stdout, "Please start backends now.\n\nWaiting for %u backends to connect\n", 
             waitfor_count );
    fflush(stdout);
    unsigned curr_count = 0;
    do {
        sleep(1);
        cb_lock.Lock();
        curr_count = num_attach_callbacks;
        cb_lock.Unlock();
    } while( curr_count != waitfor_count );
    fprintf( stdout, "All %u backends have attached!\n", waitfor_count);

    // A simple broadcast/gather
    comm_BC = net->get_BroadcastCommunicator();
    stream = net->new_Stream(comm_BC, TFILTER_NULL, SFILTER_DONTWAIT);

    fprintf( stdout, "broadcasting int %d to back-ends\n", send_val );
    if( (stream->send(PROT_INT, "%d", send_val) == -1) ||
        (stream->flush() == -1) ){
        printf("stream::send(%d) failure\n", send_val);
        return -1;
    }
  
    fprintf( stdout, "waiting for response from %d back-ends\n", waitfor_count );
    for( unsigned int i = 0; i < waitfor_count; i++ ){
        int tag;
        PacketPtr p;
  
        int retval = stream->recv(&tag, p, true);
        if( retval == -1){ //recv error
            printf("stream::recv() int failure\n");
            return -1;
        }
  
        if( p->unpack( "%d", &recv_val ) == -1 ){
            printf("stream::unpack() failure\n");
            return -1;
        }
        printf("FE received int = %d\n", recv_val);
    } 

    if( (stream->send(PROT_EXIT, "") == -1) ||
        (stream->flush() == -1) ){
        printf("stream::send_exit() failure\n");
        return -1;
    }

    sleep(1);
    //delete stream;

#if 0 // TESTING detach before shutdown
    fprintf( stdout, "Waiting for %u backends to detach\n", 
             waitfor_count );
    fflush(stdout);
    curr_count = 0;
    do {
        sleep(1);
        cb_lock.Lock();
        curr_count = num_detach_callbacks;
        cb_lock.Unlock();
    } while( curr_count != waitfor_count );
    fprintf( stdout, "All %u backends have detached!\n", waitfor_count);
#endif

    // The Network destructor causes internal and leaf nodes to exit
    delete net;

    return 0;
}
