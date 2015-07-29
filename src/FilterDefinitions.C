
/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <map>
#include <set>
#include <vector>
#include <string>

#include "mrnet/MRNet.h"
#include "mrnet/DataElement.h"

#include "FilterDefinitions.h"
#include "utils.h"
#include "PeerNode.h"
#include "PerfDataEvent.h"
#include "TimeKeeper.h"

typedef unsigned char uchar_t;

using namespace std;

namespace MRN
{

FilterId TFILTER_NULL=0;
const char* TFILTER_NULL_FORMATSTR = NULL_STRING;  // Don't check fmt string

FilterId TFILTER_SUM=0;
const char* TFILTER_SUM_FORMATSTR = NULL_STRING;  // Don't check fmt string

FilterId TFILTER_AVG=0;
const char* TFILTER_AVG_FORMATSTR = NULL_STRING;  // Don't check fmt string

FilterId TFILTER_MIN=0;
const char* TFILTER_MIN_FORMATSTR = NULL_STRING;  // Don't check fmt string

FilterId TFILTER_MAX=0;
const char* TFILTER_MAX_FORMATSTR = NULL_STRING;  // Don't check fmt string

FilterId TFILTER_ARRAY_CONCAT=0;
const char* TFILTER_ARRAY_CONCAT_FORMATSTR = NULL_STRING; // Don't check fmt string

FilterId TFILTER_TOPO_UPDATE=0;
const char* TFILTER_TOPO_UPDATE_FORMATSTR = NULL_STRING; // Don't check fmt string

FilterId TFILTER_TOPO_UPDATE_DOWNSTREAM=0;
const char* TFILTER_TOPO_UPDATE_DOWNSTREAM_FORMATSTR = NULL_STRING; // Don't check fmt string

FilterId TFILTER_INT_EQ_CLASS=0;
const char* TFILTER_INT_EQ_CLASS_FORMATSTR = "%aud %aud %aud";

FilterId TFILTER_PERFDATA=0;
const char* TFILTER_PERFDATA_FORMATSTR = NULL_STRING; // Don't check fmt string

FilterId SFILTER_WAITFORALL=0;
FilterId SFILTER_DONTWAIT=0;
FilterId SFILTER_TIMEOUT=0;

static inline void mrn_max(const void *in1, const void *in2, void* out, DataType type);
static inline void mrn_min(const void *in1, const void *in2, void* out, DataType type);
static inline void div(const void *in1, int in2, void* out, DataType type);
static inline void mult(const void *in1, int in2, void* out, DataType type);
static inline void sum(const void *in1, const void *in2, void* out, DataType type);

/*=====================================================*
 *    Default Transformation Filter Definitions        *
 *=====================================================*/
void tfilter_IntSum( const vector< PacketPtr >& ipackets,
                     vector< PacketPtr >& opackets,
                     vector< PacketPtr >& /* opackets_reverse */,
                     void ** /* client data */, PacketPtr&,
                     const TopologyLocalInfo& )
{
    int sum = 0;
    
    for( unsigned int i = 0; i < ipackets.size( ); i++ ) {
        PacketPtr cur_packet( ipackets[i] );
        sum += (*cur_packet)[0]->get_int32_t( );
    }
    
    PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                      ipackets[0]->get_Tag( ),
                                      "%d", sum ) );
    opackets.push_back( new_packet );
}

void tfilter_Sum( const vector< PacketPtr >& ipackets,
                  vector< PacketPtr >& opackets,
                  vector< PacketPtr >& /* opackets_reverse */,
                  void ** /* client data */, PacketPtr&,
                  const TopologyLocalInfo& )
{
    char result[8]; //ptr to 8 bytes
    string format_string;
    DataType type=UNKNOWN_T;

    memset(result, 0, 8); //zeroing the result buf
    for( unsigned int i = 0; i < ipackets.size( ); i++ ) {
        PacketPtr cur_packet( ipackets[i] );

        assert( strcmp(cur_packet->get_FormatString(),
                       ipackets[0]->get_FormatString()) == 0 );

        if( i == 0 ){ //1st time thru
            //hack to get pass "%" in arg to fmt2type()
            type = Fmt2Type( (cur_packet->get_FormatString()+1) );
            switch(type){
            case CHAR_T:
                format_string="%c";
                break;
            case UCHAR_T:
                format_string="%uc";
                break;
            case INT16_T:
                format_string="%hd";
                break;
            case UINT16_T:
                format_string="%uhd";
                break;
            case INT32_T:
                format_string="%d";
                break;
            case UINT32_T:
                format_string="%ud";
                break;
            case INT64_T:
                format_string="%ld";
                break;
            case UINT64_T:
                format_string="%uld";
                break;
            case FLOAT_T:
                format_string="%f";
                break;
            case DOUBLE_T:
                format_string="%lf";
                break;
            default:
                mrn_dbg(1, mrn_printf(FLF, stderr, 
                                      "ERROR: tfilter_Sum() - invalid packet type: %d (%s)\n", 
                                      type, cur_packet->get_FormatString()));
                return;
            }
        }
        sum( result, &((*cur_packet)[0]->val.p), result, type );
    }

    if( type == CHAR_T ){
        char tmp = *((char*)result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), tmp ) );
        opackets.push_back( new_packet );
    }
    else if( type == UCHAR_T ){
        uchar_t tmp = *((uchar_t *)result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), tmp ) );
        opackets.push_back( new_packet );
    }
    else if( type == FLOAT_T ){
        float * tmp = reinterpret_cast<float *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp ) );
        opackets.push_back( new_packet );
    }
    else if( type == DOUBLE_T ){
        double * tmp = reinterpret_cast<double *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp ));
        opackets.push_back( new_packet );
    }
    else if( type == INT16_T ){
        int16_t * tmp = reinterpret_cast<int16_t *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp ) );
        opackets.push_back( new_packet );
    }
    else if( type == UINT16_T ){

        uint16_t * tmp = reinterpret_cast<uint16_t *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(),*tmp ) );
        opackets.push_back( new_packet );
    }
    else if( type == INT32_T ){
        int32_t * tmp = reinterpret_cast<int32_t *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp ) );
        opackets.push_back( new_packet );
    }
    else if( type == UINT32_T ){
        uint32_t * tmp = reinterpret_cast<uint32_t *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp ) );
        opackets.push_back( new_packet );
    }
    else if( type == INT64_T ){
        int64_t * tmp = reinterpret_cast<int64_t *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp ) );
        opackets.push_back( new_packet );
    }
    else if( type == UINT64_T ){
        uint64_t * tmp = reinterpret_cast<uint64_t *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp ) );
        opackets.push_back( new_packet );
    }
    else{
        assert(0);
    }
}

void tfilter_Max( const vector< PacketPtr >& ipackets,
                  vector< PacketPtr >& opackets,
                  vector< PacketPtr >& /* opackets_reverse */,
                  void ** /* client data */, PacketPtr&,
                  const TopologyLocalInfo& )
{
    char result[8]; //ptr to 8 bytes
    string format_string;
    DataType type=UNKNOWN_T;
    
    for( unsigned int i = 0; i < ipackets.size( ); i++ ) {
        PacketPtr cur_packet( ipackets[i] );
        assert( strcmp(cur_packet->get_FormatString(),
                       ipackets[0]->get_FormatString()) == 0 );
        if( i == 0 ){ //1st time thru
            //+ 1 "hack" to get past "%" in arg to fmt2type()	
            type = Fmt2Type( cur_packet->get_FormatString( ) +1);
            switch(type){
            case CHAR_T:
                format_string="%c";
                break;
            case UCHAR_T:
                format_string="%uc";
                break;
            case INT16_T:
                format_string="%hd";
                break;
            case UINT16_T:
                format_string="%uhd";
                break;
            case INT32_T:
                format_string="%d";
                break;
            case UINT32_T:
                format_string="%ud";
                break;
            case INT64_T:
                format_string="%ld";
                break;
            case UINT64_T:
                format_string="%uld";
                break;
            case FLOAT_T:
                format_string="%f";
                break;
            case DOUBLE_T:
                format_string="%lf";
                break;
            default:
                mrn_dbg(1, mrn_printf(FLF, stderr, 
                                      "ERROR: tfilter_Max() - invalid packet type: %d (%s)\n",
                                      type, cur_packet->get_FormatString()));
                return;
            }
            mrn_max( &((*cur_packet)[0]->val.p), &((*cur_packet)[0]->val.p), result, type );
        }
        else{
            mrn_max( result, &((*cur_packet)[0]->val.p), result, type );
        }
    }

    if( type == CHAR_T ){
        char tmp = *((char*)result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), tmp ) );
        opackets.push_back( new_packet );
    }
    else if( type == UCHAR_T ){
        uchar_t tmp = *((uchar_t *)result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), tmp ) );
        opackets.push_back( new_packet );
    }
    else if( type == FLOAT_T ){
        float * tmp = reinterpret_cast<float *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp ) );
        opackets.push_back( new_packet );
    }
    else if( type == DOUBLE_T ){
        double * tmp = reinterpret_cast<double *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp ));
        opackets.push_back( new_packet );
    }
    else if( type == INT16_T ){
        int16_t * tmp = reinterpret_cast<int16_t *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp ) );
        opackets.push_back( new_packet );
    }
    else if( type == UINT16_T ){

        uint16_t * tmp = reinterpret_cast<uint16_t *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(),*tmp ) );
        opackets.push_back( new_packet );
    }
    else if( type == INT32_T ){
        int32_t * tmp = reinterpret_cast<int32_t *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp ) );
        opackets.push_back( new_packet );
    }
    else if( type == UINT32_T ){
        uint32_t * tmp = reinterpret_cast<uint32_t *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp ) );
        opackets.push_back( new_packet );
    }
    else if( type == INT64_T ){
        int64_t * tmp = reinterpret_cast<int64_t *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp ) );
        opackets.push_back( new_packet );
    }
    else if( type == UINT64_T ){
        uint64_t * tmp = reinterpret_cast<uint64_t *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp ) );
        opackets.push_back( new_packet );
    }
    else{
        assert(0);
    }
}

