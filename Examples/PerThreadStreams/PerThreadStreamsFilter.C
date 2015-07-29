/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <cstdio>
#include <cstdlib>
#include "mrnet/MRNet.h"
#include "mrnet/Packet.h"
#include "mrnet/NetworkTopology.h"

#include "PerThreadStreams.h"

#include <cmath>
#include <cstring>

#ifndef __FUNCTION__
#define __FUNCTION__ "nofunction"
#endif

using namespace MRN;

extern "C" {

struct IP_state {
    unsigned int min;
    unsigned int max;
    fr_bin_set percentiles;
};

PTS_API const char * IntegerPercentiles_format_string = ""; // no format, can recv multiple types
PTS_API void IntegerPercentiles( std::vector< PacketPtr >& packets_in,
                         std::vector< PacketPtr >& packets_out,
                         std::vector< PacketPtr >& packets_out_reverse,
                         void ** filter_state,
                         PacketPtr& params,
                         const TopologyLocalInfo& topol_info)
{

    struct IP_state* state = (struct IP_state*)*filter_state;
    if( *filter_state == NULL ) {
        state = new IP_state;
        state->max = 0;
        state->min = fr_range_max;
        *filter_state = (void*)state; 
    }

    fr_bin_set& aggr_bits = state->percentiles;
    int out_tag = -1;

    for( unsigned int i = 0; i < packets_in.size( ); i++ ) {
        PacketPtr cur_packet = packets_in[i];
        // Naively send tag of last packet. Better than previously hard-coded
        // tag, now that this filter is used for more than one tag
        out_tag = cur_packet->get_Tag();

        unsigned int cur_min=0;
        unsigned int cur_max=0;
        uint64_t cbits_long=0;
        fr_bin_set cbits;
 
        const char* cur_fmt = cur_packet->get_FormatString();
        if( ! strcmp("%uld %ud %ud", cur_fmt) ) {

            cur_packet->unpack("%uld %ud %ud", &cbits_long, &cur_max, &cur_min);
            cbits |= fr_bin_set(cbits_long);

        } else if( ! strcmp("%ud", cur_fmt) ) {

            cur_packet->unpack("%ud", &cur_min);
            assert( cur_min < fr_range_max );
            cur_max = cur_min;

            double p = floor( (double)cur_max / (fr_range_max / fr_bins) );
            unsigned int ndx = (unsigned int) p;
            assert( ndx < fr_bins );
            cbits.set(ndx);

        }
        else {
            fprintf(stderr, "%s: invalid format string '%s', skipping packet\n", 
                    __FUNCTION__, cur_fmt);
            continue;
        }

        aggr_bits |= cbits;
        if( cur_min < state->min )
            state->min = cur_min;
        if( cur_max > state->max )
            state->max = cur_max;
    }

    uint64_t ulbits = (uint64_t)aggr_bits.to_ulong();
    PacketPtr new_packet ( new Packet( packets_in[0]->get_StreamId(),
                                       out_tag, "%uld %ud %ud",
                                       ulbits, state->max, state->min ) );
    new_packet->set_DestroyData(true);
    packets_out.push_back( new_packet );
}

PTS_API PacketPtr IntegerPercentiles_get_state( void ** ifilter_state, int istream_id )
{
    PacketPtr packet;
    struct IP_state* state = (struct IP_state*)*ifilter_state;

    if( state == NULL ) {
        fprintf( stderr, "No filter state!\n" );
        packet = Packet::NullPacket;
    }
    else{

        packet = PacketPtr( new Packet( istream_id, PROT_WAVE,
                                        "%uld %ud %ud",
                                        state->percentiles.to_ulong(),
                                        state->max,
                                        state->min ) );
    }
    return packet;
}

PTS_API const char * BitsetOr_format_string = "%uld";
PTS_API void BitsetOr( std::vector< PacketPtr >& packets_in,
               std::vector< PacketPtr >& packets_out,
               std::vector< PacketPtr >& packets_out_reverse,
               void ** filter_state,
               PacketPtr& params,
               const TopologyLocalInfo& topol_info)
{
    uint64_t aggr_val = 0;

    for( unsigned int i = 0; i < packets_in.size( ); i++ ) {
        PacketPtr cur_packet = packets_in[i];
        uint64_t cval;
        cur_packet->unpack("%uld", &cval);
        aggr_val |= cval;
    }

    PacketPtr new_packet ( new Packet( packets_in[0]->get_StreamId(),
                                       packets_in[0]->get_Tag(), "%uld",
                                       aggr_val ) );
    packets_out.push_back( new_packet );
}

} /* extern "C" */
