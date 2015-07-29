/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "mrnet/Packet.h"
#include "xplat/Tokenizer.h"
#include "xplat/NetUtils.h"

#include "PeerNode.h"
#include "ParentNode.h"
#include "ChildNode.h"
#include "pdr.h"
#include "utils.h"

namespace MRN
{

PacketPtr Packet::NullPacket;
void Packet::encode_pdr_header(void)
{
    data_sync.Lock();
    uint64_t hdr_sz = 0;
    _byteorder = (char) pdrmem_getbo();
    pdr_sizeof( (pdrproc_t)(Packet::pdr_packet_header), this, &hdr_sz );
    hdr_len = (unsigned) hdr_sz;
    assert( hdr_len );

    /* NOTE: we tell users that packet header and data buffers will have similar
             alignment characteristics to malloc, so if we ever stop using malloc
             we will need to make sure that the buffers are properly aligned */
    hdr = (char*) malloc( size_t(hdr_sz) );
    if( hdr == NULL ) { 
        mrn_dbg( 1, mrn_printf(FLF, stderr, "malloc() failed\n") );
        data_sync.Unlock();
        return;
    }

    PDR pdrs;
    pdrmem_create( &pdrs, hdr, hdr_len, PDR_ENCODE, pdrmem_getbo() );

    if( ! Packet::pdr_packet_header(&pdrs, this) ) {
        error( ERR_PACKING, UnknownRank, "pdr_packet_header() failed" );
    }
    else {
        mrn_dbg( 5, mrn_printf(FLF, stderr, "stream:%u tag:%d fmt:'%s'\n",
                               stream_id, tag, fmt_str) );
    }

    data_sync.Unlock();

}

void Packet::encode_pdr_data(void)
{
    // only called from constructor, so no locking (add locking if this changes)

    if( _decoded == false )
        return;
 
    bool done = pdr_sizeof( (pdrproc_t)(Packet::pdr_packet_data), this, &buf_len );
    if (buf_len == 0 && done == true)
        return;
    assert( buf_len );

    /* NOTE: we tell users that packet header and data buffers will have similar
             alignment characteristics to malloc, so if we ever stop using malloc
             we will need to make sure that the buffers are properly aligned */
    buf = (char*) malloc( size_t(buf_len) );
    if( buf == NULL ) { 
        mrn_dbg( 1, mrn_printf(FLF, stderr, "malloc() failed\n") );
        return;
    }

    PDR pdrs;
    pdrmem_create( &pdrs, buf, buf_len, PDR_ENCODE, pdrmem_getbo() );

    if( ! Packet::pdr_packet_data(&pdrs, this) ) {
        error( ERR_PACKING, UnknownRank, "pdr_packet_data() failed" );
        return;
    }

    mrn_dbg( 5, mrn_printf(FLF, stderr, "stream:%u tag:%d fmt:'%s'\n",
                           stream_id, tag, fmt_str) );
}

void Packet::decode_pdr_header(void) const
{
    // only called from constructor, so no locking (add locking if this changes)

    if( hdr_len == 0 ) // NullPacket has hdr==NULL and hdr_len==0
        return;

    Packet* me = const_cast< Packet* >(this);
    PDR pdrs;
    pdrmem_create( &pdrs, hdr, hdr_len, PDR_DECODE, (pdr_byteorder)hdr[hdr_len - 1] );

    if( ! Packet::pdr_packet_header(&pdrs, me) ) {
        error( ERR_PACKING, UnknownRank, "pdr_packet_header() failed" );
    }

    mrn_dbg( 5, mrn_printf(FLF, stderr, "stream:%u tag:%d fmt:'%s'\n",
                           stream_id, tag, fmt_str) );
}

void Packet::decode_pdr_data(void) const
{
    data_sync.Lock();

    if( ! _decoded ) {
        
        if( buf_len == 0 ) { // NullPacket has buf==NULL and buf_len==0
            _decoded = true;
            data_sync.Unlock();
            return;
        }

        Packet* me = const_cast< Packet* >(this);
        PDR pdrs;
        pdrmem_create( &pdrs, buf, buf_len, PDR_DECODE, (pdr_byteorder) _byteorder );

        if( ! Packet::pdr_packet_data(&pdrs, me) ) {
            error( ERR_PACKING, UnknownRank, "pdr_packet_data() decode failed" );
        }
        else {
            _decoded = true;
            mrn_dbg( 5, mrn_printf(FLF, stderr, "stream:%u tag:%d fmt:'%s'\n",
                                   stream_id, tag, fmt_str) );
        }
    }

    data_sync.Unlock();
}

/* API constructors */

Packet::Packet( unsigned int istream_id, int itag, 
                const char *ifmt_str, ... )
    : stream_id(istream_id), tag(itag), src_rank(UnknownRank),
      fmt_str(NULL), hdr(NULL), hdr_len(0), buf(NULL), buf_len(0),
      inlet_rank(UnknownRank), dest_arr(NULL), dest_arr_len(0), 
      destroy_data(false), _decoded(true)
{    
    // NOTE: we do lazy encoding for the header at the time the packet
    //       is really sent (see Message::send())
    //encode_pdr_header(); 

    _in_packet_count= 1;
    _out_packet_count = 1;
    _perf_data_timer = new Timer[PERFDATA_PKT_TIMERS_MAX];

    if( ifmt_str != NULL ) {

        fmt_str = strdup( ifmt_str );

        va_list arg_list;
        va_start( arg_list, ifmt_str );
        ArgList2DataElementArray( arg_list ); 
        va_end( arg_list );
        encode_pdr_data();
    }
    else
        fmt_str = strdup( NULL_STRING );
    assert( fmt_str != NULL );
}

Packet::Packet( const char *ifmt_str, va_list idata, 
                unsigned int istream_id, int itag )
    : stream_id(istream_id), tag(itag), src_rank(UnknownRank),
      fmt_str(NULL), hdr(NULL), hdr_len(0), buf(NULL), buf_len(0),
      inlet_rank(UnknownRank), dest_arr(NULL), dest_arr_len(0), 
      destroy_data(false), _decoded(true)
{
    // NOTE: we do lazy encoding for the header at the time the packet
    //       is really sent (see Message::send())
    //encode_pdr_header();

    _in_packet_count= 1;
    _out_packet_count = 1;   
    _perf_data_timer = new Timer[PERFDATA_PKT_TIMERS_MAX];
    
    if( ifmt_str != NULL ) {

        fmt_str = strdup( ifmt_str );

        ArgList2DataElementArray( idata );
        encode_pdr_data();
    }
    else
        fmt_str = strdup( NULL_STRING );
    assert( fmt_str != NULL );
}

Packet::Packet( unsigned int istream_id, int itag, 
		const void **idata, const char *ifmt_str ) 
    : stream_id(istream_id), tag(itag), src_rank(UnknownRank),
      fmt_str(NULL), hdr(NULL), hdr_len(0), buf(NULL), buf_len(0), 
      inlet_rank(UnknownRank), dest_arr(NULL), dest_arr_len(0), 
      destroy_data(false), _decoded(true)
{
    // NOTE: we do lazy encoding for the header at the time the packet
    //       is really sent (see Message::send())
    //encode_pdr_header();

    _in_packet_count= 1;
    _out_packet_count = 1;
    _perf_data_timer = new Timer[PERFDATA_PKT_TIMERS_MAX];
    
    if( ifmt_str != NULL ) {

        fmt_str = strdup( ifmt_str );

        ArgVec2DataElementArray( idata ); 
        encode_pdr_data();
    }
    else
        fmt_str = strdup( NULL_STRING );
    assert( fmt_str != NULL );
}

/* Internal constructors */

Packet::Packet( Rank isrc, unsigned int istream_id, int itag, 
                const char *ifmt_str, va_list arg_list )
    : stream_id(istream_id), tag(itag), src_rank(isrc),
      fmt_str(NULL), hdr(NULL), hdr_len(0), buf(NULL), buf_len(0),
      inlet_rank(UnknownRank), dest_arr(NULL), dest_arr_len(0), 
      destroy_data(false), _decoded(true)
{
    // NOTE: we do lazy encoding for the header at the time the packet
    //       is really sent (see Message::send())
    //encode_pdr_header();

    _in_packet_count= 1;
    _out_packet_count = 1;
    _perf_data_timer = new Timer[PERFDATA_PKT_TIMERS_MAX];
    
    if( ifmt_str != NULL ) {

        fmt_str = strdup( ifmt_str );

        ArgList2DataElementArray( arg_list );
        encode_pdr_data();
    }
    else
        fmt_str = strdup( NULL_STRING );
    assert( fmt_str != NULL );
}

Packet::Packet( Rank isrc, unsigned int istream_id, int itag, 
                const void **idata, const char *ifmt_str )
    : stream_id(istream_id), tag(itag), src_rank(isrc),
      fmt_str(NULL), hdr(NULL), hdr_len(0), buf(NULL), buf_len(0), 
      inlet_rank(UnknownRank), dest_arr(NULL), dest_arr_len(0), 
      destroy_data(false), _decoded(true)
{
    // NOTE: we do lazy encoding for the header at the time the packet
    //       is really sent (see Message::send())
    //encode_pdr_header();

    _in_packet_count= 1;
    _out_packet_count = 1;
    _perf_data_timer = new Timer[PERFDATA_PKT_TIMERS_MAX];
    
    if( ifmt_str != NULL ) {

        fmt_str = strdup( ifmt_str );

        ArgVec2DataElementArray( idata ); 
        encode_pdr_data();
    }
    else
        fmt_str = strdup( NULL_STRING );
    assert( fmt_str != NULL );
}

Packet::Packet( unsigned int ihdr_len, char *ihdr, 
                uint64_t ibuf_len, char *ibuf, 
                Rank iinlet_rank )
    : stream_id((unsigned int)-1), tag(-1), src_rank(UnknownRank), 
      fmt_str(NULL), hdr(ihdr), hdr_len(ihdr_len), buf(ibuf), buf_len(ibuf_len), 
      inlet_rank(iinlet_rank), dest_arr(NULL), dest_arr_len(0), 
      destroy_data(true), _decoded(false)
{
    mrn_dbg( 5, mrn_printf(FLF, stderr, "Packet(%p): hdr_len=%u buf_len=%u\n",
                           this, hdr_len, buf_len) );

    _in_packet_count= 1;
    _out_packet_count = 1;
    _perf_data_timer = new Timer[PERFDATA_PKT_TIMERS_MAX];

    decode_pdr_header();
    // comment out following for lazy decode on unpack
    //decode_pdr_data();
}

Packet::~Packet()
{
    data_sync.Lock();

    if( _perf_data_timer != NULL ){
        delete[] _perf_data_timer;
    }
    if( fmt_str != NULL ){
        free( fmt_str );
        fmt_str = NULL;
    }
    if( hdr != NULL ){
        free( hdr );
        hdr = NULL;
    }
    if( buf != NULL ){
        free( buf );
        buf = NULL;
    }

    DataElement* de = NULL;
    for( unsigned int i=0; i < data_elements.size(); i++ ) {
        de = const_cast< DataElement* >( data_elements[i] );
        if( destroy_data ) {
            de->set_DestroyData( true );
        }
        delete de;
    }

    data_sync.Unlock();
}

int Packet::unpack( const char *ifmt_str, ... )
{
    int ret = -1;

    if( NULL != ifmt_str ) {
        va_list arg_list;
        va_start( arg_list, ifmt_str );
        ret = unpack( arg_list, ifmt_str, true );
        va_end( arg_list );
    }

    return ret;
}

int Packet::unpack( va_list iarg_list, const char* ifmt_str, 
                    bool /*dummy parameter to make sure signatures differ*/ )
{
    decode_pdr_data();

    int ret = -1;
    if( ifmt_str != NULL ) {
        ret = ExtractVaList( ifmt_str, iarg_list );
    }
    return ret;
}

const DataElement * Packet::operator[]( unsigned int i ) const
{
    const DataElement * ret = NULL;
    size_t num_elems = 0;

    decode_pdr_data();

    num_elems = data_elements.size();
    if( i < num_elems )
        ret = data_elements[i];

    return ret;
}

bool Packet::operator==( const Packet& p ) const
{
    bool ret = ( this == &p );
    return ret;
}

bool Packet::operator!=(const Packet& p) const
{
    bool ret = ( this != &p );
    return ret;
}

void Packet::set_DestroyData( bool b )
{
    data_sync.Lock();
    destroy_data = b;
    data_sync.Unlock();
}

int Packet::get_Tag(void) const
{
    data_sync.Lock();
    int ret = tag;
    data_sync.Unlock();
    return ret;
}

unsigned int Packet::get_StreamId(void) const
{
    data_sync.Lock();
    unsigned int ret = stream_id;
    data_sync.Unlock();
    return ret;
}

const char *Packet::get_FormatString(void) const
{
    data_sync.Lock();
    const char * ret = fmt_str;
    data_sync.Unlock();
    return ret;
}

const char* Packet::get_Header(void) const
{
    data_sync.Lock();
    const char * ret = hdr;
    data_sync.Unlock();
    return ret;
}

unsigned int Packet::get_HeaderLen(void) const
{
    data_sync.Lock();
    unsigned int ret = hdr_len;
    data_sync.Unlock();
    return ret;
}

const char* Packet::get_Buffer(void) const
{
    data_sync.Lock();
    const char * ret = buf;
    data_sync.Unlock();
    return ret;
}

uint64_t Packet::get_BufferLen(void) const
{
    data_sync.Lock();
    uint64_t ret = buf_len;
    data_sync.Unlock();
    return ret;
}

Rank Packet::get_InletNodeRank(void) const
{
    data_sync.Lock();
    Rank ret = inlet_rank;
    data_sync.Unlock();
    return ret;
}

Rank Packet::get_SourceRank(void) const
{
    data_sync.Lock();
    Rank ret = src_rank;
    data_sync.Unlock();
    return ret;
}

size_t Packet::get_NumDataElements(void) const
{
    data_sync.Lock();
    size_t ret = data_elements.size();
    data_sync.Unlock();
    return ret;
}

const DataElement* Packet::get_DataElement( unsigned int i ) const
{
    data_sync.Lock();
    const DataElement * ret = data_elements[i];
    data_sync.Unlock();
    return ret;
}

bool Packet::set_Destinations( const Rank* bes, unsigned int num_bes )
{
    bool rc = true;

    data_sync.Lock();

    if( dest_arr != NULL )
        free( dest_arr );
    
    dest_arr_len = num_bes;

    if( num_bes ) {
        size_t alloc_sz = size_t(dest_arr_len) * sizeof(Rank);
        dest_arr = (Rank*) malloc( alloc_sz );
        if( dest_arr == NULL )
            rc = false;
        else {
            memcpy( (void*)dest_arr, (const void*)bes, alloc_sz ); 
        }
    }

    data_sync.Unlock();

    return rc;
}

bool Packet::get_Destinations( unsigned int& num_dest, Rank** dests )
{
    if( dest_arr_len ) {
        num_dest = (unsigned int) size_t(dest_arr_len);
        *dests = dest_arr;
        return true;
    }
    return false;
}

void Packet::set_OutgoingPktCount(int size)
{
    _out_packet_count = size;
}

void Packet::set_IncomingPktCount(int size)
{
    _in_packet_count = size;
}

int Packet::ExtractVaList( const char *fmt, va_list arg_list ) const
{
    mrn_dbg( 5, mrn_printf(FLF, stderr, "pkt(%p)\n", this) );

    // make sure format strings agree
    if( (NULL == fmt_str) || strcmp(fmt, fmt_str) ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr, 
                               "passed format '%s' does not match packet's\n", 
                               fmt) );
        return -1;
    }

    int ret;

    data_sync.Lock();
    ret = DataElementArray2ArgList( arg_list );
    data_sync.Unlock();

    mrn_dbg_func_end();
    return ret;
}

