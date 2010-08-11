/****************************************************************************
 * Copyright © 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "mrnet/MRNet.h"
#include "header.h"

using namespace MRN;
using namespace std;
int num_callbacks;

void Callback_resTopo(CBClass icbcl,CBType icbt, EventCB* icl)
{
        TopoEvent* te = (TopoEvent*)icl;

        //fprintf(stdout,"ResTopo: The Rank of node in topology add BE is= %d \t \t\n\n",te->get_Rank());

	num_callbacks++;
}

void write_be_connections(vector< NetworkTopology::Node * >& leaves, unsigned num_be)
{
   FILE *f;
   const char* connfile = "./attachBE_connections";
   if ((f = fopen(connfile, (const char *)"w+")) == NULL)
   {
      perror("fopen");
      exit(-1);
   }

   unsigned num_leaves = leaves.size();
   unsigned be_per_leaf = num_be / num_leaves;
   unsigned curr_leaf = 0;
   for(unsigned i=0; (i < num_be) && (curr_leaf < num_leaves); i++)
   {
      if( i && (i % be_per_leaf == 0) )
         curr_leaf++;

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

    if( argc != 3 ) {
        fprintf( stderr, "Usage: %s <topology_file> <num_backends>\n", argv[0] );
        exit(-1);
    }
    char* topology_file = argv[1];
    unsigned int num_backends = atoi( argv[2] );
    
    // If backend_exe (2nd arg) and backend_args (3rd arg) are both NULL,
    // then all nodes specified in the topology are internal tree nodes.
    net = Network::CreateNetworkFE( topology_file, NULL, NULL );
 

    bool cbrett = net->register_Callback(TOPOLOGY_EVENT_CB,Callback_resTopo,TOPO_ADD_BE);
    if(cbrett==true){
        fprintf(stdout,"Register Callback for add backend success\n");

        }
        else
                fprintf(stdout,"Register Callback func for add backend error\n");

    // Query net for topology object
    NetworkTopology * topology = net->get_NetworkTopology();
    vector< NetworkTopology::Node * > internal_leaves;
    topology->get_Leaves(internal_leaves);
    topology->print(stdout);

    // Write connection information to temporary file
    write_be_connections( internal_leaves, num_backends );


    // Wait for backends to attach
    fprintf( stdout, "Please start backends now.\n\nWaiting for %u backends to connect ...", num_backends );
    fflush(stdout);
    do {
        sleep(1);
    } while( num_callbacks!=num_backends );
    fprintf( stdout, "complete!\n");


    // A simple broadcast/gather
    comm_BC = net->get_BroadcastCommunicator();
    stream = net->new_Stream(comm_BC, TFILTER_NULL, SFILTER_DONTWAIT);

    fprintf( stdout, "broadcasting int %d to back-ends\n", send_val );
    if( (stream->send(PROT_INT, "%d", send_val) == -1) ||
        (stream->flush() == -1) ){
        printf("stream::send(%d) failure\n", send_val);
        return -1;
    }
  
    fprintf( stdout, "waiting for response from %d back-ends\n", num_backends );
    for( unsigned int i = 0; i < num_backends; i++ ){
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
  
    // The Network destructor causes internal and leaf nodes to exit
    delete net;

    return 0;
}