void tfilter_Min( const vector< PacketPtr >& ipackets,
                  vector< PacketPtr >& opackets,
                  vector< PacketPtr >& /* opackets_reverse */,
                  void ** /* client data */, PacketPtr&,
                  const TopologyLocalInfo& )
{
    char result[8]; //ptr to 8 bytes
    string format_string;
    DataType type=UNKNOWN_T;

    for( unsigned int i = 0; i < ipackets.size( ); i++ ) {
        PacketPtr cur_packet( ipackets[i] );
        assert( strcmp(cur_packet->get_FormatString(),
                       ipackets[0]->get_FormatString()) == 0 );

        if( i == 0 ){ //1st time thru
            //+ 1 "hack" to get past "%" in arg to fmt2type()	
            type = Fmt2Type( cur_packet->get_FormatString( ) +1);
            switch(type){
            case CHAR_T:
                format_string="%c";
                break;
            case UCHAR_T:
                format_string="%uc";
                break;
            case INT16_T:
                format_string="%hd";
                break;
            case UINT16_T:
                format_string="%uhd";
                break;
            case INT32_T:
                format_string="%d";
                break;
            case UINT32_T:
                format_string="%ud";
                break;
            case INT64_T:
                format_string="%ld";
                break;
            case UINT64_T:
                format_string="%uld";
                break;
            case FLOAT_T:
                format_string="%f";
                break;
            case DOUBLE_T:
                format_string="%lf";
                break;
            default:
                mrn_dbg(1, mrn_printf(FLF, stderr, 
                                      "ERROR: tfilter_Min() - invalid packet type: %d (%s)\n",
                                      type, cur_packet->get_FormatString()));
                return;
            }
            mrn_min( &((*cur_packet)[0]->val.p), &((*cur_packet)[0]->val.p), result, type );
        }
        else{
            mrn_min( result, &((*cur_packet)[0]->val.p), result, type );
        }
    }

    if( type == CHAR_T ){
        char tmp = *((char*)result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), tmp ) );
        opackets.push_back( new_packet );
    }
    else if( type == UCHAR_T ){
        uchar_t tmp = *((uchar_t *)result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), tmp ) );
        opackets.push_back( new_packet );
    }
    else if( type == FLOAT_T ){
        float * tmp = reinterpret_cast<float *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp ) );
        opackets.push_back( new_packet );
    }
    else if( type == DOUBLE_T ){
        double * tmp = reinterpret_cast<double *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp ));
        opackets.push_back( new_packet );
    }
    else if( type == INT16_T ){
        int16_t * tmp = reinterpret_cast<int16_t *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp ) );
        opackets.push_back( new_packet );
    }
    else if( type == UINT16_T ){

        uint16_t * tmp = reinterpret_cast<uint16_t *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(),*tmp ) );
        opackets.push_back( new_packet );
    }
    else if( type == INT32_T ){
        int32_t * tmp = reinterpret_cast<int32_t *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp ) );
        opackets.push_back( new_packet );
    }
    else if( type == UINT32_T ){
        uint32_t * tmp = reinterpret_cast<uint32_t *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp ) );
        opackets.push_back( new_packet );
    }
    else if( type == INT64_T ){
        int64_t * tmp = reinterpret_cast<int64_t *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp ) );
        opackets.push_back( new_packet );
    }
    else if( type == UINT64_T ){
        uint64_t * tmp = reinterpret_cast<uint64_t *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp ) );
        opackets.push_back( new_packet );
    }
    else{
        assert(0);
    }
}

void tfilter_Avg( const vector < PacketPtr >& ipackets,
                  vector< PacketPtr >& opackets,
                  vector< PacketPtr >& /* opackets_reverse */,
                  void ** /* client data */, PacketPtr&,
                  const TopologyLocalInfo& )
{
    char result[8]; //ptr to 8 bytes
    char product[8];
    int num_results=0;
    string format_string;
    DataType type=UNKNOWN_T;

    memset(result, 0, 8); //zeroing the bufs
    memset(product, 0, 8);
		
    for( unsigned int i = 0; i < ipackets.size( ); i++ ) {
        PacketPtr cur_packet( ipackets[i] );
        assert( strcmp(cur_packet->get_FormatString(),
                       ipackets[0]->get_FormatString()) == 0 );

        if( i == 0 ){
            format_string = cur_packet->get_FormatString();

            if( format_string == "%c %d" )
                type = CHAR_T;
            else if( format_string == "%uc %d" )
                type = UCHAR_T;
            else if( format_string == "%hd %d" )
                type = INT16_T;
            else if( format_string == "%uhd %d" )
                type = UINT16_T;
            else if( format_string == "%d %d" )
                type = INT32_T;
            else if( format_string == "%ud %d" )
                type = UINT32_T;
            else if( format_string == "%ld %d" )
                type = INT64_T;
            else if( format_string == "%uld %d" )
                type = UINT64_T;
            else if( format_string == "%f %d" )
                type = FLOAT_T;
            else if( format_string == "%lf %d" )
                type = DOUBLE_T;
            else {
                mrn_dbg(1, mrn_printf(FLF, stderr, 
                                      "ERROR: tfilter_Avg() - invalid packet type: %d (%s)\n", 
                                      type, cur_packet->get_FormatString()));
                return;
            }
        }

        mult(&((*cur_packet)[0]->val.p), (*cur_packet)[1]->val.d, product, type);	
        
        sum( result, product, result, type );
	
        num_results += (*cur_packet)[1]->val.d;
        
    }
    
    div( result, num_results, result, type );

    if( type == CHAR_T ){
        char tmp = *((char*)result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), tmp, num_results ) );
        opackets.push_back( new_packet );
    }
    else if( type == UCHAR_T ){
        uchar_t tmp = *((uchar_t *)result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), tmp, num_results ) );
        opackets.push_back( new_packet );
    }
    else if( type == FLOAT_T ){

        float * tmp = reinterpret_cast<float *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp, num_results ) );
        opackets.push_back( new_packet );
    }
    else if( type == DOUBLE_T ) {
        double * tmp = reinterpret_cast<double *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp, num_results ) );
        opackets.push_back( new_packet );
    }
    else if( type == INT16_T ){

        int16_t * tmp = reinterpret_cast<int16_t *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp, num_results ));
        opackets.push_back( new_packet );
    }
    else if( type == UINT16_T ){
        
        uint16_t * tmp = reinterpret_cast<uint16_t *>(result);
	PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp, num_results ));
        opackets.push_back( new_packet );
    }
    else if( type == INT32_T ){

        int32_t * tmp = reinterpret_cast<int32_t *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp, num_results ));
        opackets.push_back( new_packet );
    }
    else if( type == UINT32_T ){

        uint32_t * tmp = reinterpret_cast<uint32_t *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp, num_results ));
        opackets.push_back( new_packet );
    }
    else if( type == INT64_T ){

        int64_t * tmp = reinterpret_cast<int64_t *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp, num_results ));
        opackets.push_back( new_packet );
    }
    else if( type == UINT64_T ){
        uint64_t * tmp = reinterpret_cast<uint64_t *>(result);
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), *tmp, num_results ));
        opackets.push_back( new_packet );
    }
    else{
        PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                          ipackets[0]->get_Tag( ),
                                          format_string.c_str(), result, num_results ));
        opackets.push_back( new_packet );
    }
}