static const char* op2str( PDR* pdrs )
{
    switch( pdrs->p_op ) {
    case PDR_ENCODE:
        return "ENCODING";
    case PDR_DECODE:
        return "DECODING";
    case PDR_FREE:
        return "FREEING";
    }
    return NULL;
} 

int Packet::pdr_packet_header( PDR * pdrs, Packet * pkt )
{
    mrn_dbg( 3, mrn_printf(FLF, stderr, "op: %s\n", op2str(pdrs) ));

    /* Process Packet Header into/out of the pdr mem */

    if( pdr_uint32( pdrs, &( pkt->stream_id ) ) == FALSE ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_uint16() failed\n" ));
        return FALSE;
    }
    if( pdr_int32( pdrs, &( pkt->tag ) ) == FALSE ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_int32() failed\n" ));
        return FALSE;
    }
    if( pdr_uint32( pdrs, &( pkt->src_rank ) ) == FALSE ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_uint32() failed\n" ));
        return FALSE;
    }
    if( pdr_wrapstring( pdrs, &( pkt->fmt_str ) ) == FALSE ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_wrapstring() failed\n" ));
        return FALSE;
    }
    Rank** rank_arr = &(pkt->dest_arr);
    if( pdr_array( pdrs, (void**)rank_arr, &( pkt->dest_arr_len ), 
                   UINT64_MAX, sizeof(uint32_t), 
                   (pdrproc_t) pdr_uint32 ) == FALSE ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_array() failed\n" ));
        return FALSE;
    }

    if( pdr_char( pdrs, &( pkt->_byteorder ) ) == FALSE ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_char() failed\n" ));
        return FALSE;
    }
    mrn_dbg_func_end();
    return TRUE;
}
int Packet::pdr_packet_data( PDR * pdrs, Packet * pkt )
{
    mrn_dbg( 3, mrn_printf(FLF, stderr, "op: %s\n", op2str(pdrs) ));

    const char* fmtstr = pkt->fmt_str;
    if( fmtstr == NULL ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr,
                    "No data in message. just header info\n" ));
        return TRUE;
    }

    unsigned int i;
    bool_t retval = 0;
    DataElement* cur_elem = NULL;
    std::string fmt = fmtstr;
    XPlat::Tokenizer tok( fmt );
    std::string::size_type curLen;
    const char* delim = " \t\n%";

    std::string::size_type curPos = tok.GetNextToken( curLen, delim );
    i = 0;

    while( curPos != std::string::npos ) {

        assert( curLen != 0 );
        std::string cur_fmt = fmt.substr( curPos, curLen );

        if( pdrs->p_op == PDR_ENCODE ) {
            cur_elem = const_cast< DataElement *>( pkt->data_elements[i] );
        }
        else if( pdrs->p_op == PDR_DECODE ) {
            cur_elem = new DataElement;
            cur_elem->type = Fmt2Type( cur_fmt.c_str() );
        }

        switch ( cur_elem->type ) {
        case UNKNOWN_T:
            assert( 0 );
        case CHAR_T:
        case UCHAR_T:
            retval =
                pdr_uchar( pdrs, (uchar_t*)( &( cur_elem->val.c ) ) );
            break;

        case CHAR_ARRAY_T:
        case UCHAR_ARRAY_T: {
            void** vpp;
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            vpp = &(cur_elem->val.p);
            retval = pdr_bytes( pdrs, 
                                reinterpret_cast<char**>(vpp),
                                &(cur_elem->array_len), INT32_MAX );
            break;
        }
        case CHAR_LRG_ARRAY_T:
        case UCHAR_LRG_ARRAY_T: {
            void** vpp;
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            vpp = &(cur_elem->val.p);
            retval = pdr_bytes( pdrs, 
                                reinterpret_cast<char**>(vpp),
                               &(cur_elem->array_len), UINT64_MAX );
            break;
        }
        case INT16_T:
        case UINT16_T:
            retval =
                pdr_uint16( pdrs, (uint16_t*)( &( cur_elem->val.hd ) ) );
            break;
        case INT16_LRG_ARRAY_T:
        case UINT16_LRG_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval =
                pdr_array( pdrs, &cur_elem->val.p,
                           &(cur_elem->array_len), UINT64_MAX,
                           sizeof(uint16_t), (pdrproc_t) pdr_uint16 );
            break;


        case INT16_ARRAY_T:
        case UINT16_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval =
                pdr_array( pdrs, &cur_elem->val.p,
                           &(cur_elem->array_len), INT32_MAX,
                           sizeof(uint16_t), (pdrproc_t) pdr_uint16 );
            break;


        case INT32_T:
        case UINT32_T:
            retval =
                pdr_uint32( pdrs, (uint32_t*)( &( cur_elem->val.d ) ) );
            break;


        case INT32_LRG_ARRAY_T:
        case UINT32_LRG_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval =
                pdr_array( pdrs, &cur_elem->val.p,
                           &(cur_elem->array_len), UINT64_MAX,
                           sizeof(uint32_t), (pdrproc_t) pdr_uint32 );
            break;

        case INT32_ARRAY_T:
        case UINT32_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval =
                pdr_array( pdrs, &cur_elem->val.p,
                           &(cur_elem->array_len), INT32_MAX,
                           sizeof(uint32_t), (pdrproc_t) pdr_uint32 );
            break;

        case INT64_T:
        case UINT64_T:
            retval =
                pdr_uint64( pdrs, (uint64_t*)( &( cur_elem->val.ld ) ) );
            break;

        case INT64_LRG_ARRAY_T:
        case UINT64_LRG_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval = pdr_array( pdrs, &cur_elem->val.p,
                                &( cur_elem->array_len ), UINT64_MAX,
                                sizeof(uint64_t), (pdrproc_t) pdr_uint64 );
            break;

        case INT64_ARRAY_T:
        case UINT64_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval = pdr_array( pdrs, &cur_elem->val.p,
                                &( cur_elem->array_len ), INT32_MAX,
                                sizeof(uint64_t), (pdrproc_t) pdr_uint64 );
            break;

        case FLOAT_T:
            retval = pdr_float( pdrs, (float*)( &( cur_elem->val.f ) ) );
            break;
        case DOUBLE_T:
            retval =
                pdr_double( pdrs, (double*)( &( cur_elem->val.lf ) ) );
            break;

        case FLOAT_LRG_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval =
                pdr_array( pdrs, &cur_elem->val.p,
                           &( cur_elem->array_len ), UINT64_MAX,
                           sizeof(float), (pdrproc_t) pdr_float );
            break;
 
        case FLOAT_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval =
                pdr_array( pdrs, &cur_elem->val.p,
                           &( cur_elem->array_len ), INT32_MAX,
                           sizeof(float), (pdrproc_t) pdr_float );
            break;

        case DOUBLE_LRG_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval =
                pdr_array( pdrs, &cur_elem->val.p,
                           &( cur_elem->array_len ), UINT64_MAX,
                           sizeof(double), (pdrproc_t) pdr_double );
            break;

        case DOUBLE_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval =
                pdr_array( pdrs, &cur_elem->val.p,
                           &( cur_elem->array_len ), INT32_MAX,
                           sizeof(double), (pdrproc_t) pdr_double );
            break;

        case STRING_LRG_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval = pdr_array( pdrs, &cur_elem->val.p,
                                &(cur_elem->array_len), UINT64_MAX,
                                sizeof(char*),
                                (pdrproc_t) pdr_wrapstring );
            break;


        case STRING_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval = pdr_array( pdrs, &cur_elem->val.p,
                                &(cur_elem->array_len), INT32_MAX,
                                sizeof(char*),
                                (pdrproc_t) pdr_wrapstring );
            break;
        
        case STRING_T:
            {
                if( pdrs->p_op == PDR_DECODE ) {
                    cur_elem->val.p = NULL;
                }
                void **vp = &(cur_elem->val.p);
                char **cp = (char**)vp;
                retval = pdr_wrapstring( pdrs, cp );             
                mrn_dbg( 5, mrn_printf(FLF, stderr,
                                       "string (%p): '%s'\n", 
                                       cur_elem->val.p, cur_elem->val.p) );
                break;
            }
        }
        if( !retval ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                        "pdr_xxx() failed for elem[%d] of type %d\n", 
                                   i, cur_elem->type) );
            return FALSE;
        }
        if( pdrs->p_op == PDR_DECODE ) {
            pkt->data_elements.push_back( cur_elem );
        }

        curPos = tok.GetNextToken( curLen, delim );
        i++;
    }

    mrn_dbg_func_end();
    return TRUE;
}

