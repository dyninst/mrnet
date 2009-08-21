/****************************************************************************
 * Copyright © 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <bitset>
#include <cmath>
#include <cstring>

#include "mrnet/Packet.h"
#include "mrnet/NetworkTopology.h"

#include "FaultRecovery.h"

using namespace MRN;

extern "C" {

struct IP_state {
    unsigned int min;
    unsigned int max;
    std::bitset<10> percentiles;
};

const char * IntegerPercentiles_format_string = ""; // no format, can recv multiple types
void IntegerPercentiles( std::vector< PacketPtr >& packets_in,
                         std::vector< PacketPtr >& packets_out,
                         std::vector< PacketPtr >& /* packets_out_reverse */,
                         void ** filter_state,
                         PacketPtr& /* params */,
                         const TopologyLocalInfo& )
{
    struct IP_state* state = (struct IP_state*)*filter_state;
    if( *filter_state == NULL ) {
        state = new IP_state;
        state->max = 0;
        state->min = 100;
        *filter_state = (void*)state; 
    }

    std::bitset<10>& aggr_bits = state->percentiles;
    
    for( unsigned int i = 0; i < packets_in.size( ); i++ ) {
        PacketPtr cur_packet = packets_in[i];

        unsigned int cur_min=0;
        unsigned int cur_max=0;
        unsigned int cbits_long=0;
        std::bitset<10> cbits;
 
        const char* cur_fmt = cur_packet->get_FormatString();
        if( ! strcmp("%ud %ud %ud", cur_fmt) ) {

            cur_packet->unpack("%ud %ud %ud", &cbits_long, &cur_max, &cur_min);
            cbits |= std::bitset<10>(cbits_long);

        } else if( ! strcmp("%ud", cur_fmt) ) {

            cur_packet->unpack("%ud", &cur_min);
            assert( cur_min < 100 );
            cur_max = cur_min;

            double p = floor( (double)cur_max / 10 );
            unsigned int ndx = (unsigned int) p;
            assert( ndx < cbits.size() );
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
    
    PacketPtr new_packet ( new Packet( packets_in[0]->get_StreamId(),
                                       PROT_WAVE, "%ud %ud %ud",
                                       (unsigned int)aggr_bits.to_ulong(), state->max, state->min ) );
    packets_out.push_back( new_packet );
}

PacketPtr IntegerPercentiles_get_state( void ** ifilter_state, int istream_id )
{
    PacketPtr packet;
    struct IP_state* state = (struct IP_state*)*ifilter_state;

    if( state == NULL ) {
        fprintf( stderr, "No filter state!\n" );
        packet = Packet::NullPacket;
    }
    else{

        packet = PacketPtr( new Packet( istream_id, PROT_WAVE,
                                        "%ud %ud %ud",
                                        (unsigned int)state->percentiles.to_ulong(),
                                        state->max,
                                        state->min ) );
    }
    return packet;
}

const char * BitsetOr_format_string = "%ud";
void BitsetOr( std::vector< PacketPtr >& packets_in,
               std::vector< PacketPtr >& packets_out,
               std::vector< PacketPtr >&, void **, PacketPtr&,
               const TopologyLocalInfo& )
{
    unsigned long aggr_val = 0;

    for( unsigned int i = 0; i < packets_in.size( ); i++ ) {
        PacketPtr cur_packet = packets_in[i];
        unsigned long cval;
        cur_packet->unpack("%ud", &cval);
        aggr_val |= cval;
    }

    PacketPtr new_packet ( new Packet( packets_in[0]->get_StreamId(),
                                       packets_in[0]->get_Tag(), "%ud",
                                       aggr_val ) );
    packets_out.push_back( new_packet );
}

} /* extern "C" */
