/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "utils_lightweight.h"
#include "pdr.h"
#include <stdbool.h>
#include "mrnet_lightweight/DataElement.h"
#include "mrnet_lightweight/Error.h"
#include "mrnet_lightweight/Network.h"
#include "mrnet_lightweight/Packet.h"
#include "xplat_lightweight/vector.h"

void delete_Packet_t(Packet_t* packet)
{
    unsigned int i;

    if( packet->data_elements != NULL ) {
        for( i = 0; i < packet->data_elements->size; i++ ) {
            DataElement_t* de = packet->data_elements->vec[i];
            delete_DataElement_t( de );
        }
        delete_vector_t(packet->data_elements);
        packet->data_elements = NULL;
    }

    if( packet->fmt_str != NULL ) {
        free( packet->fmt_str );
        packet->fmt_str = NULL;
    }

    if( packet->hdr != NULL ) {
        free( packet->hdr );
        packet->hdr = NULL;
    }

    if( packet->buf != NULL ) {
        free( packet->buf );
        packet->buf = NULL;
    }

    free(packet);
}

Packet_t* Packet_construct(Packet_t* packet)
{
    PDR pdrs;
    uint64_t hdr_sz, buf_sz;

    // header
    
    bool_t done =  pdr_sizeof((pdrproc_t)(Packet_pdr_packet_header), packet, &hdr_sz);
    assert( hdr_sz );
    packet->hdr_len = (unsigned) hdr_sz;
    packet->hdr = (char*) malloc( (size_t)hdr_sz );
    if( NULL == packet->hdr ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "malloc() failed\n"));
        return NULL;
    }

    pdrmem_create(&pdrs, packet->hdr, hdr_sz, PDR_ENCODE, pdrmem_getbo());

    if( ! Packet_pdr_packet_header(&pdrs, packet) ) {
        error(ERR_PACKING, UnknownRank, "pdr_packet() failed\n");
        mrn_dbg(1, mrn_printf(FLF, stderr, "pdr_packet() failed\n"));
        return NULL;
    }

    // data
    done = pdr_sizeof((pdrproc_t)(Packet_pdr_packet_data), packet, &buf_sz);
    if (done == true && buf_sz == 0)
        return packet;
    assert( buf_sz );
    packet->buf_len = buf_sz;
    packet->buf = (char*) malloc( (size_t)buf_sz );
    if( NULL == packet->buf ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "malloc() failed\n"));
        return NULL;
    }

    pdrmem_create(&pdrs, packet->buf, packet->buf_len, PDR_ENCODE, pdrmem_getbo());

    if( ! Packet_pdr_packet_data(&pdrs, packet) ) {
        error(ERR_PACKING, UnknownRank, "pdr_packet() failed\n");
        mrn_dbg(1, mrn_printf(FLF, stderr, "pdr_packet() failed\n"));
        return NULL;
    }

    mrn_dbg(5, mrn_printf(FLF, stderr,
                          "Packet(%p) success: stream:%d tag:%d fmt:'%s'\n", 
                          packet, packet->stream_id, packet->tag, packet->fmt_str));
    return packet;
}

Packet_t* new_Packet_t(Rank isrc, unsigned int istream_id, int itag, 
                       const char* fmt, va_list arg_list)
{
    Packet_t* packet = (Packet_t*) calloc( (size_t)1, sizeof(Packet_t) );
    if( packet == NULL ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "calloc() failed\n"));
        return NULL;
    }
  
    // initialize values
    packet->stream_id = istream_id;
    packet->tag = itag;

    if( fmt != NULL )
        packet->fmt_str = strdup(fmt);
    else
        packet->fmt_str = NULL;

    packet->byteorder = (char) pdrmem_getbo();
    packet->hdr_len = 0;
    packet->hdr = NULL;
    packet->buf_len = 0;
    packet->buf = NULL;

    packet->inlet_rank = UnknownRank;
    packet->src_rank = isrc;

    packet->data_elements = new_empty_vector_t();

    Packet_ArgList2DataElementArray(packet, arg_list);

    return Packet_construct(packet);

}
Packet_t* new_Packet_t_2(unsigned int istream_id, int itag, 
                         const char* ifmt_str, ... )
{
    Packet_t* packet = NULL;

    va_list arg_list;
    va_start(arg_list, ifmt_str);
    packet = new_Packet_t(UnknownRank, istream_id, itag, 
                          ifmt_str, arg_list);
    va_end(arg_list);

    return packet;
}

