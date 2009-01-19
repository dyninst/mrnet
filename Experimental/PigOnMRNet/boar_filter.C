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
#include "physical_plan.h"
#include "types_visitor.h"

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
    uint node_rank = network->get_LocalRank();
    INFO << "filter started on node " << node_rank << "\n";

    uint i;
    char * in_buffer = NULL;
    char * out_buffer = NULL;
    string serialized_tuple;
    vector< tuple > output_tuples;

    params->unpack("%s", &in_buffer);
    physical_plan pp;
    pp.load_manifest(in_buffer);
    vector<physical_operator> filter_ops;
    pp.get_operators_for_host(node_rank, filter_ops);

    for(i = 0; i < packets_in.size(); ++i)
    {
        PacketPtr cur_packet = packets_in[i];
        cur_packet->unpack(superfilter_format_string, &in_buffer);
        serialized_tuple = in_buffer;
        if(" " != serialized_tuple)
        {
            INFO << "filter received {" << serialized_tuple << "}\n";
        }
        output_tuples.push_back(unserialize(serialized_tuple));
        free(in_buffer);
    }
    
    // TODO:
    // apply filter_ops to input data here

    for(i = 0; i < output_tuples.size(); ++i)
    {
        out_buffer = copy_into_new_buffer(serialize(output_tuples[i]));
        PacketPtr new_packet( new Packet(
                                packets_in[0]->get_StreamId(),
                                packets_in[0]->get_Tag(),
                                superfilter_format_string,
                                out_buffer
                              ) );
        new_packet->set_DestroyData(true);
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