void tfilter_ArrayConcat( const vector< PacketPtr >& ipackets,
                          vector< PacketPtr >& opackets,
                          vector< PacketPtr >& /* opackets_reverse */,
                          void ** /* client data */, PacketPtr&,
                          const TopologyLocalInfo& )
{
    unsigned int result_array_size=0, i, j;
    char* result_array=NULL;
    int data_size=0;
    string format_string;
    DataType type=UNKNOWN_T;
 
    vector< void* > iarrays;
    vector< unsigned > iarray_lens;
    void *tmp_arr;
    unsigned int tmp_arr_len;
 
    for( i = 0; i < ipackets.size( ); i++ ) {
        PacketPtr cur_packet( ipackets[i] );
        assert( strcmp(cur_packet->get_FormatString(),
                       ipackets[0]->get_FormatString()) == 0 );
        if( i == 0 ){ //1st time thru
            //+ 1 "hack" to get past "%" in arg to fmt2type()	
            type = Fmt2Type( cur_packet->get_FormatString( ) +1);
            switch(type){
            case CHAR_ARRAY_T:
                data_size = sizeof(char);
                format_string="%ac";
                break;
            case UCHAR_ARRAY_T:
                data_size = sizeof(unsigned char);
                format_string="%auc";
                break;
            case INT16_ARRAY_T:
                data_size = sizeof(int16_t);
                format_string="%ahd";
                break;
            case UINT16_ARRAY_T:
                data_size = sizeof(uint16_t);
                format_string="%auhd";
                break;
            case INT32_ARRAY_T:
                data_size = sizeof(int32_t);
                format_string="%ad";
                break;
            case UINT32_ARRAY_T:
                data_size = sizeof(uint32_t);
                format_string="%aud";
                break;
            case INT64_ARRAY_T:
                data_size = sizeof(int64_t);
                format_string="%ald";
                break;
            case UINT64_ARRAY_T:
                data_size = sizeof(uint64_t);
                format_string="%auld";
                break;
            case FLOAT_ARRAY_T:
                data_size = sizeof(float);
                format_string="%af";
                break;
            case DOUBLE_ARRAY_T:
                data_size = sizeof(double);
                format_string="%alf";
                break;
            case STRING_ARRAY_T:
                data_size = sizeof(char*);
                format_string="%as";
                break;
            
            case CHAR_LRG_ARRAY_T:
                data_size = sizeof(char);
                format_string="%Ac";
                break;
            case UCHAR_LRG_ARRAY_T:
                data_size = sizeof(unsigned char);
                format_string="%Auc";
                break;
            case INT16_LRG_ARRAY_T:
                data_size = sizeof(int16_t);
                format_string="%Ahd";
                break;
            case UINT16_LRG_ARRAY_T:
                data_size = sizeof(uint16_t);
                format_string="%Auhd";
                break;
            case INT32_LRG_ARRAY_T:
                data_size = sizeof(int32_t);
                format_string="%Ad";
                break;
            case UINT32_LRG_ARRAY_T:
                data_size = sizeof(uint32_t);
                format_string="%Aud";
                break;
            case INT64_LRG_ARRAY_T:
                data_size = sizeof(int64_t);
                format_string="%Ald";
                break;
            case UINT64_LRG_ARRAY_T:
                data_size = sizeof(uint64_t);
                format_string="%Auld";
                break;
            case FLOAT_LRG_ARRAY_T:
                data_size = sizeof(float);
                format_string="%Af";
                break;
            case DOUBLE_LRG_ARRAY_T:
                data_size = sizeof(double);
                format_string="%Alf";
                break;
            case STRING_LRG_ARRAY_T:
                data_size = sizeof(char*);
                format_string="%As";
                break;
            
            default:
                mrn_dbg(1, mrn_printf(FLF, stderr, 
                                      "ERROR: tfilter_ArrayConcat() - invalid packet type: %d (%s)\n", 
                                      type, cur_packet->get_FormatString()));
                return;
            }
        }
        if( cur_packet->unpack( format_string.c_str(), 
                                &tmp_arr, &tmp_arr_len ) == -1 ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, 
                                  "ERROR: tfilter_ArrayConcat() - unpack(%s) failure\n", 
                                  cur_packet->get_FormatString()));
        }
        else {
           iarrays.push_back( tmp_arr );
           iarray_lens.push_back( tmp_arr_len );
           result_array_size += tmp_arr_len;
        }
    }

    result_array = (char*) malloc( result_array_size * data_size );
    
    unsigned pos = 0;
    for( i = 0; i < iarrays.size( ); i++ ) {
        if( type != STRING_ARRAY_T ) {
            memcpy( result_array + pos,
                    iarrays[i],
                    iarray_lens[i] * data_size );
        }
        else {
            char** iarray = (char**) iarrays[i];
            char** oarray = (char**)( result_array + pos );
            for( j = 0; j < iarray_lens[i]; j++ )
                oarray[j] = strdup( iarray[j] );
        }
        pos += ( iarray_lens[i] * data_size );
    }
    fflush(stderr);

    PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                      ipackets[0]->get_Tag( ),
                                      format_string.c_str(), result_array, result_array_size ) );
    // tell MRNet to free result_array
    new_packet->set_DestroyData(true);

    opackets.push_back( new_packet );
}

void tfilter_IntEqClass( const vector< PacketPtr >& ipackets,
                         vector< PacketPtr >& opackets,
                         vector< PacketPtr >& /* opackets_reverse */,
                         void ** /* client data */, PacketPtr&,
                         const TopologyLocalInfo& )
{
    DataType type;
    uint64_t array_len0, array_len1, array_len2;
    map< unsigned int, vector < unsigned int > > classes;
    unsigned int i;

    // find equivalence classes across our input 
    for( i = 0; i < ipackets.size(); i++ ) {
        PacketPtr cur_packet( ipackets[i] );
        const unsigned int *vals = ( const unsigned int * )
            ( (*cur_packet)[0]->get_array(&type, &array_len0) );
        const unsigned int *memcnts = ( const unsigned int * )
            ( (*cur_packet)[1]->get_array(&type, &array_len1) );
        const unsigned int *mems = ( const unsigned int * )
            ( (*cur_packet)[2]->get_array(&type, &array_len2) );

	assert( array_len0 == array_len1 );
	unsigned int curClassMemIdx = 0;
	for( unsigned int j = 0; j < array_len0; j++ ) {
	    mrn_dbg( 3, mrn_printf(FLF, stderr,
                                   "\tclass %d: val = %u, nMems = %u, mems = ", j,
                                   vals[j], memcnts[j] ));

	    // update the members for the current class
	    for( unsigned int k = 0; k < memcnts[j]; k++ ) {
	        mrn_dbg( 3, mrn_printf(FLF, stderr, "%d ",
                                       mems[curClassMemIdx + k] ));
		classes[vals[j]].push_back( mems[curClassMemIdx + k] );
	    }
	    mrn_dbg( 3, mrn_printf(FLF, stderr, "\n" ));
	    curClassMemIdx += memcnts[j];
	}
    }

    // build data structures for the output 
    unsigned int *values = new unsigned int[classes.size(  )];
    unsigned int *memcnts = new unsigned int[classes.size(  )];
    unsigned int nMems = 0;
    unsigned int curIdx = 0;
    for( map< unsigned int,
	      vector< unsigned int > >::iterator iter = classes.begin(  );
	 iter != classes.end(  ); iter++ ) {
        values[curIdx] = iter->first;
	memcnts[curIdx] = (unsigned int)( iter->second ).size(  );
	nMems += memcnts[curIdx];
	curIdx++;
    }
    unsigned int *mems = new unsigned int[nMems];
    unsigned int curMemIdx = 0;
    unsigned int curClassIdx = 0;
    for( map< unsigned int,
              vector< unsigned int > >::iterator citer = classes.begin(  );
            citer != classes.end(  ); citer++ ) {
        for( unsigned int j = 0; j < memcnts[curClassIdx]; j++ ) {
	  mems[curMemIdx] = ( citer->second )[j];
	  curMemIdx++;
	}
	curClassIdx++;
    }
    
    PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                      ipackets[0]->get_Tag( ),
                                      "%aud %aud %aud",
                                      values, classes.size( ),
                                      memcnts, classes.size( ),
                                      mems, nMems ));
    opackets.push_back( new_packet );

    mrn_dbg( 3, mrn_printf(FLF, stderr, "tfilter_IntEqClass: returning\n" ));
    unsigned int curMem = 0;
    for( i = 0; i < classes.size(  ); i++ ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr,
		    "\tclass %d: val = %u, nMems = %u, mems = ", i,
                               ( ( const unsigned int * )( (*opackets[0])[0]->val.p) )[i],
                               ( ( const unsigned int * )( (*opackets[0])[1]->val.p) )[i] ));
	for( unsigned int j = 0;
	     j < ( ( const unsigned int * )( (*opackets[0])[1]->val.p ) )[i];
	     j++ ) {
	    mrn_dbg( 3, mrn_printf(FLF, stderr, "%d ",
                               ( ( const unsigned int * )( (*opackets[0])[2]->val.
					      p ) )[curMem] ));
	    curMem++;
	}
	mrn_dbg( 3, mrn_printf(FLF, stderr, "\n" ));
    }
}