Packet_t* new_Packet_t_3(unsigned int ihdr_len, char* ihdr, 
                         uint64_t ibuf_len, char* ibuf, 
                         Rank iinlet_rank)
{
    PDR pdrs;
    Packet_t* packet;

    mrn_dbg_func_begin();

    if( ihdr_len == 0 )
        return NULL;

    // initialize packet
    packet = (Packet_t*) calloc( (size_t)1, sizeof(Packet_t) );
    if( packet == NULL ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "calloc() failed\n"));
        return NULL;
    }
  
    packet->stream_id = 0;
    packet->fmt_str = NULL;
    packet->hdr = ihdr;
    packet->hdr_len = ihdr_len;
    packet->buf = ibuf;
    packet->buf_len = ibuf_len;
    packet->inlet_rank = iinlet_rank;
    packet->data_elements = new_empty_vector_t();
    packet->src_rank = UnknownRank;
    
    // decode header
    pdrmem_create(&pdrs, packet->hdr, (uint64_t)packet->hdr_len, PDR_DECODE, (pdr_byteorder)packet->hdr[packet->hdr_len - 1]);

    if( ! Packet_pdr_packet_header(&pdrs, packet) ) {
        mrn_dbg(1, mrn_printf(FLF,stderr, "pdr_packet() failed\n"));
        error(ERR_PACKING,UnknownRank, "pdr_packet() failed\n");
    }

    mrn_dbg(3, mrn_printf(FLF, stderr,  
                          "Packet(%p): stream:%d tag:%d fmt:'%s'\n", 
                          packet, packet->stream_id, packet->tag, packet->fmt_str));

    if( packet->buf_len != 0 ) { 

        // decode data
        pdrmem_create(&pdrs, packet->buf, packet->buf_len, PDR_DECODE, (pdr_byteorder) packet->byteorder);

        if( ! Packet_pdr_packet_data(&pdrs, packet) ) {
            mrn_dbg(1, mrn_printf(FLF,stderr, "pdr_packet() failed\n"));
            error(ERR_PACKING,UnknownRank, "pdr_packet() failed\n");
        }
    }

    Packet_set_DestroyData( packet, true );
    return packet;
}

void Packet_set_DestroyData(Packet_t * packet, int dd)
{
    unsigned int i;
    if( packet->data_elements != NULL ) {
        for( i = 0; i < packet->data_elements->size; i++ ) {
            DataElement_t* de = packet->data_elements->vec[i];
            if( de != NULL )
                de->destroy_data = dd;
        }
    }
}

int Packet_get_Tag(Packet_t* packet)
{
    return packet->tag;
}

void Packet_set_Tag(Packet_t* packet, int tag)
{
    packet->tag = tag;
}

unsigned int Packet_get_StreamId(Packet_t* packet)
{
    return packet->stream_id;
}

void Packet_set_StreamId(Packet_t* packet, unsigned int strm_id)
{
    packet->stream_id = strm_id;
}

char* Packet_get_FormatString(Packet_t* packet) 
{
    return packet->fmt_str;
}

