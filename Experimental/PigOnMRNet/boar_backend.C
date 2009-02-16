// Copyright Paradyn, 2008: contact jolly@cs.wisc.edu for details

#include <fstream>
using std::ofstream;

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <sstream>
using std::stringstream;

#include "mrnet/MRNet.h"
#include "mrnet/NetworkTopology.h"
#include "boar.h"
#include "logger.h"

#include "message_processing.h"
#include "input_reader.h"
#include "types_visitor.h"

using namespace MRN;

int
main(
    int argc,
    char ** argv
    )
{
    logger log("backend");
	Network * network = new Network(argc, argv);
    uint node_rank = network->get_LocalRank();
    INFO << "backend started on node " << node_rank << "\n";

	Stream * stream = NULL;
	PacketPtr p;
	int tag = 0;

	do
	{
		if(network->recv(&tag, p, &stream) != 1)
		{
			ERROR << "stream::recv()\n";
			return -1;
		}

        if(START == tag)
        {
            int num_fields;
            char * string_received;
			p->unpack("%s %d", &string_received, &num_fields);
            string startup_string = string_received;
            INFO << "backend got START message '" << startup_string << "'\n";

            vector<tuple> input_tuples;
            input_tuples = read_input(get_shard_filename(startup_string), num_fields);

            uint i;
            for(i = 0; i < input_tuples.size(); ++i)
            {
                if(-1 == stream->send(DATA, "%s", serialize(input_tuples[i]).c_str()))
                {
                    ERROR << "stream::send()\n";
                    return -1;
                }
                if(-1 == stream->flush())
                {
                    ERROR << "stream::flush()\n";
                    return -1;
                }
                INFO << "backend sending {" << serialize(input_tuples[i]).c_str() << "}\n";
            }

            if(-1 == stream->send(DONE, "%s", ""))
            {
                ERROR << "stream::send()\n";
                return -1;
            }
            if(-1 == stream->flush())
            {
                ERROR << "stream::flush()\n";
                return -1;
            }

			INFO << "backend done sending\n";
            break;
        }
        else
        {
            WARN << "received unknown message tag " << tag << "\n";
        }
	}
	while(1);

	return 0;
}