void tfilter_PerfData( const vector< PacketPtr >& ipackets,
                       vector< PacketPtr >& opackets,
                       vector< PacketPtr >& /* opackets_reverse */,
                       void ** /* client data */, PacketPtr& params,
                       const TopologyLocalInfo& info)
{
    // fast path when no aggregation necessary
    const Network* net = info.get_Network();
    bool is_BE = net->is_LocalNodeBackEnd();
    if( (ipackets.size() == 1) && is_BE ) {
        opackets.push_back( ipackets[0] );
        return;
    }

    // check configuration
    int metric, context;
    uint32_t aggr_id, strm_id;
    if( params == Packet::NullPacket ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "ERROR: configuration params not set\n"));
        return;
    }
    params->unpack( "%d %d %ud %ud", &metric, &context, &aggr_id, &strm_id );
    Stream* strm = net->get_Stream( strm_id );
    if( strm == NULL ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "ERROR: stream lookup %d failed\n", strm_id));
        return;
    }

    // determine type of data
    int data_size=0;
    DataType typ;
    perfdata_mettype_t mettype = PerfDataMgr::get_MetricType( (perfdata_metric_t)metric );
    
    switch( mettype ) {
     case PERFDATA_TYPE_UINT:
         data_size = sizeof(uint64_t);
         typ = UINT64_T;
         break;
     case PERFDATA_TYPE_INT:
         data_size = sizeof(int64_t);
         typ = INT64_T;
         break;
     case PERFDATA_TYPE_FLOAT:
         data_size = sizeof(double);
         typ = DOUBLE_T;
         break;
     default:
         mrn_dbg(1, mrn_printf(FLF, stderr, "ERROR: bad metric type\n"));
         return;
    }

    vector< pair<const int*, unsigned> > input_rank_arrays;
    vector< pair<const int*, unsigned> > input_nelems_arrays;
    vector< pair<const void*, unsigned> > input_data_arrays;
    unsigned total_ndatums=0, total_nranks=0;
    vector< perfdata_t > aggr_results;
    vector< int > aggr_ranks;
    perfdata_t init;
    if( aggr_id == (int)TFILTER_MIN )
        memset(&init, 0xFF, sizeof(init));
    else
        memset(&init, 0, sizeof(init));

    const int* rank_arr;
    const int* nelems_arr;
    const void* data_arr;
    unsigned int rank_len, nelems_len, data_len;
    
    // aggregate input packets
    int i = 0;
    PacketPtr local_data, cur_packet;

    if( ! is_BE ) {
        // kludge for collecting data on non-BE nodes
        i = -1;
        local_data = strm->collect_PerfData( (perfdata_metric_t)metric, 
                                             (perfdata_context_t)context, 
                                             ipackets[0]->get_StreamId() );
    }        

    for( ; i < (int)ipackets.size(); i++ ) {

        if( i == -1 ) {
            // kludge for non-BE nodes
            cur_packet = local_data;
        }
        else
            cur_packet = ipackets[i];

        DataType tmptyp;
        rank_arr = (const int*) (*cur_packet)[0]->get_array(&tmptyp, &rank_len);
        nelems_arr = (const int*) (*cur_packet)[1]->get_array(&tmptyp, &nelems_len);
        data_arr = (*cur_packet)[2]->get_array(&tmptyp, &data_len);

        if( data_len ) {

            // concat is easy
            if( aggr_id == (int)TFILTER_ARRAY_CONCAT ) {
                input_rank_arrays.push_back( make_pair(rank_arr, rank_len) );
                input_nelems_arrays.push_back( make_pair(nelems_arr, nelems_len) );
                input_data_arrays.push_back( make_pair(data_arr, data_len) );
                total_ndatums += data_len;
                total_nranks += rank_len;
                continue;
            }

            int nrank;
            int rank = *rank_arr;
            if( rank < 0 ) {
                // aggregations track number of ranks aggregated using negative total
                nrank = -rank;
                total_nranks += nrank;
            }
            else {
                // a positive rank id
                total_nranks++;
                nrank = 1;
            }
            
            unsigned sz = (unsigned)aggr_results.size();
            if( sz < data_len )
                aggr_results.insert( aggr_results.end(), data_len - sz, init );            

            if( aggr_id == (int)TFILTER_SUM ) {

                for( unsigned u=0; u < data_len; u++ ) {
                    perfdata_t& ag = aggr_results[u];
                    switch( typ ) {
                    case UINT64_T:
                        sum( &(ag.u), ((const uint64_t*)data_arr)+u, &(ag.u), typ );
                        break;
                    case INT64_T:
                        sum( &(ag.i), ((const int64_t*)data_arr)+u, &(ag.i), typ );
                        break;
                    case DOUBLE_T:
                        sum( &(ag.d), ((const double*)data_arr)+u, &(ag.d), typ );
                        break;
                    default:
                        break;
                    }
                }
            }
            else if( aggr_id == (int)TFILTER_MIN ) {

                for( unsigned u=0; u < data_len; u++ ) {
                    perfdata_t& ag = aggr_results[u];
                    switch( typ ) {
                    case UINT64_T:
                        mrn_min( &(ag.u), ((const uint64_t*)data_arr)+u, &(ag.u), typ );
                        break;
                    case INT64_T:
                        mrn_min( &(ag.i), ((const int64_t*)data_arr)+u, &(ag.i), typ );
                        break;
                    case DOUBLE_T:
                        mrn_min( &(ag.d), ((const double*)data_arr)+u, &(ag.d), typ );
                        break;
                    default:
                        break;
                    }
                }
            }
            else if( aggr_id == (int)TFILTER_MAX ) {

                for( unsigned u=0; u < data_len; u++ ) {
                    perfdata_t& ag = aggr_results[u];
                    switch( typ ) {
                    case UINT64_T:
                        mrn_max( &(ag.u), ((const uint64_t*)data_arr)+u, &(ag.u), typ );
                        break;
                    case INT64_T:
                        mrn_max( &(ag.i), ((const int64_t*)data_arr)+u, &(ag.i), typ );
                        break;
                    case DOUBLE_T:
                        mrn_max( &(ag.d), ((const double*)data_arr)+u, &(ag.d), typ );
                        break;
                    default:
                        break;
                    }
                }
            }
            else if( aggr_id == (int)TFILTER_AVG ) {

                for( unsigned u=0; u < data_len; u++ ) {
                    perfdata_t& ag = aggr_results[u];
                    switch( typ ) {
                    case UINT64_T: {
                        uint64_t u64 = ((const uint64_t*)data_arr)[u];
                        u64 *= nrank;
                        ag.u += u64;
                        break;
                    }
                    case INT64_T: {
                        int64_t i64 = ((const int64_t*)data_arr)[u];
                        i64 *= nrank;
                        ag.i += i64;
                        break;
                    }
                    case DOUBLE_T: {
                        double dbl = ((const double*)data_arr)[u];
                        dbl *= nrank;
                        ag.d += dbl;
                        break;
                    }
                    default:
                        break;
                    }
                }
            }
        }
    }
    if( data_len && (aggr_id == (int)TFILTER_AVG) ) {
        // divide each sum by number of ranks
        for( unsigned u=0; u < data_len; u++ ) {
            perfdata_t& ag = aggr_results[u];
            switch( typ ) {
             case UINT64_T:
                 ag.u /= total_nranks;
                 break;
             case INT64_T:
                 ag.i /= total_nranks;
                 break;
             case DOUBLE_T:
                 ag.d /= total_nranks;
                 break;
             default:
                 break;
            }
        }
    }

    // allocate for output arrays
    int* orank_arr = NULL;
    int* onelems_arr = NULL;
    void* odata_arr = NULL;
    unsigned int orank_len=0, onelems_len=0, odata_len=0;
    if( aggr_id == (int)TFILTER_ARRAY_CONCAT ) {
        orank_len = onelems_len = total_nranks;
        odata_len = total_ndatums;
    }
    else {
        orank_len = onelems_len = 1;
        odata_len = (unsigned int)aggr_results.size();
    }

    if( orank_len ) {
        orank_arr = (int*) malloc( orank_len * sizeof(int) );
        if( orank_arr == NULL ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "ERROR: malloc output rank array failed\n"));
            return;
        }
    }
    if( onelems_len ) {
        onelems_arr = (int*) malloc( onelems_len * sizeof(int) );
        if( onelems_arr == NULL ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "ERROR: malloc output nelems array failed\n"));
            return;
        }
    }
    if( odata_len ) {
        odata_arr = malloc( odata_len * data_size );
        if( odata_arr == NULL ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "ERROR: malloc output data array failed\n"));
            return;
        }
    }

    // fill output arrays
    if( aggr_id == (int)TFILTER_ARRAY_CONCAT ) {
        int* curr_rank = orank_arr;
        int* curr_nelems = onelems_arr;
        void* curr_data = odata_arr;
        for( unsigned u=0; u < input_rank_arrays.size(); u++ ) {
 
            pair< const int*, unsigned >& r = input_rank_arrays[u];
            memcpy( curr_rank, r.first, r.second * sizeof(int) );
            curr_rank += r.second;

            pair< const int*, unsigned >& n = input_nelems_arrays[u];
            memcpy( curr_nelems, n.first, n.second * sizeof(int) );
            curr_nelems += n.second;
      
            pair< const void*, unsigned >& d = input_data_arrays[u];           
            switch( typ ) { 
             case UINT64_T:
                 memcpy( curr_data, d.first, d.second * sizeof(uint64_t) );
                 curr_data = ((uint64_t*)curr_data) + d.second;
                 break;
             case INT64_T:
                 memcpy( curr_data, d.first, d.second * sizeof(int64_t) );
                 curr_data = ((int64_t*)curr_data) + d.second;
                 break;
             case DOUBLE_T:
                 memcpy( curr_data, d.first, d.second * sizeof(double) );
                 curr_data = ((double*)curr_data) + d.second;
                 break;
             default:
                 break;
            }
        }
    }
    else {
        orank_arr[0] = 0 - (int)total_nranks;
        onelems_arr[0] = (unsigned int)aggr_results.size();
        for( unsigned u=0; u < aggr_results.size(); u++ ) {
            switch( typ ) { 
             case UINT64_T:
                 ((uint64_t*)odata_arr)[u] = aggr_results[u].u;
                 break;
             case INT64_T:
                 ((uint64_t*)odata_arr)[u] = aggr_results[u].i;
                 break;
             case DOUBLE_T:
                 ((double*)odata_arr)[u] = aggr_results[u].d;
                 break;
             default:
                 break;
            }
        }
    }

    // generate output packet
    PacketPtr new_packet( new Packet( ipackets[0]->get_StreamId( ),
                                      ipackets[0]->get_Tag( ),
                                      ipackets[0]->get_FormatString( ),
                                      orank_arr, orank_len,
                                      onelems_arr, onelems_len,
                                      odata_arr, odata_len ) );
    if( new_packet->has_Error() ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "new packet() fail\n"));
        return;
    }
    // tell MRNet to free output arrays
    new_packet->set_DestroyData(true);

    opackets.push_back( new_packet );
}
	