void Packet_ArgList2DataElementArray(Packet_t* packet, va_list arg_list)
{
    char* tok;
    char* fmt;
    char *saveptr;
    const char* delim = " \t\n%";
    DataElement_t* cur_elem = NULL;
    
    mrn_dbg_func_begin();

    fmt = strdup(packet->fmt_str);
    assert(fmt);

    tok = mrn_strtok(fmt, delim, &saveptr);

    while (tok != NULL) {
        cur_elem = new_DataElement_t();
        cur_elem->type = DataType_Fmt2Type(tok);

        switch (cur_elem->type) {
        case UNKNOWN_T:
            assert(0);
        case CHAR_T:
            cur_elem->val.c = (char)va_arg(arg_list, int32_t);
            break;
        case UCHAR_T:
            cur_elem->val.uc = (char)va_arg(arg_list, uint32_t);
            break;
        case INT16_T:
            cur_elem->val.hd = (short int)va_arg(arg_list, int32_t);
            break;
        case UINT16_T:
            cur_elem->val.uhd = (short int)va_arg(arg_list, uint32_t);
            break;
        case INT32_T:
            cur_elem->val.d = (int)va_arg(arg_list, int32_t);
            break;
        case UINT32_T:
            cur_elem->val.ud = (int)va_arg(arg_list, uint32_t);
            break;
        case INT64_T:
            cur_elem->val.ld = (int64_t)va_arg(arg_list, int64_t);
            break;
        case UINT64_T:
            cur_elem->val.uld = (uint64_t)va_arg(arg_list, uint64_t);
            break;
        case FLOAT_T:
            cur_elem->val.f = (float)va_arg(arg_list, double);
            break;
        case DOUBLE_T:
            cur_elem->val.lf = (double)va_arg(arg_list, double);
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
            cur_elem->val.p = va_arg( arg_list, char * );
            cur_elem->array_len =
                ( uint32_t )va_arg( arg_list, uint32_t );
            break;

        case STRING_T:
            cur_elem->val.p = va_arg( arg_list, char * );
            if( cur_elem->val.p != NULL )
                cur_elem->array_len = strlen( ( const char * )cur_elem->val.p );
            else
                cur_elem->array_len = 0;
            break;
        default:
            assert( 0 );
            break;
        }

	Packet_pushBackElement(packet, cur_elem);
        delete_DataElement_t(cur_elem);
        cur_elem = NULL;

        tok = mrn_strtok(NULL, delim, &saveptr);
    }

    free(fmt);
    (void) saveptr;
    mrn_dbg_func_end();

}

void Packet_pushBackElement(Packet_t* packet, DataElement_t* cur_elem)
{
    DataElement_t* new_elem = (DataElement_t*) calloc( (size_t)1, sizeof(DataElement_t) );
    assert(new_elem != NULL);
    *new_elem = *cur_elem;
    pushBackElement(packet->data_elements, new_elem);
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

bool_t Packet_pdr_packet_header( PDR * pdrs, Packet_t * pkt )
{
    void *vtmp = NULL;
    uint64_t vlen = 0;    

    mrn_dbg( 3, mrn_printf(FLF, stderr, "op: %s\n", op2str(pdrs) ));

    /* Process Packet Header into/out of the pdr mem (see Packet.h for header layout) */
  
    if( pdr_uint32( pdrs, &( pkt->stream_id ) ) == false ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_uint16() failed\n" ));
        return false;
    }
    if( pdr_int32( pdrs, &( pkt->tag ) ) == false ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_int32() failed\n" ));
        return false;
    }
    if( pdr_uint32( pdrs, &( pkt->src_rank ) ) == false ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_uint32() failed\n" ));
        return false;
    }
    if( pdr_wrapstring( pdrs, &( pkt->fmt_str ) ) == false ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_wrapstring() failed\n" ));
        return false;
    }
    if( pdr_array( pdrs, &vtmp, &vlen, (uint64_t)INT32_MAX, 
                   (uint32_t) sizeof(uint32_t), 
                   (pdrproc_t) pdr_uint32 ) == false ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_array() failed\n" ));
        return false;
    }

    if( pdr_char( pdrs, &( pkt->byteorder ) ) == false ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "pdr_uint16() failed\n" ));
        return false;
    }
    mrn_dbg_func_end();
    return true;
}