int Packet::ArgList2DataElementArray( va_list arg_list )
{
    mrn_dbg_func_begin();

    DataElement * cur_elem=NULL;

    std::string fmt = fmt_str;
    XPlat::Tokenizer tok( fmt );
    std::string::size_type curLen;
    const char* delim = " \t\n%";

    std::string::size_type curPos = tok.GetNextToken( curLen, delim );
    while( curPos != std::string::npos ) {

        assert( curLen != 0 );
        std::string cur_fmt = fmt.substr( curPos, curLen );

        cur_elem = new DataElement;
        cur_elem->type = Fmt2Type( cur_fmt.c_str() );
        switch ( cur_elem->type ) {
        case UNKNOWN_T:
            return -1;
        case CHAR_T:
            cur_elem->val.c = ( char )va_arg( arg_list, int32_t );
            break;
        case UCHAR_T:
            cur_elem->val.uc = ( char )va_arg( arg_list, uint32_t );
            break;

        case INT16_T:
            cur_elem->val.hd = ( short int )va_arg( arg_list, int32_t );
            break;
        case UINT16_T:
            cur_elem->val.uhd = ( short int )va_arg( arg_list, uint32_t );
            break;

        case INT32_T:
            cur_elem->val.d = ( int )va_arg( arg_list, int32_t );
            break;
        case UINT32_T:
            cur_elem->val.ud = ( int )va_arg( arg_list, uint32_t );
            break;

        case INT64_T:
            cur_elem->val.ld = ( int64_t )va_arg( arg_list, int64_t );
            break;
        case UINT64_T:
            cur_elem->val.uld = ( uint64_t )va_arg( arg_list, uint64_t );
            break;

        case FLOAT_T:
            cur_elem->val.f = ( float )va_arg( arg_list, double );
            break;
        case DOUBLE_T:
            cur_elem->val.lf = ( double )va_arg( arg_list, double );
            break;

        case STRING_ARRAY_T:
        case INT16_ARRAY_T:
        case UINT16_ARRAY_T:
        case INT32_ARRAY_T:
        case UINT32_ARRAY_T:
        case INT64_ARRAY_T:
        case UINT64_ARRAY_T:
        case CHAR_ARRAY_T:
        case UCHAR_ARRAY_T:
        case FLOAT_ARRAY_T:
        case DOUBLE_ARRAY_T:
            cur_elem->val.p = va_arg( arg_list, char * );
            cur_elem->array_len =
                ( uint32_t )va_arg( arg_list, uint32_t );
            break;

        case STRING_LRG_ARRAY_T:
        case INT16_LRG_ARRAY_T:
        case UINT16_LRG_ARRAY_T:
        case INT32_LRG_ARRAY_T:
        case UINT32_LRG_ARRAY_T:
        case INT64_LRG_ARRAY_T:
        case UINT64_LRG_ARRAY_T:
        case CHAR_LRG_ARRAY_T:
        case UCHAR_LRG_ARRAY_T:
        case FLOAT_LRG_ARRAY_T:
        case DOUBLE_LRG_ARRAY_T:
            cur_elem->val.p = va_arg( arg_list, char * );
            cur_elem->array_len =
                ( uint64_t )va_arg( arg_list, uint64_t );
            break;

        case STRING_T:
            cur_elem->val.p = va_arg( arg_list, char * );
            if( cur_elem->val.p != NULL )
               cur_elem->array_len = strlen( ( const char * )cur_elem->val.p );
            else
               cur_elem->array_len = 0;
            break;
        default:
            return -1;
            break;
        }
        data_elements.push_back( cur_elem );

        curPos = tok.GetNextToken( curLen, delim );
    }

    mrn_dbg_func_end();
    return 0;
}

