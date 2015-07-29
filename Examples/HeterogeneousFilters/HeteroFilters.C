/**************************************************************************
 * Copyright 2003-2015   Michael J. Brim, Barton P. Miller                *
 *                Detailed MRNet usage rights in "LICENSE" file.          *
 **************************************************************************/

#include <set>
#include <algorithm>

#include "mrnet/Packet.h"
#include "mrnet/NetworkTopology.h"

#include "HeteroFilters.h"

using namespace MRN;

const char* invalid_search = "NO SEARCH STRING PROVIDED";

extern "C" {

const char * HF_BE_scan_format_string = "%ac";
void HF_BE_scan( std::vector< PacketPtr >& pin,
                 std::vector< PacketPtr >& pout,
                 std::vector< PacketPtr >& /* packets_out_reverse */,
                 void ** /* client data */,
                 PacketPtr& params,
                 const TopologyLocalInfo& )
{
    char* search = NULL;
    if( params != Packet::NullPacket ) {
        if( params->unpack("%s", &search) == -1 ) {
            fprintf(stderr, "Error: filter %s - params->unpack() failure\n", __FUNCTION__);
            pout = pin;
            return;
        }
    }
    else
        search = strdup( invalid_search );

    //fprintf(stdout, "%s - search string is '%s'\n", __FUNCTION__, search);
    //fflush(stdout);

    vector<string> matches;
    for( size_t i = 0; i < pin.size( ); i++ ) {
        PacketPtr cur_packet = pin[i];
        char* data;
        unsigned dataLen;
        cur_packet->unpack( "%ac", &data, &dataLen );
        if( ! dataLen ) continue;

        unsigned nlines;
        vector<unsigned> line_offsets;
        get_lines(data, dataLen, line_offsets);
        nlines = unsigned(line_offsets.size());

        for( ; i < nlines; i++ ) {

            unsigned tmp_len = ( i+1 < nlines ?
                                 line_offsets[i+1] - line_offsets[i] :
                                 dataLen - line_offsets[i] );
            string tmp(data + line_offsets[i], tmp_len);

            if( tmp.find(search) != tmp.npos )
                matches.push_back(tmp);
        }
	free( data );
    }
    if( search != NULL) free(search);

    char** match_arr = NULL;
    if( matches.size() ) {
        match_arr = (char**) malloc( matches.size() * sizeof(char*) );
        for( unsigned u = 0; u < matches.size( ); u++ )
            match_arr[u] = strdup( matches[u].c_str() );
    }
    PacketPtr new_packet ( new Packet(pin[0]->get_StreamId(),
                                      pin[0]->get_Tag(), 
                                      "%as", match_arr, matches.size() ) );
    pout.push_back( new_packet );
}

const char * HF_CP_sort_format_string = "%as";
void HF_CP_sort( std::vector< PacketPtr >& pin,
                 std::vector< PacketPtr >& pout,
                 std::vector< PacketPtr >& /* pout_reverse */,
                 void ** /* client data */,
                 PacketPtr& /* params */,
                 const TopologyLocalInfo& )
{
    vector< string > sorted;
    
    for( unsigned i = 0; i < pin.size( ); i++ ) {
        char** strArr;
        unsigned arrLen;
        PacketPtr cur_packet = pin[i];
        cur_packet->unpack( "%as", &strArr, &arrLen );
        if( arrLen == 0 ) continue;
        
	for( unsigned u=0; u < arrLen; u++ ) {
            sorted.push_back( string(strArr[u]) );
	    free( strArr[u] );
	}
	free( strArr );
    }

    char** sort_arr = NULL;
    if( sorted.size() ) {
        sort_arr = (char**) malloc( sorted.size() * sizeof(char*) );
        std::sort( sorted.begin(), sorted.end() );
        std::vector< string >::iterator iter = sorted.begin();
        for( unsigned u=0; iter != sorted.end() ; iter++, u++ )
            sort_arr[u] = strdup( iter->c_str() );
    }
    PacketPtr new_packet ( new Packet(pin[0]->get_StreamId(),
                                      pin[0]->get_Tag(), 
                                      "%as", sort_arr, sorted.size() ) );
    pout.push_back( new_packet );
}

const char * HF_FE_uniq_format_string = "%as";
void HF_FE_uniq( std::vector< PacketPtr >& pin,
                 std::vector< PacketPtr >& pout,
                 std::vector< PacketPtr >& pout_reverse,
                 void **filter_state,
                 PacketPtr& /* params */,
                 const TopologyLocalInfo& info )
{
    std::set< string >* uniq = (std::set< string >*)*filter_state;
    if( uniq == NULL ) {
        uniq = new std::set< string >();
        *filter_state = (void*)uniq;
    }

    std::vector< PacketPtr > tmp_out;
    HF_CP_sort( pin, tmp_out, pout_reverse, NULL, Packet::NullPacket, info );

    PacketPtr& sorted = tmp_out.front();
    char** strArr;
    unsigned arrLen;
    sorted->unpack( "%as", &strArr, &arrLen );

    std::vector< string > new_uniq;
    for( unsigned u=0; u < arrLen; u++ ) {
        string tmp( strArr[u] );
        std::pair< std::set<string>::iterator, bool > result = uniq->insert( tmp );
        if( result.second )
            new_uniq.push_back( tmp );
	free( strArr[u] );
    }
    if( strArr ) free( strArr );
                     
    char** uniq_arr = NULL;
    if( new_uniq.size() ) {
        uniq_arr = (char**) malloc( new_uniq.size() * sizeof(char*) );
        std::vector< string >::iterator iter = new_uniq.begin();
        for( unsigned u=0; iter != new_uniq.end() ; iter++, u++ )
            uniq_arr[u] = strdup( iter->c_str() );
    }
    PacketPtr new_packet ( new Packet(pin[0]->get_StreamId(),
                                      pin[0]->get_Tag(), 
                                      "%as", uniq_arr, new_uniq.size() ) );
    pout.push_back( new_packet );
}

} /* extern "C" */