void tfilter_TopoUpdate_common( bool upstream,
                                Network* net,
                                const std::vector < PacketPtr >& ipackets,
                                PacketPtr& opacket )
{
    if( net->is_ShutDown() )
        return;

    int *type_arr, *rtype_arr;
    Rank *prank_arr, *crank_arr, *rprank_arr, *rcrank_arr;
    char **chost_arr, **rchost_arr;
    Port *cport_arr, *rcport_arr;
    uint32_t i, arr_len, rarr_len = 0;
 
    string format_string;

    vector< int* > itype_arr;
    vector< Rank* > iprank_arr;
    vector< Rank* > icrank_arr;
    vector< char** > ichost_arr;
    vector< Port* > icport_arr;
    vector< unsigned > iarray_lens;

    NetworkTopology* nettop = net->get_NetworkTopology();

    unsigned int strm_id = ipackets[0]->get_StreamId();

    bool gen_output = true;
    if( strm_id != PORT_STRM_ID ) {
        if( upstream ) {
            if( net->is_LocalNodeFrontEnd() ) {
                gen_output = false;
            }
        }
        else {
            if( net->is_LocalNodeBackEnd() ) {
                gen_output = false;
            }
        }
    }

    /* Process each input packet
     *   5 parallel arrays: type, parent rank, child rank, child host, child port
     */
    size_t npackets = ipackets.size();
    size_t nreserve = npackets + 1; // +1 to account for possible local port update
    itype_arr.reserve( nreserve );
    iprank_arr.reserve( nreserve );
    icrank_arr.reserve( nreserve );
    ichost_arr.reserve( nreserve );
    icport_arr.reserve( nreserve );
    iarray_lens.reserve( nreserve );
    
    for( i = 0; i < npackets; i++ ) {
     
	PacketPtr cur_packet( ipackets[i] );
        format_string = cur_packet->get_FormatString();

	if( cur_packet->unpack(format_string.c_str(),
                               &type_arr, &arr_len, &prank_arr, &arr_len,
                               &crank_arr, &arr_len, &chost_arr, &arr_len, 
                               &cport_arr, &arr_len) == -1 ) {
	     mrn_dbg(1, mrn_printf(FLF, stderr, "ERROR: unpack(%s) failure\n",
                                   format_string.c_str()));
	}
	else {

            if( (strm_id == PORT_STRM_ID) &&
                (arr_len == 1) && 
                (type_arr[0] == NetworkTopology::TOPO_CHANGE_PORT) &&
                (cport_arr[0] == UnknownPort) ) {
                /* back-ends don't actually have listening ports, so discard the
                   back-end port updates that were used to start the port update
                   stream wave */
                free( type_arr );
                free( prank_arr );
                free( crank_arr );
                free( chost_arr[0] );
                free( chost_arr );
                free( cport_arr );
                continue;
            }

            // Cache the update arrays
	    itype_arr.push_back( type_arr );
	    iprank_arr.push_back( prank_arr );
	    icrank_arr.push_back( crank_arr );
	    ichost_arr.push_back( chost_arr );
	    icport_arr.push_back( cport_arr );
	    iarray_lens.push_back( arr_len );
	    rarr_len += arr_len;
	}
    }

    int type_size = sizeof(NetworkTopology::update_type);
    int port_size = sizeof(Port);
    int rank_size = sizeof(Rank);
    int charptr_size = sizeof(char*);

    if( strm_id == PORT_STRM_ID ) {

        if( net->is_LocalNodeInternal() ) {

            /* Special case: port updates sent on waitforall stream
             * - need to append my local port update to those recv'd from children
             * - if I don't have children, I sent the port update, so don't append
             */

            NetworkTopology::Node* me = nettop->find_Node( net->get_LocalRank() );
            if( me->get_NumChildren() ) {
        
                int* my_type_arr = (int*) calloc(1, type_size);
                Rank* my_prank_arr = (Rank*) calloc(1, rank_size);
                Rank* my_crank_arr = (Rank*) calloc(1, rank_size);
                char** my_chost_arr = (char**) calloc(1, charptr_size);;
                Port* my_cport_arr = (Port*) calloc(1, port_size);

                my_type_arr[0] = NetworkTopology::TOPO_CHANGE_PORT;
                my_prank_arr[0] = UnknownRank;
                my_crank_arr[0] = net->get_LocalRank();
                my_chost_arr[0] = strdup("NULL");
                my_cport_arr[0] = net->get_LocalPort();

                itype_arr.push_back( my_type_arr );
                iprank_arr.push_back( my_prank_arr );
                icrank_arr.push_back( my_crank_arr );
                ichost_arr.push_back( my_chost_arr );
                icport_arr.push_back( my_cport_arr );
                iarray_lens.push_back( 1 );
                rarr_len += 1;
            }
        }
    }

    // Dynamic allocation for result arrays
    rtype_arr  = (int *) malloc( rarr_len * type_size );
    rprank_arr = (Rank *) malloc( rarr_len * rank_size );
    rcrank_arr = (Rank *) malloc( rarr_len * rank_size );
    rchost_arr = (char **) malloc( rarr_len * charptr_size );
    rcport_arr = (Port*) malloc( rarr_len * port_size );

    // Aggregating input packets to one large update array
    unsigned arr_pos=0;

    for( i = 0; i < itype_arr.size(); i++ ) {
     
	unsigned iarr_len = iarray_lens[i];

        if( itype_arr[i] != NULL ) {
            memcpy( rtype_arr + arr_pos,
                    itype_arr[i],
                    (size_t)(iarr_len * type_size));
            free( itype_arr[i] );
        }
 
	if( iprank_arr[i] != NULL ) {
            memcpy( rprank_arr + arr_pos,
                    iprank_arr[i],
                    (size_t) (iarr_len * rank_size));
            free( iprank_arr[i] ); 
        }

	if( icrank_arr[i] != NULL ) {
            memcpy( rcrank_arr + arr_pos,
                    icrank_arr[i],
                    (size_t) (iarr_len * rank_size));
            free( icrank_arr[i] );
        } 

	if( ichost_arr[i] != NULL ) {
            memcpy( rchost_arr + arr_pos,
                    ichost_arr[i],
                    (size_t) (iarr_len * charptr_size));
            free( ichost_arr[i] );
        } 

	if( icport_arr[i] != NULL ) {
            memcpy( rcport_arr + arr_pos,
                    icport_arr[i],
                    (size_t) (iarr_len * port_size));
            free( icport_arr[i] );
        } 
	
	arr_pos += iarr_len;
    }

    // end points whose outlet nodes should be inserted in topology stream peers
    vector< Rank > new_nodes;
    bool update_table = false;

    // Apply updates to NetworkTopology object
    for( i = 0; i < rarr_len; i++ ) {
        switch( rtype_arr[i] ) {

          case NetworkTopology::TOPO_NEW_BE :
              nettop->update_addBackEnd( rprank_arr[i], rcrank_arr[i], 
                                         rchost_arr[i], rcport_arr[i], upstream );
              new_nodes.push_back( rcrank_arr[i] );
              update_table = true;
              break;

          case NetworkTopology::TOPO_REMOVE_RANK :
              nettop->update_removeNode( rprank_arr[i], rcrank_arr[i], upstream ); 
              break;

          case NetworkTopology::TOPO_CHANGE_PARENT :
              nettop->update_changeParent( rprank_arr[i], rcrank_arr[i], upstream ); 
              break;

          case NetworkTopology::TOPO_CHANGE_PORT :
              nettop->update_changePort( rcrank_arr[i], rcport_arr[i], upstream );
              break;

          case NetworkTopology::TOPO_NEW_CP :
	      nettop->update_addInternalNode( rprank_arr[i], rcrank_arr[i],
                                              rchost_arr[i], rcport_arr[i], upstream );
              update_table = true;
              break;

          default:
              //TODO: report invalid update type and exit??
              break;
        }
    }

    if( update_table )
        nettop->update_Router_Table();

    if( upstream && new_nodes.size() && (! net->is_LocalNodeBackEnd()) )
        nettop->update_TopoStreamPeers( new_nodes );

    if( gen_output ) {
        // Create output packet
        opacket = PacketPtr( new Packet(ipackets[0]->get_StreamId(),
                                        ipackets[0]->get_Tag(),
                                        format_string.c_str(),
                                        rtype_arr, rarr_len, rprank_arr, rarr_len, 
                                        rcrank_arr, rarr_len, rchost_arr, rarr_len,
                                        rcport_arr, rarr_len) );

        // tell MRNet to free arrays
        opacket->set_DestroyData(true);
    }
    else {
        // free the arrays
        if( rarr_len ) {
            free( rtype_arr );
            free( rprank_arr );
            free( rcrank_arr );
            free( rcport_arr );
            for( i = 0; i < rarr_len; i++ )
                free( rchost_arr[i] );
            free( rchost_arr );
        }
    }
}