int Packet::ArgVec2DataElementArray( const void **idata )
{
    mrn_dbg_func_begin();

    DataElement * cur_elem=NULL;
    unsigned data_ndx = 0;

    std::string fmt = fmt_str;
    XPlat::Tokenizer tok( fmt );
    std::string::size_type curLen;
    const char* delim = " \t\n%";

    std::string::size_type curPos = tok.GetNextToken( curLen, delim );
    while( curPos != std::string::npos ) {

        assert( curLen != 0 );
        std::string cur_fmt = fmt.substr( curPos, curLen );

        cur_elem = new DataElement;
        cur_elem->type = Fmt2Type( cur_fmt.c_str() );
        switch ( cur_elem->type ) {
        case UNKNOWN_T:
            return -1;
        case CHAR_T:
            cur_elem->val.c = *(const char* )idata[data_ndx];
            break;
        case UCHAR_T:
            cur_elem->val.uc = *(const char* )idata[data_ndx];
            break;

        case INT16_T:
            cur_elem->val.hd = *(const short int* )idata[data_ndx];
            break;
        case UINT16_T:
            cur_elem->val.uhd = *(const short int* )idata[data_ndx];
            break;

        case INT32_T:
            cur_elem->val.d = *(const int* )idata[data_ndx];
            break;
        case UINT32_T:
            cur_elem->val.ud = *(const int* )idata[data_ndx];
            break;

        case INT64_T:
            cur_elem->val.ld = *(const int64_t* )idata[data_ndx];
            break;
        case UINT64_T:
            cur_elem->val.uld = *(const uint64_t* )idata[data_ndx];
            break;

        case FLOAT_T:
            cur_elem->val.f = *(const float* )idata[data_ndx];
            break;
        case DOUBLE_T:
	    cur_elem->val.lf = *(const double* )idata[data_ndx];
            break;

        case CHAR_ARRAY_T:
        case UCHAR_ARRAY_T:
        case INT32_ARRAY_T:
        case UINT32_ARRAY_T:
        case INT16_ARRAY_T:
        case UINT16_ARRAY_T:
        case INT64_ARRAY_T:
        case UINT64_ARRAY_T:
        case FLOAT_ARRAY_T:
        case DOUBLE_ARRAY_T:
        case STRING_ARRAY_T:
            cur_elem->val.p = const_cast<void*>( idata[data_ndx++] );
            cur_elem->array_len = *(const uint32_t* )idata[data_ndx];
            break;
        
        case CHAR_LRG_ARRAY_T:
        case UCHAR_LRG_ARRAY_T:
        case INT32_LRG_ARRAY_T:
        case UINT32_LRG_ARRAY_T:
        case INT16_LRG_ARRAY_T:
        case UINT16_LRG_ARRAY_T:
        case INT64_LRG_ARRAY_T:
        case UINT64_LRG_ARRAY_T:
        case FLOAT_LRG_ARRAY_T:
        case DOUBLE_LRG_ARRAY_T:
        case STRING_LRG_ARRAY_T:
            cur_elem->val.p = const_cast<void*>( idata[data_ndx++] );
            cur_elem->array_len = *(const uint64_t* )idata[data_ndx];
            break;
        
        case STRING_T:
            cur_elem->val.p = const_cast<void*>( idata[data_ndx] );
            if( cur_elem->val.p != NULL )
               cur_elem->array_len = strlen( ( const char* )cur_elem->val.p );
            else
               cur_elem->array_len = 0;
            break;
        default:
            return -1;
            break;
        }
        data_elements.push_back( cur_elem );

        curPos = tok.GetNextToken( curLen, delim );
	data_ndx++;
    }

    mrn_dbg_func_end();
    return 0;
}

