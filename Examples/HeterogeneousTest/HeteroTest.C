/**************************************************************************
 * Copyright 2003-2009   James Jolly, Michael J. Brim, Barton P. Miller   *
 *                Detailed MRNet usage rights in "LICENSE" file.          *
 **************************************************************************/

#include "HeteroTest.h"
using namespace MRN;

extern "C" {

inline
char*
copy_into_new_buffer(
    const string& to_convert
    )
{
    uint i;
    char* buf;
    buf = (char*)malloc(to_convert.size( ) + 1);  
    for(i = 0; i < to_convert.size( ); ++i)
    {
        buf[i] = to_convert[i];
    }
    buf[i] = '\0';
    return buf;
}

inline
string
int_to_str(
    int i
    )
{
    ostringstream oss("");
    oss << i;
    return oss.str( );
}

void inject_history_item(
    vector< PacketPtr >& packets_in,
    vector< PacketPtr >& packets_out,
    char marker
    )
{
    uint i;
    char * in_buffer = NULL;
    char * out_buffer = NULL;
    string history_str;
    vector< string > output_tuples;
    int node_rank = network->get_LocalRank( );
    for( i = 0; i < packets_in.size( ); ++i ){
        PacketPtr cur_packet = packets_in[i];
        cur_packet->unpack("%s", &in_buffer);
        history_str = in_buffer;
        free(in_buffer);
        history_str += "(";
        history_str += int_to_str(node_rank);
        history_str += ",";
        history_str += marker;
        history_str += ")";
        out_buffer = copy_into_new_buffer(history_str);
        PacketPtr new_packet( new Packet(
                                packets_in[0]->get_StreamId( ),
                                packets_in[0]->get_Tag( ),
                                "%s",
                                out_buffer
                              ) );
        new_packet->set_DestroyData(true);
        if( new_packet->has_Error( ) ){
            continue;
        }
        packets_out.push_back(new_packet);
    } 
}

const char * insertX_format_string = "%s";
void insertX( vector< PacketPtr >& packets_in,
              vector< PacketPtr >& packets_out,
              vector< PacketPtr >& /* packets_out_reverse */,
              void ** /* client data */,
              PacketPtr& /* params */ )
{
    inject_history_item(packets_in, packets_out, 'X');
}

const char * insertY_format_string = "%s";
void insertY( vector< PacketPtr >& packets_in,
              vector< PacketPtr >& packets_out,
              vector< PacketPtr >& /* packets_out_reverse */,
              void ** /* client data */,
              PacketPtr& /* params */ )
{
    inject_history_item(packets_in, packets_out, 'Y');
}

const char * insertI_format_string = "%s";
void insertI( vector< PacketPtr >& packets_in,
              vector< PacketPtr >& packets_out,
              vector< PacketPtr >& /* packets_out_reverse */,
              void ** /* client data */,
              PacketPtr& /* params */ )
{
    inject_history_item(packets_in, packets_out, 'I');
}

const char * insertJ_format_string = "%s";
void insertJ( vector< PacketPtr >& packets_in,
              vector< PacketPtr >& packets_out,
              vector< PacketPtr >& /* packets_out_reverse */,
              void ** /* client data */,
              PacketPtr& /* params */ )
{
    inject_history_item(packets_in, packets_out, 'J');
}

} /* extern "C" */