void tfilter_TopoUpdate( const std::vector < PacketPtr >& ipackets,
                         std::vector < PacketPtr >& opackets,
                         std::vector < PacketPtr >&,
                         void**, 
                         PacketPtr&, 
                         const TopologyLocalInfo& info )
{
    mrn_dbg_func_begin();
       
    Network* net = const_cast< Network* >( info.get_Network() );

    bool upstream = true;
    PacketPtr new_packet = Packet::NullPacket;

    tfilter_TopoUpdate_common( upstream,
                               net,
                               ipackets,
                               new_packet );

    if( new_packet != Packet::NullPacket ) {
        // Put the newly created packet in output packet list
        opackets.push_back( new_packet );                                            
    }

    mrn_dbg_func_end();
}

void tfilter_TopoUpdate_Downstream( const std::vector < PacketPtr >& ipackets,
                                    std::vector < PacketPtr >& opackets,
                                    std::vector < PacketPtr >&,
                                    void**, 
                                    PacketPtr&, 
                                    const TopologyLocalInfo& info)
{
    mrn_dbg_func_begin();
   
    Network* net = const_cast< Network* >( info.get_Network() );

    if( net->is_LocalNodeFrontEnd() ) {
        // updates already applied in upstream filter
        opackets = ipackets;
        mrn_dbg_func_end();
        return;
    }

    bool upstream = false;
    PacketPtr new_packet = Packet::NullPacket;

    tfilter_TopoUpdate_common( upstream,
                               net,
                               ipackets,
                               new_packet );
  
    if( new_packet != Packet::NullPacket ) {
        // Put the newly created packet in output packet list
        opackets.push_back( new_packet );       
    }

    mrn_dbg_func_end();
}



/*================================================*
 *    Default Synchronization Filter Definitions  *
 *================================================*/

typedef struct {
    map < Rank, vector< PacketPtr >* > packets_by_rank;
    set < Rank > ready_peers;
} wfa_state;

void sfilter_WaitForAll( const vector< PacketPtr >& ipackets,
                         vector< PacketPtr >& opackets,
                         vector< PacketPtr >& /* opackets_reverse */,
                         void **local_storage, PacketPtr&,
                         const TopologyLocalInfo& info )
{
    mrn_dbg_func_begin();
    map < Rank, vector< PacketPtr >* >::iterator map_iter, del_iter;
    wfa_state* state;
    
    Network* net = const_cast< Network* >( info.get_Network() );

    int stream_id = ipackets[0]->get_StreamId();
    Stream* stream = net->get_Stream( stream_id );
    if( stream == NULL ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "ERROR: stream lookup %d failed\n", stream_id));
        return;
    }

    //1. Setup/Recover Filter State
    if( *local_storage == NULL ) {
        // allocate packet buffer map as appropriate
        mrn_dbg( 5, mrn_printf(FLF, stderr, "No previous storage, allocating ...\n"));
        state = new wfa_state;
        *local_storage = state;
    }
    else{
        // get packet buffer map from filter state
        state = ( wfa_state * ) *local_storage;

        // check for failed nodes && closed Peers
        map_iter = state->packets_by_rank.begin();
        while ( map_iter != state->packets_by_rank.end() ) {

            Rank rank = (*map_iter).first;
            if( net->node_Failed(rank) ) {
                mrn_dbg( 5, mrn_printf(FLF, stderr,
                                       "Discarding packets from failed node[%d] ...\n",
                                       rank ));
                del_iter = map_iter;
                map_iter++;

                // clear packet vector
                (*del_iter).second->clear();

                // erase map slot
                state->packets_by_rank.erase( del_iter );
                state->ready_peers.erase( rank );
            }
            else{
                mrn_dbg( 5, mrn_printf(FLF, stderr, "Node[%d] failed? no\n", rank ));
                map_iter++;
            }
        }
    }

    //2. Place input packets
    for( unsigned int i=0; i < ipackets.size(); i++ ) {

        Rank cur_inlet_rank = ipackets[i]->get_InletNodeRank();

        // special case for back-end synchronization; packets have unknown inlet
        if( cur_inlet_rank == UnknownRank ) {
            if( ipackets.size() == 1 ) {
                opackets.push_back( ipackets[i] );
                return;
            }
        }

        if( net->node_Failed(cur_inlet_rank) ) {
            // drop packets from failed node
            continue;
        }

        // insert packet into map
        map_iter = state->packets_by_rank.find( cur_inlet_rank );

        // allocate new slot if necessary
        if( map_iter == state->packets_by_rank.end() ) {
            mrn_dbg( 5, mrn_printf(FLF, stderr,
                                   "Allocating new map slot for node[%d] ...\n",
                                   cur_inlet_rank ));
            state->packets_by_rank[ cur_inlet_rank ] = new vector < PacketPtr >;
        }

        mrn_dbg( 5, mrn_printf(FLF, stderr, "Placing packet[%d] from node[%d]\n",
                               i, cur_inlet_rank ));
        state->packets_by_rank[ cur_inlet_rank ]->push_back( ipackets[i] );
        state->ready_peers.insert( cur_inlet_rank );
    }

    set< Rank > peers;
    stream->get_ChildRanks( peers );

    mrn_dbg( 5, mrn_printf(FLF, stderr, "slots:%d ready:%d peers:%d\n",
                           state->packets_by_rank.size(), 
                           state->ready_peers.size(),
                           peers.size()) );

    // check for a complete wave
    if( state->ready_peers.size() < peers.size() ) {
        // not all peers ready, so sync condition not met
        return;
    }

    // if we get here, SYNC CONDITION MET!
    mrn_dbg( 5, mrn_printf(FLF, stderr, "All child nodes ready!\n") );

    //3. All nodes ready! Place output packets
    for( map_iter = state->packets_by_rank.begin();
         map_iter != state->packets_by_rank.end();
         map_iter++ ) {

        Rank r = map_iter->first;
        vector< PacketPtr >* pkt_vec = map_iter->second;
        if( pkt_vec->empty() ) {
            // list should only be empty if peer closed stream
            mrn_dbg( 5, mrn_printf(FLF, stderr, 
                                   "Node[%d]'s slot is empty\n", r) );
            continue;
        }

        mrn_dbg( 5, mrn_printf(FLF, stderr, 
                               "Popping packet from Node[%d]\n", r) );
        // push head of list onto output vector
        opackets.push_back( pkt_vec->front() );
        pkt_vec->erase( pkt_vec->begin() );
        
        // if list now empty, remove slot from ready list
        if( pkt_vec->empty() ) {
            mrn_dbg( 5, mrn_printf(FLF, stderr, 
                                   "Removing Node[%d] from ready list\n", r) );
            state->ready_peers.erase( r );
        }
    }
    mrn_dbg( 3, mrn_printf(FLF, stderr, 
                           "Returning %d packets\n", opackets.size()) );
}