int Packet::DataElementArray2ArgList( va_list arg_list ) const
{
    mrn_dbg_func_begin();
    int i = 0; 
    uint64_t array_len = 0;
    const DataElement * cur_elem=NULL;
    void *tmp_ptr, *tmp_array;

    std::string fmt = fmt_str;
    XPlat::Tokenizer tok( fmt );
    std::string::size_type curLen;
    const char* delim = " \t\n%";

    std::string::size_type curPos = tok.GetNextToken( curLen, delim );
    std::string cur_fmt;
    while( curPos != std::string::npos ) {
        assert( curLen != 0 );
        cur_fmt = fmt.substr( curPos, curLen );

        cur_elem = data_elements[i];
        assert( cur_elem->type == Fmt2Type( cur_fmt.c_str() ) );
        switch ( cur_elem->type ) {
        case UNKNOWN_T:
            return -1;

        case CHAR_T: {
            char* cp = va_arg( arg_list, char * );
            *cp = cur_elem->val.c;
            break;
        }
        case UCHAR_T: {
            unsigned char* ucp = va_arg( arg_list, unsigned char * );
            *ucp = cur_elem->val.uc;
            break;
        }

        case INT16_T: {
            short int* hdp = va_arg( arg_list, short int * );
            *hdp = cur_elem->val.hd;
            break;
        }
        case UINT16_T: {
            unsigned short int* uhdp = va_arg( arg_list, unsigned short int * );
            *uhdp = cur_elem->val.uhd;
            break;
        }

        case INT32_T: {
            int* dp = va_arg( arg_list, int * );
            *dp = cur_elem->val.d;
            break;
        }
        case UINT32_T: {
            unsigned int* udp = va_arg( arg_list, unsigned int * );
            *udp = cur_elem->val.ud;
            break;
        }

        case INT64_T: {
            int64_t* ldp = va_arg( arg_list, int64_t * );
            *ldp = cur_elem->val.ld;
            break;
        }
        case UINT64_T: {
            uint64_t* uldp = va_arg( arg_list, uint64_t * );
            *uldp = cur_elem->val.uld;
            break;
        }

        case FLOAT_T: {
            float* fp = va_arg( arg_list, float * );
            *fp = cur_elem->val.f;
            break;
        }
        case DOUBLE_T: {
            double* lfp = va_arg( arg_list, double * );
            *lfp = cur_elem->val.lf;
            break;
        }

        case STRING_T: {
            //MEMORY LEAK HERE
            const char** cpp = ( const char ** ) va_arg( arg_list, const char ** );
            *cpp = strdup( (const char *) cur_elem->val.p );
            assert( *cpp != NULL );
            break;
        }

        case CHAR_LRG_ARRAY_T:
        case UCHAR_LRG_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(char);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               memcpy( tmp_array, cur_elem->val.p, size_t(array_len) );
            }
            else
               tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list, uint64_t * );
            assert( tmp_ptr != NULL );
            *( ( uint64_t * )tmp_ptr ) = cur_elem->array_len;
            break;
        }

        case CHAR_ARRAY_T:
        case UCHAR_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(char);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               memcpy( tmp_array, cur_elem->val.p, size_t(array_len) );
            }
            else
               tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list, uint32_t * );
            assert( tmp_ptr != NULL );
            *( ( uint32_t * )tmp_ptr ) = uint32_t(cur_elem->array_len);
            break;
        }

        case INT32_LRG_ARRAY_T:
        case UINT32_LRG_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(int);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               memcpy( tmp_array, cur_elem->val.p, size_t(array_len ));
            }
            else
               tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list, uint64_t * );
            assert( tmp_ptr != NULL );
            *( ( uint64_t * )tmp_ptr ) = cur_elem->array_len;
            break;
        }

        case INT32_ARRAY_T:
        case UINT32_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(int);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               memcpy( tmp_array, cur_elem->val.p, size_t(array_len) );
            }
            else
               tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list, uint32_t * );
            assert( tmp_ptr != NULL );
            *( ( uint32_t * )tmp_ptr ) = uint32_t(cur_elem->array_len);
            break;
        }

        case INT16_LRG_ARRAY_T:
        case UINT16_LRG_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(short int);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               memcpy( tmp_array, cur_elem->val.p, size_t(array_len ));
            }
            else
               tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list, uint64_t * );
            assert( tmp_ptr != NULL );
            *( ( uint64_t * )tmp_ptr ) = cur_elem->array_len;
            break;
        }

        case INT16_ARRAY_T:
        case UINT16_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(short int);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               memcpy( tmp_array, cur_elem->val.p, size_t(array_len ));
            }
            else
               tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list, uint32_t * );
            assert( tmp_ptr != NULL );
            *( ( uint32_t * )tmp_ptr ) = uint32_t(cur_elem->array_len);
            break;
        }

        case INT64_LRG_ARRAY_T:
        case UINT64_LRG_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(int64_t);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               memcpy( tmp_array, cur_elem->val.p, size_t(array_len));
            }
            else
               tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list, uint64_t * );
            assert( tmp_ptr != NULL );
            *( ( uint64_t * )tmp_ptr ) = cur_elem->array_len;
            break;
        }


        case INT64_ARRAY_T:
        case UINT64_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(int64_t);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               memcpy( tmp_array, cur_elem->val.p, size_t(array_len ));
            }
            else
               tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list, uint32_t * );
            assert( tmp_ptr != NULL );
            *( ( uint32_t * )tmp_ptr ) = uint32_t(cur_elem->array_len);
            break;
        }

        case FLOAT_LRG_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(float);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               memcpy( tmp_array, cur_elem->val.p,size_t( array_len ));
            }
            else
               tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list, uint64_t * );
            assert( tmp_ptr != NULL );
            *( ( uint64_t * )tmp_ptr ) = cur_elem->array_len;
            break;
        }

        case FLOAT_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(float);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               memcpy( tmp_array, cur_elem->val.p, size_t(array_len) );
            }
            else
               tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list, uint32_t * );
            assert( tmp_ptr != NULL );
            *( ( uint32_t * )tmp_ptr ) = uint32_t(cur_elem->array_len);
            break;
        }

        case DOUBLE_LRG_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(double);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               memcpy( tmp_array, cur_elem->val.p, size_t(array_len) );
            }
            else
               tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list, uint64_t * );
            assert( tmp_ptr != NULL );
            *( ( uint64_t * )tmp_ptr ) = cur_elem->array_len;
            break;
        }


        case DOUBLE_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(double);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               memcpy( tmp_array, cur_elem->val.p, size_t(array_len) );
            }
            else
               tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list, uint32_t * );
            assert( tmp_ptr != NULL );
            *( ( uint32_t * )tmp_ptr ) = uint32_t(cur_elem->array_len);
            break;
        }

        case STRING_LRG_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(char*);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               for( unsigned j = 0; j < cur_elem->array_len; j++ ) {
                  (( char ** ) tmp_array)[j] = strdup( ((const char **)(cur_elem->val.p))[j] );
                  assert( (( char ** ) tmp_array)[j] != NULL );
               }
            }
            else
               tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list, uint64_t * );
            assert( tmp_ptr != NULL );
            *( ( uint64_t * )tmp_ptr ) = cur_elem->array_len;
            break;
        }

        case STRING_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(char*);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               for( unsigned j = 0; j < cur_elem->array_len; j++ ) {
                  (( char ** ) tmp_array)[j] = strdup( ((const char **)(cur_elem->val.p))[j] );
                  assert( (( char ** ) tmp_array)[j] != NULL );
               }
            }
            else
               tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list, uint32_t * );
            assert( tmp_ptr != NULL );
            *( ( uint32_t * )tmp_ptr ) = uint32_t(cur_elem->array_len);
            break;
        }

        default:
            return -1;
        }
        i++;

        curPos = tok.GetNextToken( curLen, delim );
    }

    mrn_dbg_func_end();
    return 0;
}

