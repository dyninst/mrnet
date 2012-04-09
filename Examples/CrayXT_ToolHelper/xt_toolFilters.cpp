/****************************************************************************
 * Copyright 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller   *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/


#include <vector>

#include "mrnet/MRNet.h"
using namespace MRN;

extern "C" {

const char * ToolFilterSum_format_string = "%d";
void ToolFilterSum( std::vector< PacketPtr >& packets_in,
                    std::vector< PacketPtr >& packets_out,
                    std::vector< PacketPtr >& /* packets_out_reverse */,
                    void ** /* client data */,
		    PacketPtr& /* params */,
                    const TopologyLocalInfo& )
{
    int sum = 0;
    
    for( unsigned int i = 0; i < packets_in.size(); i++ ) {
        PacketPtr cur_packet = packets_in[i];
	int val;
	cur_packet->unpack(ToolFilterSum_format_string, &val);
        sum += val;
    }
    
    PacketPtr new_packet ( new Packet(packets_in[0]->get_StreamId(),
                                      packets_in[0]->get_Tag(),
                                      ToolFilterSum_format_string, sum ) );
    packets_out.push_back( new_packet );
}

} /* extern "C" */
