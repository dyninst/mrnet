// Copyright Paradyn, 2008: contact jolly@cs.wisc.edu for details

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <fstream>
using std::ofstream;

#include "FilterDefinitions.h"
#include "mrnet/MRNet.h"
#include "mrnet/Packet.h"

#include "message_processing.h"
#include "logger.h"

using namespace MRN;

extern "C"
{

const char * superfilter_format_string = "%s";

void
superfilter(
    std::vector< PacketPtr >& packets_in,
    std::vector< PacketPtr >& packets_out,
    std::vector< PacketPtr >& packets_out_reverse,
    void ** client_data,
    PacketPtr& params
    )
{
    logger log("filter");
    int node_rank = 42; // TEMP: Setting static filter rank across all filters.
                        //       How do we properly discover rank within a filter?
    INFO << "filter started on node " << node_rank << "\n";

    uint i;
    char * buf;
    string serialized_tuple;
    vector<string> filter_outputs;
    vector<string> serialized_tuples;
    vector< tuple > input_tuples;

    params->unpack("%s", &buf);
    INFO << "filter params: " << buf << "\n";

    for(i = 0; i < packets_in.size(); ++i)
    {
        PacketPtr cur_packet = packets_in[i];
        cur_packet->unpack(superfilter_format_string, &buf);
        serialized_tuple = buf;
        INFO << "adding {" << serialized_tuple << "}\n";
        input_tuples.push_back(unserialize(serialized_tuple));
    }
    
    // TODO:
    //
    // feed tuples for processing into microfilters here

    for(i = 0; i < input_tuples.size(); ++i)
    {
        filter_outputs.push_back(serialize(input_tuples[i]));
        PacketPtr new_packet( new Packet(
                                packets_in[0]->get_StreamId(),
                                packets_in[0]->get_Tag(),
                                superfilter_format_string,
                                filter_outputs[i].c_str()
                              ) );
        if(new_packet->has_Error())
        {
            ERROR << "new_packet() failed, dropping tuple\n";
            continue;
        }
        packets_out.push_back(new_packet);
    }

    INFO << "filter end\n";
}

} // extern "C"