int Packet::DataElementArray2ArgVec( void **odata ) const
{
    mrn_dbg_func_begin();
    int i = 0;
    uint64_t array_len = 0;
    unsigned data_ndx = 0;
    const DataElement * cur_elem=NULL;
    void *tmp_ptr, *tmp_array;

    std::string fmt = fmt_str;
    XPlat::Tokenizer tok( fmt );
    std::string::size_type curLen;
    const char* delim = " \t\n%";

    std::string::size_type curPos = tok.GetNextToken( curLen, delim );
    std::string cur_fmt;
    while( curPos != std::string::npos ) {
        assert( curLen != 0 );
        cur_fmt = fmt.substr( curPos, curLen );

        cur_elem = data_elements[i];
        assert( cur_elem->type == Fmt2Type( cur_fmt.c_str() ) );
        switch ( cur_elem->type ) {
        case UNKNOWN_T:
            return -1;

        case CHAR_T: {
            char* cp = ( char* )odata[data_ndx];
            *cp = cur_elem->val.c;
            break;
        }
        case UCHAR_T: {
            unsigned char* ucp = ( unsigned char* )odata[data_ndx];
            *ucp = cur_elem->val.uc;
            break;
        }

        case INT16_T: {
            short int* hdp = ( short int* )odata[data_ndx];
            *hdp = cur_elem->val.hd;
            break;
        }
        case UINT16_T: {
	    unsigned short int* uhdp = ( unsigned short int* )odata[data_ndx];
            *uhdp = cur_elem->val.uhd;
            break;
        }

        case INT32_T: {
            int* dp = ( int* )odata[data_ndx];
            *dp = cur_elem->val.d;
            break;
        }
        case UINT32_T: {
	    unsigned int* udp = ( unsigned int* )odata[data_ndx];
            *udp = cur_elem->val.ud;
            break;
        }

        case INT64_T: {
	    int64_t* ldp = ( int64_t* )odata[data_ndx];
            *ldp = cur_elem->val.ld;
            break;
        }
        case UINT64_T: {
	    uint64_t* uldp = ( uint64_t* )odata[data_ndx];
            *uldp = cur_elem->val.uld;
            break;
        }

        case FLOAT_T: {
            float* fp = ( float* )odata[data_ndx];
            *fp = cur_elem->val.f;
            break;
        }
        case DOUBLE_T: {
            double* lfp = ( double* )odata[data_ndx];
            *lfp = cur_elem->val.lf;
            break;
        }

        case STRING_T: {
            const char** cpp = ( const char** ) odata[data_ndx];
            *cpp = strdup( (const char*) cur_elem->val.p );
            assert( *cpp != NULL );
            break;
        }

        case CHAR_LRG_ARRAY_T:
        case UCHAR_LRG_ARRAY_T: {
            tmp_ptr = odata[data_ndx++];
            assert( tmp_ptr != NULL );
            array_len = size_t(cur_elem->array_len) * sizeof(char);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               memcpy( tmp_array, cur_elem->val.p, size_t(array_len ));
            }
            else
               tmp_array = NULL;
            *( (const void**)tmp_ptr ) = tmp_array;
            tmp_ptr = odata[data_ndx];
            assert( tmp_ptr != NULL );
            *( (uint64_t*)tmp_ptr ) = cur_elem->array_len;
            break;
        }

        case CHAR_ARRAY_T:
        case UCHAR_ARRAY_T: {
            tmp_ptr = odata[data_ndx++];
            assert( tmp_ptr != NULL );
            array_len = size_t(cur_elem->array_len) * sizeof(char);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               memcpy( tmp_array, cur_elem->val.p, size_t(array_len) );
            }
            else
               tmp_array = NULL;
            *( (const void**)tmp_ptr ) = tmp_array;
            tmp_ptr = odata[data_ndx];
            assert( tmp_ptr != NULL );
            *( (uint32_t*)tmp_ptr ) = uint32_t(cur_elem->array_len);
            break;
        }

        case INT32_LRG_ARRAY_T:
        case UINT32_LRG_ARRAY_T: {
            tmp_ptr = odata[data_ndx++];
            assert( tmp_ptr != NULL );
            array_len = size_t(cur_elem->array_len) * sizeof(int);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               memcpy( tmp_array, cur_elem->val.p, size_t(array_len));
            }
            else
               tmp_array = NULL;
            *( (const void**)tmp_ptr ) = tmp_array;
            tmp_ptr = odata[data_ndx];
            assert( tmp_ptr != NULL );
            *( (uint64_t*)tmp_ptr ) = cur_elem->array_len;
            break;
        }

        case INT32_ARRAY_T:
        case UINT32_ARRAY_T: {
            tmp_ptr = odata[data_ndx++];
            assert( tmp_ptr != NULL );
            array_len = size_t(cur_elem->array_len) * sizeof(int);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               memcpy( tmp_array, cur_elem->val.p, size_t(array_len ));
            }
            else
               tmp_array = NULL;
            *( (const void**)tmp_ptr ) = tmp_array;
            tmp_ptr = odata[data_ndx];
            assert( tmp_ptr != NULL );
            *( (uint32_t*)tmp_ptr ) = uint32_t(cur_elem->array_len);
            break;
        }
        case INT16_LRG_ARRAY_T:
        case UINT16_LRG_ARRAY_T: {
            tmp_ptr = odata[data_ndx++];
            assert( tmp_ptr != NULL );
            array_len = size_t(cur_elem->array_len) * sizeof(short int);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               memcpy( tmp_array, cur_elem->val.p, size_t(array_len ));
            }
            else
               tmp_array = NULL;
            *( (const void**)tmp_ptr ) = tmp_array;
            tmp_ptr = odata[data_ndx];
            assert( tmp_ptr != NULL );
            *( (uint64_t*)tmp_ptr ) = cur_elem->array_len;
            break;
        }

        case INT16_ARRAY_T:
        case UINT16_ARRAY_T: {
            tmp_ptr = odata[data_ndx++];
            assert( tmp_ptr != NULL );
            array_len = size_t(cur_elem->array_len) * sizeof(short int);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               memcpy( tmp_array, cur_elem->val.p, size_t(array_len) );
            }
            else
               tmp_array = NULL;
            *( (const void**)tmp_ptr ) = tmp_array;
            tmp_ptr = odata[data_ndx];
            assert( tmp_ptr != NULL );
            *( (uint32_t*)tmp_ptr ) = uint32_t(cur_elem->array_len);
            break;
        }
        case INT64_LRG_ARRAY_T:
        case UINT64_LRG_ARRAY_T: {
	        tmp_ptr = odata[data_ndx++];
            assert( tmp_ptr != NULL );
            array_len = size_t(cur_elem->array_len) * sizeof(int64_t);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               memcpy( tmp_array, cur_elem->val.p, size_t(array_len) );
            }
            else
               tmp_array = NULL;
            *( (const void**)tmp_ptr ) = tmp_array;
            tmp_ptr = odata[data_ndx];
            assert( tmp_ptr != NULL );
            *( (uint64_t*)tmp_ptr ) = cur_elem->array_len;
            break;
        }

        case INT64_ARRAY_T:
        case UINT64_ARRAY_T: {
            tmp_ptr = odata[data_ndx++];
            assert( tmp_ptr != NULL );
            array_len = size_t(cur_elem->array_len) * sizeof(int64_t);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               memcpy( tmp_array, cur_elem->val.p, size_t(array_len) );
            }
            else
               tmp_array = NULL;
            *( (const void**)tmp_ptr ) = tmp_array;
            tmp_ptr = odata[data_ndx];
            assert( tmp_ptr != NULL );
            *( (uint32_t*)tmp_ptr ) = uint32_t(cur_elem->array_len);
            break;
        }
        case FLOAT_LRG_ARRAY_T: {
	    tmp_ptr = odata[data_ndx++];
            assert( tmp_ptr != NULL );
            array_len = size_t(cur_elem->array_len) * sizeof(float);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               memcpy( tmp_array, cur_elem->val.p, size_t(array_len) );
            }
            else
               tmp_array = NULL;
            *( (const void**)tmp_ptr ) = tmp_array;
            tmp_ptr = odata[data_ndx];
            assert( tmp_ptr != NULL );
            *( (uint64_t*)tmp_ptr ) = cur_elem->array_len;
            break;
        }

        case FLOAT_ARRAY_T: {
	    tmp_ptr = odata[data_ndx++];
            assert( tmp_ptr != NULL );
            array_len = size_t(cur_elem->array_len) * sizeof(float);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               memcpy( tmp_array, cur_elem->val.p, size_t(array_len) );
            }
            else
               tmp_array = NULL;
            *( (const void**)tmp_ptr ) = tmp_array;
            tmp_ptr = odata[data_ndx];
            assert( tmp_ptr != NULL );
            *( (uint32_t*)tmp_ptr ) = uint32_t(cur_elem->array_len);
            break;
        }
        case DOUBLE_LRG_ARRAY_T: {
            tmp_ptr = odata[data_ndx++];
            assert( tmp_ptr != NULL );
            array_len = size_t(cur_elem->array_len) * sizeof(double);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               memcpy( tmp_array, cur_elem->val.p, size_t(array_len));
            }
            else
               tmp_array = NULL;
            *( (const void**)tmp_ptr ) = tmp_array;
            tmp_ptr = odata[data_ndx];
            assert( tmp_ptr != NULL );
            *( (uint64_t*)tmp_ptr ) = cur_elem->array_len;
            break;
        }

        case DOUBLE_ARRAY_T: {
            tmp_ptr = odata[data_ndx++];
            assert( tmp_ptr != NULL );
            array_len = size_t(cur_elem->array_len) * sizeof(double);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               memcpy( tmp_array, cur_elem->val.p, size_t(array_len));
            }
            else
               tmp_array = NULL;
            *( (const void**)tmp_ptr ) = tmp_array;
            tmp_ptr = odata[data_ndx];
            assert( tmp_ptr != NULL );
            *( (uint32_t*)tmp_ptr ) = uint32_t(cur_elem->array_len);
            break;
        }

        case STRING_LRG_ARRAY_T: {
            tmp_ptr = odata[data_ndx++];
            assert( tmp_ptr != NULL );
            array_len = size_t(cur_elem->array_len) * sizeof(char*);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               for( unsigned j = 0; j < cur_elem->array_len; j++ ) {
                  ((char**)tmp_array)[j] = strdup( ((const char **)(cur_elem->val.p))[j] );
                  assert( ((char**)tmp_array)[j] != NULL );
               }
            }
            else
               tmp_array = NULL;
            *( (const void**)tmp_ptr ) = tmp_array;
            tmp_ptr = odata[data_ndx++];
            assert( tmp_ptr != NULL );
            *( (uint64_t*)tmp_ptr ) = cur_elem->array_len;
            break;
        }

        case STRING_ARRAY_T: {
            tmp_ptr = odata[data_ndx++];
            assert( tmp_ptr != NULL );
            array_len = size_t(cur_elem->array_len) * sizeof(char*);
            if( array_len > 0 ) {
               tmp_array = malloc(size_t(array_len));
               assert( tmp_array != NULL );
               for( unsigned j = 0; j < cur_elem->array_len; j++ ) {
                  ((char**)tmp_array)[j] = strdup( ((const char **)(cur_elem->val.p))[j] );
                  assert( ((char**)tmp_array)[j] != NULL );
               }
            }
            else
               tmp_array = NULL;
            *( (const void**)tmp_ptr ) = tmp_array;
            tmp_ptr = odata[data_ndx++];
            assert( tmp_ptr != NULL );
            *( (uint32_t*)tmp_ptr ) = uint32_t(cur_elem->array_len);
            break;
        }
        default:
            return -1;
        }
        i++;
	data_ndx++;

        curPos = tok.GetNextToken( curLen, delim );
    }

    mrn_dbg_func_end();
    return 0;
}