bool_t Packet_pdr_packet_data( PDR * pdrs, Packet_t * pkt )
{
    DataElement_t * cur_elem = NULL;
    void **vp;
    char **cp;
    char* fmt = NULL;
    char* tok;
    const char* delim = " \t\n%";
    unsigned int i;
    bool_t retval = 0;
    char *saveptr;

    mrn_dbg( 3, mrn_printf(FLF, stderr, "op: %s\n", op2str(pdrs) ));

    if( pkt->fmt_str == NULL ) {
        mrn_dbg( 3, mrn_printf(FLF, stderr,
                               "No data in message. just header info\n" ));
        return true;
    }

    fmt = strdup(pkt->fmt_str);
    assert(fmt);

    i = 0;
    tok = mrn_strtok(fmt, delim, &saveptr);

    while( tok != NULL ) {

        if( pdrs->p_op == PDR_ENCODE ) {
            cur_elem = (DataElement_t *)(pkt->data_elements->vec[i]); 
        }
        else if( pdrs->p_op == PDR_DECODE ) {
            cur_elem = new_DataElement_t();
            cur_elem->type = DataType_Fmt2Type( tok );
        }
     
        switch ( cur_elem->type ) {
        case UNKNOWN_T:
            assert( 0 );
        case CHAR_T:
        case UCHAR_T:
            retval =
                pdr_uchar( pdrs, ( uchar_t * ) ( &( cur_elem->val.c ) ) );
            break;

        case CHAR_ARRAY_T:
        case UCHAR_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval =
                pdr_array( pdrs, &cur_elem->val.p,
                           &( cur_elem->array_len ), (uint64_t) (uint64_t)UINT32_MAX,
                           (uint32_t) sizeof( uchar_t ), ( pdrproc_t ) pdr_uchar );
            break;

        case CHAR_LRG_ARRAY_T:
        case UCHAR_LRG_ARRAY_T: {
            void ** vpp;
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            vpp = &(cur_elem->val.p);
            retval = pdr_bytes( pdrs, 
                                (char **)(vpp),
                               &(cur_elem->array_len), UINT64_MAX );
            break;
            }
        case INT16_T:
        case UINT16_T:
            retval =
                pdr_uint16( pdrs, ( uint16_t * ) ( &( cur_elem->val.hd ) ) );
            break;

        case INT16_ARRAY_T:
        case UINT16_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval =
                pdr_array( pdrs, &cur_elem->val.p,
                           &( cur_elem->array_len ), (uint64_t)UINT32_MAX,
                           (uint32_t) sizeof( uint16_t ), ( pdrproc_t ) pdr_uint16 );
            break;

        case INT16_LRG_ARRAY_T:
        case UINT16_LRG_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval =
                pdr_array( pdrs, &cur_elem->val.p,
                           &(cur_elem->array_len), UINT64_MAX,
                           (uint32_t) sizeof(uint16_t), (pdrproc_t) pdr_uint16 );
            break;

        case INT32_T:
        case UINT32_T:
            retval =
                pdr_uint32( pdrs, ( uint32_t * ) ( &( cur_elem->val.d ) ) );
            break;

 
        case INT32_LRG_ARRAY_T:
        case UINT32_LRG_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval =
                pdr_array( pdrs, &cur_elem->val.p,
                           &(cur_elem->array_len), UINT64_MAX,
                           (uint32_t)sizeof(uint32_t), (pdrproc_t) pdr_uint32 );
            break;
        
        case INT32_ARRAY_T:
        case UINT32_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval =
                pdr_array( pdrs, &cur_elem->val.p,
                           &( cur_elem->array_len ), (uint64_t)UINT32_MAX,
                           (uint32_t) sizeof( uint32_t ), ( pdrproc_t ) pdr_uint32 );
            break;

        case INT64_T:
        case UINT64_T:
            retval =
                pdr_uint64( pdrs, ( uint64_t * ) ( &( cur_elem->val.ld ) ) );
            break;

        case INT64_ARRAY_T:
        case UINT64_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval = pdr_array( pdrs, &cur_elem->val.p,
                                &( cur_elem->array_len ), (uint64_t)UINT32_MAX,
                                (uint32_t) sizeof( uint64_t ), ( pdrproc_t ) pdr_uint64 );
            break;

        case INT64_LRG_ARRAY_T:
        case UINT64_LRG_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval = pdr_array( pdrs, &cur_elem->val.p,
                                &( cur_elem->array_len ), UINT64_MAX,
                                (uint32_t)sizeof(uint64_t), (pdrproc_t) pdr_uint64 );
            break;

        case FLOAT_T:
            retval = pdr_float( pdrs, ( float * )( &( cur_elem->val.f ) ) );
            break;
        case DOUBLE_T:
            retval =
                pdr_double( pdrs, ( double * )( &( cur_elem->val.lf ) ) );
            break;
        case FLOAT_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval =
                pdr_array( pdrs, &cur_elem->val.p,
                           &( cur_elem->array_len ), (uint64_t)UINT32_MAX,
                           (uint32_t) sizeof( float ), ( pdrproc_t ) pdr_float );
            break;

        case FLOAT_LRG_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval =
                pdr_array( pdrs, &cur_elem->val.p,
                           &( cur_elem->array_len ), UINT64_MAX,
                           (uint32_t)sizeof(float), (pdrproc_t) pdr_float );
            break;



        case DOUBLE_LRG_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval =
                pdr_array( pdrs, &cur_elem->val.p,
                           &( cur_elem->array_len ), UINT64_MAX,
                           (uint32_t)sizeof(double), (pdrproc_t) pdr_double );
            break;

        case DOUBLE_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval =
                pdr_array( pdrs, &cur_elem->val.p,
                           &( cur_elem->array_len ), (uint64_t)UINT32_MAX,
                           (uint32_t) sizeof( double ), ( pdrproc_t ) pdr_double );
            break;

        case STRING_LRG_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval = pdr_array( pdrs, &cur_elem->val.p,
                                &(cur_elem->array_len), UINT64_MAX,
                                (uint32_t)sizeof(char*),
                                (pdrproc_t) pdr_wrapstring );
            break;
        case STRING_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval = pdr_array( pdrs, &cur_elem->val.p,
                                &(cur_elem->array_len), (uint64_t)UINT32_MAX,
                                (uint32_t) sizeof(char*),
                                (pdrproc_t)pdr_wrapstring );
            break;
        case STRING_T:
            {
                if( pdrs->p_op == PDR_DECODE ) {
                    cur_elem->val.p = NULL;
                }
                vp = &(cur_elem->val.p);
                cp = (char**)vp;
                retval = pdr_wrapstring( pdrs, cp );             
                mrn_dbg( 3, mrn_printf(FLF, stderr,
                                       "string (%p): \"%s\"\n", cur_elem->val.p, cur_elem->val.p));
                break;
            }
        }
        if( !retval ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "pdr_xxx() failed for elem[%d] of type %d\n", i,
                                   cur_elem->type ));
            return retval;
        }
        if( pdrs->p_op == PDR_DECODE ) {
            Packet_pushBackElement( pkt, cur_elem );
            delete_DataElement_t( cur_elem );
            cur_elem = NULL;
        }

        tok = mrn_strtok(NULL, delim, &saveptr);
        i++;
    }
    free(fmt);
    // Surpress unused variable warning from strtok on GCC
    (void) saveptr;
    mrn_dbg_func_end();
    return true;
}

