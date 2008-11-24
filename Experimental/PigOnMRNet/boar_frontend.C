// Copyright Paradyn, 2008: contact jolly@cs.wisc.edu for details

#include "mrnet/MRNet.h"
#include "boar.h"
#include "logger.h"
#include "physical_plan.h"

#include <fstream>
using std::ofstream;

#include <sstream>
using std::stringstream;

using namespace MRN;

// 'boar' presently reads tuples of a relation sharded across many backends.
// It concatenates the shards together in parallel and outputs them at the frontend.
// This code is a dirt-simple starting point for my Pig-on-MRNet work.
// The next step is build the pipeline code to place in the 'superfilter'.
int
main(
    int argc,
    char ** argv
    )
{
    logger log("frontend");

	if(6 != argc)
	{
		ERROR
            << "incorrect arguments, try "
            << argv[0]
            << " <topology file> <backend_exe> <so_file> <shard_filename> <num_fields>\n";
		return -1;
	}

	const char * topology_file = argv[1];
	const char * backend_exe = argv[2];
	const char * so_file = argv[3];
	const char * dummy_argv = NULL;

	Network * network = new Network(topology_file, backend_exe, &dummy_argv);
	int filter_id = network->load_FilterFunc(so_file, "superfilter");
	if(-1 == filter_id)
	{
		ERROR << "unable to load filter function\n";
		delete network;
		return -1;
	}

    uint node_rank = network->get_LocalRank();
    INFO << "frontend started on node " << node_rank << "\n";    

	Communicator * comm_BC = network->get_BroadcastCommunicator();
	Stream * stream = network->new_Stream(comm_BC, filter_id, SFILTER_DONTWAIT);
    int num_backends = comm_BC->get_EndPoints().size();
    INFO << "attempting to fetch data from " << num_backends << " backends\n";

    // TODO:
    // get logical plan from Hadoop
    // translate into a physical_plan for given MRNet TBON
    // send a description of the plan to all the nodes so they can act accordingly 
    physical_plan plan;
    stream->set_FilterParameters(true, "%s", plan.get_manifest().c_str());

    // TODO:
    // nail down the format of a START message that contains:
    // * just the fields of interest in the relation
    // * a field-to-field delimiter of choice
    // * a tuple-to-tuple delimiter of choice
    // etc.
    // at the moment, we send the number of fields in the relation
    // and filename that each shard of the relation should be stored in
	if(-1 == stream->send(START, "%s %d", argv[4], atoi(argv[5])))
	{
		ERROR << "stream::send()\n";
		return -1;
	}
	if(-1 == stream->flush())
	{
		ERROR << "stream::flush()\n";
		return -1;
	}
    
    char * buf;
    PacketPtr p;
    int tag;
    int num_replies(0);

    while(1)
    {
        int recv_status = stream->recv(&tag, p);
        if(0 == recv_status || -1 == recv_status)
        {
            ERROR << "bad recv_status\n";
            return -1;
        }

        if(DONE == tag)
        {
            ++num_replies;
            if(num_replies >= num_backends)
            {
                INFO << "received DONE message from all backends\n";
                break;
            }
            continue;
        }

        if(-1 == p->unpack("%s", &buf))
        {
            ERROR << "stream::unpack()\n";
            return -1;
        }
        INFO << "frontend received tuple {" << buf << "}\n";
    }

	delete network; // force all nodes to exit
	INFO << "network closed at frontend\n";
	return 0;
}