void Packet::start_Timer( perfdata_pkt_timers_t context )
{
    _perf_data_timer[context].start();
}

void Packet::stop_Timer( perfdata_pkt_timers_t context )
{
    _perf_data_timer[context].stop();
}

void Packet::reset_Timers()
{
    if (_perf_data_timer != NULL)
        delete [] _perf_data_timer;

    _perf_data_timer = new Timer[PERFDATA_PKT_TIMERS_MAX];
}

void Packet::set_Timer( perfdata_pkt_timers_t context, Timer t )
{
    _perf_data_timer[context] = t;
}

double Packet::get_ElapsedTime( perfdata_pkt_timers_t context )
{
    if (context == PERFDATA_PKT_TIMERS_RECV)
        return _perf_data_timer[context].get_latency_secs() / _in_packet_count;

    if (context == PERFDATA_PKT_TIMERS_SEND)
        return _perf_data_timer[context].get_latency_secs() / _out_packet_count;

    if (context == PERFDATA_PKT_TIMERS_RECV_TO_FILTER)
        return _perf_data_timer[context].get_latency_secs() +  
               (_perf_data_timer[PERFDATA_PKT_TIMERS_RECV].get_latency_secs() / _in_packet_count);

    if (context == PERFDATA_PKT_TIMERS_FILTER_TO_SEND)
        return _perf_data_timer[context].get_latency_secs() +  
               (_perf_data_timer[PERFDATA_PKT_TIMERS_SEND].get_latency_secs() / _out_packet_count);

    return _perf_data_timer[context].get_latency_secs();
}

} /* namespace MRN */