typedef struct {
    bool active_timeout;
    map < Rank, vector< PacketPtr >* > packets_by_rank;
    set < Rank > ready_peers;
} to_state;

void sfilter_TimeOut( const vector< PacketPtr >& ipackets,
		      vector< PacketPtr >& opackets,
		      vector< PacketPtr >& /* opackets_reverse */,
		      void **local_storage, PacketPtr& config,
		      const TopologyLocalInfo& info )
{
    mrn_dbg_func_begin();
    map < Rank, vector< PacketPtr >* >::iterator map_iter, del_iter;
    to_state* state;

    Network* net = const_cast< Network* >( info.get_Network() );
    NetworkTopology* nettop = net->get_NetworkTopology();

    bool active = false;
    bool hit_timeout = false;
    unsigned int timeout_ms = 0;
    if( config != Packet::NullPacket ) {
	 config->unpack( "%ud", &timeout_ms );
    }
    if( timeout_ms == 0 ) {
	 opackets = ipackets;
	 mrn_dbg( 3, mrn_printf(FLF, stderr, "No timeout specified, pushing all inputs\n") );
         return;
    }
    
    //1. Setup/Recover Filter State
    if( *local_storage == NULL ) {

        //Allocate packet buffer map as appropriate
        mrn_dbg( 5, mrn_printf(FLF, stderr, "No previous storage, allocating ...\n") );

        state = new to_state;
	state->active_timeout = false;

        *local_storage = state;
    }
    else {
        //Get packet buffer map from filter state
        state = ( to_state * ) *local_storage;

	active = state->active_timeout;

        //Check for failed nodes && closed Peers
        map_iter = state->packets_by_rank.begin();
        while( map_iter != state->packets_by_rank.end() ) {

            Rank rank = map_iter->first;
            NetworkTopology::Node* node = nettop->find_Node( rank );
            if( (node != NULL) && node->failed() ) {
                mrn_dbg( 5, mrn_printf(FLF, stderr,
                                       "Discarding packets from failed node[%d] ...\n",
                                       rank) );
                del_iter = map_iter;
                map_iter++;

                //clear packet vector
                del_iter->second->clear();

                //erase map slot
                state->packets_by_rank.erase( del_iter );
                state->ready_peers.erase( rank );
            }
            else {
                mrn_dbg( 5, mrn_printf(FLF, stderr, "Node[%d] failed? no\n", rank) );
                map_iter++;
            }
        }
    }
   
    mrn_dbg( 5, mrn_printf(FLF, stderr, " input packets size is %d\n" ,ipackets.size()) );
    
    uint32_t stream_id = 0;
    Stream * stream = NULL;
    set< Rank > peers;

    if( ipackets.size() > 0 ) {
        stream_id = ipackets[0]->get_StreamId();
        stream = net->get_Stream( stream_id );
        if( stream == NULL ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "ERROR: stream lookup %d failed\n", stream_id));
            return;
        }
        stream->get_ChildRanks( peers );
    }	

    //2. Place input packets
    for( unsigned int i=0; i < ipackets.size(); i++ ) {
       
        Rank cur_inlet_rank = ipackets[i]->get_InletNodeRank();
	mrn_dbg( 5, mrn_printf(FLF, stderr, "inlet rank is %u \n", cur_inlet_rank) );

        if( cur_inlet_rank == UnknownRank ) {
            if (ipackets.size() == 1 ) {
                opackets.push_back( ipackets[0] ) ;
                mrn_dbg( 3, mrn_printf (FLF, stderr, "pushing locally sourced packet \n") );
                return;
            }
	}   

	if( stream_id == TOPOL_STRM_ID ) {
            if( peers.find(cur_inlet_rank) == peers.end() ) {
                stream->add_Stream_Peer( cur_inlet_rank );
                peers.insert( cur_inlet_rank );
            }
	}   

        NetworkTopology::Node* node = nettop->find_Node( cur_inlet_rank );
        if( (node != NULL) && node->failed() ) {
            //Drop packets from failed node
	    mrn_dbg( 3, mrn_printf(FLF, stderr, "Dropping packets from failed node %d \n", 
                                   cur_inlet_rank) );
            continue;
        }
        
        //Insert packet into map (allocate new slot if necessary)
        map_iter = state->packets_by_rank.find( cur_inlet_rank );
        if( map_iter == state->packets_by_rank.end() ) {
            mrn_dbg( 5, mrn_printf(FLF, stderr,
                                   "Allocating new map slot for node[%d] ...\n",
                                   cur_inlet_rank) );
            state->packets_by_rank[ cur_inlet_rank ] = new vector < PacketPtr >;
        }

        mrn_dbg( 5, mrn_printf(FLF, stderr, "Placing packet[%d] from node[%d]\n",
                               i, cur_inlet_rank) );
        state->packets_by_rank[ cur_inlet_rank ]->push_back( ipackets[i] );
        state->ready_peers.insert( cur_inlet_rank );
    }

    if( ipackets.size() > 0 ) {

        mrn_dbg( 5, mrn_printf(FLF, stderr, "slots:%d ready:%d peers:%d\n",
                               state->packets_by_rank.size(), 
                               state->ready_peers.size(),
                               peers.size()) );
       
        if( ! active ) {
	    // set timeout
	    TimeKeeper* tk = net->get_TimeKeeper();
	    if( tk != NULL ) {
                mrn_dbg( 5, mrn_printf(FLF, stderr, "registering timeout=%dms\n", timeout_ms) );
	        if( tk->register_Timeout( stream_id, timeout_ms ) ) {
	            state->active_timeout = true;
	        }
	    }
        }

        // check for a complete wave
        if( state->ready_peers.size() < peers.size() ) {
            // not all peers ready, so sync condition not met
	    mrn_dbg( 5, mrn_printf(FLF, stderr, "not all peers ready\n") );
            return;
        }
        mrn_dbg( 3, mrn_printf(FLF, stderr, "Complete wave.\n") ); 
    }
    else {
        hit_timeout = true;
        mrn_dbg( 3, mrn_printf(FLF, stderr, "Timeout occurred.\n") );
    }

    //if we get here, SYNC CONDITION MET!
    state->active_timeout = false;

    //3. Place output packets
    for( map_iter = state->packets_by_rank.begin( );
         map_iter != state->packets_by_rank.end( );
         map_iter++ ) {

        Rank r = map_iter->first;
        vector< PacketPtr >* pkt_vec = map_iter->second;
        if( pkt_vec->empty() ) {
            // should only occur if the peer was closed, or we timed-out
            mrn_dbg( 5, mrn_printf(FLF, stderr, "Node[%d]'s slot is empty\n",
                                   r) );
            continue;
        }

        if( hit_timeout ) {
            // push all packets
            mrn_dbg( 5, mrn_printf(FLF, stderr, "Popping all packets from Node[%d]\n",
                                   r) );
            opackets.insert( opackets.end(), pkt_vec->begin(), pkt_vec->end() );
            pkt_vec->clear();
            state->ready_peers.erase( r );
        }
        else {
            //push head of list onto output vector
            mrn_dbg( 5, mrn_printf(FLF, stderr, "Popping packet from Node[%d]\n",
                                   r) );
            opackets.push_back( pkt_vec->front() );
            pkt_vec->erase( pkt_vec->begin() );
        
            //if list now empty, remove slot from ready list
            if( pkt_vec->empty() ) {
                mrn_dbg( 5, mrn_printf(FLF, stderr, 
                                       "Removing Node[%d] from ready list\n", r) );
                state->ready_peers.erase( r );
            }
        }

    }
    mrn_dbg( 3, mrn_printf(FLF, stderr, "Returning %d packets\n", opackets.size()) );
}


/*====================================*
 *    Support Function Definitions    *
 *====================================*/

