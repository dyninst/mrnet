// Copyright Paradyn, 2008: contact jolly@cs.wisc.edu for details

#include <string>
using std::string;

#include <vector>
using std::vector;

#include "FilterDefinitions.h"
#include "mrnet/Packet.h"

#include "message_processing.h"

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
    char * buf;
    string serialized_tuple;
    vector<string> filter_outputs;
    vector<string> serialized_tuples;
    vector<tuple> foo;
    for(unsigned int i = 0; i < packets_in.size(); ++i)
    {
        PacketPtr cur_packet = packets_in[i];
        cur_packet->unpack(superfilter_format_string, &buf);
        serialized_tuple = buf;
        serialized_tuples.push_back(serialized_tuple);
        tuple input_tuple = unserialize(serialized_tuple);

        // TODO: process each tuple

        string new_serialized_tuple = serialize(input_tuple);
        filter_outputs.push_back(new_serialized_tuple);
    }

    for(unsigned int i = 0; i < filter_outputs.size(); ++i)
    {
        PacketPtr new_packet(new Packet(
                                packets_in[0]->get_StreamId(),
                                packets_in[0]->get_Tag(),
                                "%s",
                                filter_outputs[i].c_str()
                                )
            );
        packets_out.push_back(new_packet);
    }
}

} // extern "C"