int Packet_unpack(Packet_t* packet, const char *ifmt_str, ... )
{
    va_list arg_list;
    int ret;

    va_start(arg_list, ifmt_str);
    ret = Packet_ExtractVaList(packet, ifmt_str, arg_list);
    va_end(arg_list);
  
    return ret;
}

int Packet_unpack_valist(Packet_t* packet, va_list arg_list, const char *ifmt_str )
{
    int ret = Packet_ExtractVaList(packet, ifmt_str, arg_list);
    return ret;
}

int Packet_ExtractVaList(Packet_t* packet, const char* fmt, va_list arg_list)
{
    mrn_dbg(3, mrn_printf(FLF, stderr, "In ExtractvalList(%p)\n", packet));

    // make sure passed fmt same as pkt
    if( strcmp(fmt, packet->fmt_str) )
        return -1;

    Packet_DataElementArray2ArgList(packet, arg_list);

    return 0;
}

void Packet_DataElementArray2ArgList(Packet_t* packet, va_list arg_list)
{
    int i = 0;
    uint64_t array_len = 0;
    DataElement_t* cur_elem;
    void *tmp_ptr;
    void *tmp_array;
    char* fmt = NULL;
    char *saveptr;
    const char* delim = " \t\n%";
    char* tok;
   
    // variables for data types
    char* cp;
    unsigned char* ucp;
    short int* hdp;
    unsigned short int* uhdp;
    int* dp;
    unsigned int* udp;
    int64_t* ldp;
    uint64_t* uldp;
    float* fp;
    double* lfp;
    const char** cpp;

    unsigned j;

    mrn_dbg_func_begin();

    fmt = strdup(packet->fmt_str);
    assert(fmt != NULL);

    tok = mrn_strtok(fmt, delim, &saveptr);
    while( tok != NULL ) {
        cur_elem = (DataElement_t*)(packet->data_elements->vec[i]);
        assert(cur_elem->type == DataType_Fmt2Type(tok));

        switch(cur_elem->type) {
        case UNKNOWN_T:
            assert(0);

        case CHAR_T: {
            cp = va_arg( arg_list, char * );
            *cp = cur_elem->val.c;
            break;
        }
        case UCHAR_T: {
            ucp = va_arg( arg_list, unsigned char * );
            *ucp = cur_elem->val.uc;
            break;
        }

        case INT16_T: {
            hdp = va_arg( arg_list, short int * );
            *hdp = cur_elem->val.hd;
            break;
        }
        case UINT16_T: {
            uhdp = va_arg( arg_list, unsigned short int * );
            *uhdp = cur_elem->val.uhd;
            break;
        }

        case INT32_T: {
            dp = va_arg( arg_list, int * );
            *dp = cur_elem->val.d;
            break;
        }
        case UINT32_T: {
            udp = va_arg( arg_list, unsigned int * );
            *udp = cur_elem->val.ud;
            break;
        }

        case INT64_T: {
            ldp = va_arg( arg_list, int64_t * );
            *ldp = cur_elem->val.ld;
            break;
        }
        case UINT64_T: {
            uldp = va_arg( arg_list, uint64_t * );
            *uldp = cur_elem->val.uld;
            break;
        }

        case FLOAT_T: {
            fp = va_arg( arg_list, float * );
            *fp = cur_elem->val.f;
            break;
        }
        case DOUBLE_T: {
            lfp = va_arg(arg_list, double *);
            *lfp = cur_elem->val.lf;
            break;
        }

        case STRING_T: {
            cpp = ( const char ** ) va_arg( arg_list, const char ** );
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
                tmp_array = malloc((size_t)array_len);
                assert( tmp_array != NULL );
                memcpy( tmp_array, cur_elem->val.p,(size_t) array_len );
            }
            else
                tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list, uint64_t * );
            assert( tmp_ptr != NULL );
            *( ( uint64_t * )tmp_ptr ) = cur_elem->array_len;
            break;
        }

        case INT32_LRG_ARRAY_T:
        case UINT32_LRG_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(int);
            if( array_len > 0 ) {
                tmp_array = malloc((size_t)array_len);
                assert( tmp_array != NULL );
                memcpy( tmp_array, cur_elem->val.p,(size_t) array_len );
            }
            else
                tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list, uint64_t * );
            assert( tmp_ptr != NULL );
            *( ( uint64_t * )tmp_ptr ) = cur_elem->array_len;
            break;
        }

        case INT16_LRG_ARRAY_T:
        case UINT16_LRG_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(short int);
            if( array_len > 0 ) {
                tmp_array = malloc((size_t)array_len);
                assert( tmp_array != NULL );
                memcpy( tmp_array, cur_elem->val.p,(size_t) array_len );
            }
            else
                tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list, uint64_t * );
            assert( tmp_ptr != NULL );
            *( ( uint64_t * )tmp_ptr ) = cur_elem->array_len;
            break;
        }

        case INT64_LRG_ARRAY_T:
        case UINT64_LRG_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(int64_t);
            if( array_len > 0 ) {
                tmp_array = malloc((size_t)array_len);
                assert( tmp_array != NULL );
                memcpy( tmp_array, cur_elem->val.p,(size_t) array_len );
            }
            else
                tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list,uint64_t * );
            assert( tmp_ptr != NULL );
            *( ( uint64_t * )tmp_ptr ) = cur_elem->array_len;
            break;
        }

        case FLOAT_LRG_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(float);
            if( array_len > 0 ) {
                tmp_array = malloc((size_t)array_len);
                assert( tmp_array != NULL );
                memcpy( tmp_array, cur_elem->val.p,(size_t) array_len );
            }
            else
                tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list, uint64_t * );
            assert( tmp_ptr != NULL );
            *( ( uint64_t * )tmp_ptr ) = cur_elem->array_len;
            break;
        }

        case DOUBLE_LRG_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(double);
            if( array_len > 0 ) {
                tmp_array = malloc((size_t)array_len);
                assert( tmp_array != NULL );
                memcpy( tmp_array, cur_elem->val.p,(size_t) array_len );
            }
            else
                tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list, uint64_t * );
            assert( tmp_ptr != NULL );
            *( ( uint64_t * )tmp_ptr ) = cur_elem->array_len;
            break;
        }

        case STRING_LRG_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(char*);
            if( array_len > 0 ) {
                tmp_array = malloc((size_t)array_len);
                assert( tmp_array != NULL );
                for(j = 0; j < cur_elem->array_len; j++ ) {
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

        case CHAR_ARRAY_T:
        case UCHAR_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(char);
            if( array_len > 0 ) {
                tmp_array = malloc((size_t)array_len);
                assert( tmp_array != NULL );
                memcpy( tmp_array, cur_elem->val.p,(size_t) array_len );
            }
            else
                tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list, uint32_t * );
            assert( tmp_ptr != NULL );
            *( ( uint32_t * )tmp_ptr ) = (uint32_t)cur_elem->array_len;
            break;
        }

        case INT32_ARRAY_T:
        case UINT32_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(int);
            if( array_len > 0 ) {
                tmp_array = malloc((size_t)array_len);
                assert( tmp_array != NULL );
                memcpy( tmp_array, cur_elem->val.p,(size_t) array_len );
            }
            else
                tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list, uint32_t * );
            assert( tmp_ptr != NULL );
            *( ( uint32_t * )tmp_ptr ) = (uint32_t)cur_elem->array_len;
            break;
        }

        case INT16_ARRAY_T:
        case UINT16_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(short int);
            if( array_len > 0 ) {
                tmp_array = malloc((size_t)array_len);
                assert( tmp_array != NULL );
                memcpy( tmp_array, cur_elem->val.p,(size_t) array_len );
            }
            else
                tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list, uint32_t * );
            assert( tmp_ptr != NULL );
            *( ( uint32_t * )tmp_ptr ) = (uint32_t)cur_elem->array_len;
            break;
        }

        case INT64_ARRAY_T:
        case UINT64_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(int64_t);
            if( array_len > 0 ) {
                tmp_array = malloc((size_t)array_len);
                assert( tmp_array != NULL );
                memcpy( tmp_array, cur_elem->val.p,(size_t) array_len );
            }
            else
                tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list,uint32_t * );
            assert( tmp_ptr != NULL );
            *( ( uint32_t * )tmp_ptr ) = (uint32_t)cur_elem->array_len;
            break;
        }

        case FLOAT_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(float);
            if( array_len > 0 ) {
                tmp_array = malloc((size_t)array_len);
                assert( tmp_array != NULL );
                memcpy( tmp_array, cur_elem->val.p,(size_t) array_len );
            }
            else
                tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list, uint32_t * );
            assert( tmp_ptr != NULL );
            *( ( uint32_t * )tmp_ptr ) = (uint32_t)cur_elem->array_len;
            break;
        }

        case DOUBLE_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(double);
            if( array_len > 0 ) {
                tmp_array = malloc((size_t)array_len);
                assert( tmp_array != NULL );
                memcpy( tmp_array, cur_elem->val.p,(size_t) array_len );
            }
            else
                tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list, uint32_t * );
            assert( tmp_ptr != NULL );
            *( ( uint32_t * )tmp_ptr ) = (uint32_t)cur_elem->array_len;
            break;
        }

        case STRING_ARRAY_T: {
            tmp_ptr = ( void * )va_arg( arg_list, void ** );
            assert( tmp_ptr != NULL );
            array_len = cur_elem->array_len * sizeof(char*);
            if( array_len > 0 ) {
                tmp_array = malloc((size_t)array_len);
                assert( tmp_array != NULL );
                for(j = 0; j < cur_elem->array_len; j++ ) {
                    (( char ** ) tmp_array)[j] = strdup( ((const char **)(cur_elem->val.p))[j] );
                    assert( (( char ** ) tmp_array)[j] != NULL );
                }
            }
            else
                tmp_array = NULL;
            *( ( const void ** )tmp_ptr ) = tmp_array;
            tmp_ptr = ( void * )va_arg( arg_list, uint32_t * );
            assert( tmp_ptr != NULL );
            *( ( uint32_t * )tmp_ptr ) = (uint32_t)cur_elem->array_len;
            break;
        }

        default:
            assert( 0 );
        }
        i++;

        tok = mrn_strtok(NULL, delim, &saveptr);
    }
    free(fmt);
    // Surpress unused variable warning from strtok on GCC
    (void) saveptr;
    mrn_dbg_func_end();
    return;
}