static inline void sum(const void *in1, const void *in2, void* out, DataType type)
{
    switch (type){
    case CHAR_T:
        *( (char*) out ) = (char)(*((const char*)in1) + *((const char*)in2));
        break;
    case UCHAR_T:
        *( (uchar_t*) out ) = (uchar_t)
            (*((const uchar_t*)in1) + *((const uchar_t*)in2));
        break;
    case INT16_T:
        *( (int16_t*) out ) = (int16_t)
            (*((const int16_t*)in1) + *((const int16_t*)in2));
        break;
    case UINT16_T:
        *( (uint16_t*) out ) = (uint16_t)
            (*((const uint16_t*)in1) + *((const uint16_t*)in2));
        break;
    case INT32_T:
        *( (int32_t*) out ) = *((const int32_t*)in1) + *((const int32_t*)in2);
        break;
    case UINT32_T:
        *( (uint32_t*) out ) = *((const uint32_t*)in1) + *((const uint32_t*)in2);
        break;
    case INT64_T:
        *( (int64_t*) out ) = *((const int64_t*)in1) + *((const int64_t*)in2);
        break;
    case UINT64_T:
        *( (uint64_t*) out ) = *((const uint64_t*)in1) + *((const uint64_t*)in2);
        break;
    case FLOAT_T:
        *( (float*) out ) = *((const float*)in1) + *((const float*)in2);
        break;
    case DOUBLE_T:
        *( (double*) out ) = *((const double*)in1) + *((const double*)in2);
        break;
    default:
        assert(0);
    }
}

static inline void mult(const void *in1, int in2, void* out, DataType type)
{
    switch (type){
    case CHAR_T:
        *( (char*) out ) = (char)(*((const char*)in1) * in2);
        break;
    case UCHAR_T:
        *( (uchar_t*) out ) = (uchar_t)(*((const uchar_t*)in1) * in2);
        break;
    case INT16_T:
        *( (int16_t*) out ) = (int16_t)(*((const int16_t*)in1) * in2);
        break;
    case UINT16_T:
        *( (uint16_t*) out ) = (uint16_t)(*((const uint16_t*)in1) * in2);
        break;
    case INT32_T:
        *( (int32_t*) out ) = *((const int32_t*)in1) * in2;	
        break;
    case UINT32_T:
        *( (uint32_t*) out ) = *((const uint32_t*)in1) * in2;
        break;
    case INT64_T:
        *( (int64_t*) out ) = *((const int64_t*)in1) * (int64_t)in2;
        break;
    case UINT64_T:
        *( (uint64_t*) out ) = *((const uint64_t*)in1) * (int64_t)in2;
        break;
    case FLOAT_T:
        *( (float*) out ) = *((const float*)in1) * (float)in2;
        break;
    case DOUBLE_T:
        *( (double*) out ) = *((const double*)in1) * (double)in2;
        break;
    default:
        assert(0);
    }
}

static inline void div(const void *in1, int in2, void* out, DataType type)
{
    switch (type){
    case CHAR_T:
        *( (char*) out ) = (char)(*((const char*)in1) / in2);
        break;
    case UCHAR_T:
        *( (uchar_t*) out ) = (uchar_t)(*((const uchar_t*)in1) / in2);
        break;
    case INT16_T:
        *( (int16_t*) out ) = (int16_t)(*((const int16_t*)in1) / in2);
        break;
    case UINT16_T:
        *( (uint16_t*) out ) = (uint16_t)(*((const uint16_t*)in1) / in2);
        break;
    case INT32_T:
        *( (int32_t*) out ) = *((const int32_t*)in1) / in2;
        break;
    case UINT32_T:
        *( (uint32_t*) out ) = *((const uint32_t*)in1) / in2;
        break;
    case INT64_T:
        *( (int64_t*) out ) = *((const int64_t*)in1) / (int64_t)in2;
        break;
    case UINT64_T:
        *( (uint64_t*) out ) = *((const uint64_t*)in1) / (int64_t)in2;
        break;
    case FLOAT_T:
        *( (float*) out ) = *((const float*)in1) / (float)in2;
        break;
    case DOUBLE_T:
        *( (double*) out ) = *((const double*)in1) / (double)in2;
        break;
    default:
        assert(0);
    }
}

static inline void mrn_min(const void *in1, const void *in2, void* out, DataType type)
{
    switch (type){
    case CHAR_T:
        *( (char*) out ) = ( ( *((const char*)in1) < *((const char*)in2) ) ?
                             *((const char*)in1) : *((const char*)in2) );
      break;
    case UCHAR_T:
        *( (uchar_t*) out ) = ( ( *((const uchar_t*)in1) < *((const uchar_t*)in2) ) ?
                                *((const uchar_t*)in1) : *((const uchar_t*)in2) );
      break;
    case INT16_T:
        *( (int16_t*) out ) = ( ( *((const int16_t*)in1) < *((const int16_t*)in2) ) ?
                                *((const int16_t*)in1) : *((const int16_t*)in2) );
        break;
    case UINT16_T:
        *( (uint16_t*) out ) = ( ( *((const uint16_t*)in1) < *((const uint16_t*)in2) ) ?
                                 *((const uint16_t*)in1) : *((const uint16_t*)in2) );
      break;
    case INT32_T:
        *( (int32_t*) out ) = ( ( *((const int32_t*)in1) < *((const int32_t*)in2) ) ?
                                *((const int32_t*)in1) : *((const int32_t*)in2) );
      break;
    case UINT32_T:
        *( (uint32_t*) out ) = ( ( *((const uint32_t*)in1) < *((const uint32_t*)in2) ) ?
                                 *((const uint32_t*)in1) : *((const uint32_t*)in2) );
      break;
    case INT64_T:
        *( (int64_t*) out ) = ( ( *((const int64_t*)in1) < *((const int64_t*)in2) ) ?
                                *((const int64_t*)in1) : *((const int64_t*)in2) );
      break;
    case UINT64_T:
        *( (uint64_t*) out ) = ( ( *((const uint64_t*)in1) < *((const uint64_t*)in2) ) ?
                                 *((const uint64_t*)in1) : *((const uint64_t*)in2) );
      break;
    case FLOAT_T:
        *( (float*) out ) = ( ( *((const float*)in1) < *((const float*)in2) ) ?
                              *((const float*)in1) : *((const float*)in2) );
      break;
    case DOUBLE_T:
        *( (double*) out ) = ( ( *((const double*)in1) < *((const double*)in2) ) ?
                               *((const double*)in1) : *((const double*)in2) );
      break;
    default:
        assert(0);
    }
}

static inline void mrn_max(const void *in1, const void *in2, void* out, DataType type )
{
    switch (type){
    case CHAR_T:
        *( (char*) out ) = ( ( *((const char*)in1) > *((const char*)in2) ) ?
                             *((const char*)in1) : *((const char*)in2) );
        break;
    case UCHAR_T:
        *( (uchar_t*) out ) = ( ( *((const uchar_t*)in1) > *((const uchar_t*)in2) ) ?
                                *((const uchar_t*)in1) : *((const uchar_t*)in2) );
        break;
    case INT16_T:
        *( (int16_t*) out ) = ( ( *((const int16_t*)in1) > *((const int16_t*)in2) ) ?
                                *((const int16_t*)in1) : *((const int16_t*)in2) );
        break;
    case UINT16_T:
        *( (uint16_t*) out ) = ( ( *((const uint16_t*)in1) > *((const uint16_t*)in2) ) ?
                                 *((const uint16_t*)in1) : *((const uint16_t*)in2) );
        break;
    case INT32_T:
        *( (int32_t*) out ) = ( ( *((const int32_t*)in1) > *((const int32_t*)in2) ) ?
                                *((const int32_t*)in1) : *((const int32_t*)in2) );
        break;
    case UINT32_T:
        *( (uint32_t*) out ) = ( ( *((const uint32_t*)in1) > *((const uint32_t*)in2) ) ?
                                 *((const uint32_t*)in1) : *((const uint32_t*)in2) );
        break;
    case INT64_T:
        *( (int64_t*) out ) = ( ( *((const int64_t*)in1) > *((const int64_t*)in2) ) ?
                                *((const int64_t*)in1) : *((const int64_t*)in2) );
        break;
    case UINT64_T:
        *( (uint64_t*) out ) = ( ( *((const uint64_t*)in1) > *((const uint64_t*)in2) ) ?
                                 *((const uint64_t*)in1) : *((const uint64_t*)in2) );
        break;
    case FLOAT_T:
        *( (float*) out ) = ( ( *((const float*)in1) > *((const float*)in2) ) ?
                              *((const float*)in1) : *((const float*)in2) );
        break;
    case DOUBLE_T:
        *( (double*) out ) = ( ( *((const double*)in1) > *((const double*)in2) ) ?
                               *((const double*)in1) : *((const double*)in2) );
        break;
    default:
        assert(0);
    }
}


} /* namespace MRN */
