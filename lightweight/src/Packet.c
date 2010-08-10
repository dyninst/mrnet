/****************************************************************************
 *  Copyright 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef os_windows
#include <unistd.h>
#endif //ifdef os_windows

#include "mrnet_lightweight/DataElement.h"
#include "mrnet_lightweight/Network.h"
#include "mrnet_lightweight/Error.h"
#include "pdr.h"
#include "pdr_mem.h"
#include "mrnet_lightweight/Packet.h"
#include "mrnet_lightweight/Types.h"
#include "utils_lightweight.h"
#include "vector.h"

void free_Packet_t(Packet_t* packet)
{
    if (packet->data_elements != NULL)
      delete_vector_t(packet->data_elements); 
    free(packet);
}

Packet_t* new_Packet_t(int val, unsigned short _stream_id,
                       int _tag, char* fmt,
                       va_list arg_list)
{
  
  PDR pdrs;
  Packet_t* packet = (Packet_t*)malloc(sizeof(Packet_t));
  assert(packet != NULL);
  
  // initialize values
  packet->stream_id = _stream_id;
  packet->tag = _tag;
  packet->fmt_str = strdup(fmt);
  packet->buf = NULL;
  packet->inlet_rank = UnknownRank;
  packet->destroy_data = false;
  packet->data_elements = new_empty_vector_t();

  mrn_dbg(3, mrn_printf(FLF, stderr, "stream_id:%d, tag:%d, fmt:\"%s\"\n", packet->stream_id, packet->tag, packet->fmt_str));

  packet->src_rank = UnknownRank;
  Packet_ArgList2DataElementArray(packet, arg_list);

  packet->buf_len = pdr_sizeof((pdrproc_t)(Packet_pdr_packet), packet);
  assert(packet->buf_len);
  packet->buf = (char*)malloc(packet->buf_len);
  assert(packet->buf);

  pdrmem_create(&pdrs, packet->buf, packet->buf_len, PDR_ENCODE);

  if (!Packet_pdr_packet(&pdrs, packet)) {
    error(ERR_PACKING, UnknownRank, "pdr_packet() failed\n");
    mrn_dbg(5, mrn_printf(FLF, stderr, "pdr_packet() failed\n"));
    return NULL;
  }

  mrn_dbg(3, mrn_printf(FLF, stderr,
          "Packet(%p) constructor succeeded: src:%u, stream_id:%d "
          "tag:%d, fmt:%s\n", packet, packet->src_rank, packet->stream_id, packet->tag, packet->fmt_str));

  return packet;

}
Packet_t* new_Packet_t_2(unsigned short istream_id, int itag, 
                         /*const*/ char* ifmt_str, ... )
{
  va_list arg_list;
  PDR pdrs;
    
  Packet_t* packet = (Packet_t*)malloc(sizeof(Packet_t));
  assert(packet != NULL);
  
  // initialize values
  packet->stream_id = (uint16_t)istream_id;
  packet->tag = itag;
  packet->fmt_str = strdup(ifmt_str);
  packet->buf = NULL;
  packet->inlet_rank = UnknownRank;
  packet->destroy_data = false;
  packet->data_elements = new_empty_vector_t();

  mrn_dbg(2, mrn_printf(FLF, stderr, "stream_id:%d, tag: %d, fmt: \"%s\"\n",
                        istream_id, itag, ifmt_str));

  va_start(arg_list, ifmt_str);
  Packet_ArgList2DataElementArray(packet, arg_list);
  va_end(arg_list);

  packet->buf_len = pdr_sizeof ((pdrproc_t) (Packet_pdr_packet), packet);
  assert( packet->buf_len );
  packet->buf = ( char* ) malloc( packet->buf_len );
  assert( packet->buf );

  pdrmem_create(&pdrs, packet->buf, packet->buf_len, PDR_ENCODE);

  if (!Packet_pdr_packet(&pdrs, packet)) {
    error (ERR_PACKING, UnknownRank, "pdr_packet() failed\n");
    mrn_dbg(5, mrn_printf(FLF, stderr, "pdr_packet() failed\n"));
    return NULL;
  }

  mrn_dbg(3, mrn_printf(FLF, stderr,
          "Packet(%p) constructor succeeded: stream_id:%d "
          "tag:%d, fmt:%s\n", packet, istream_id, itag, ifmt_str));

  return packet;
}

Packet_t* new_Packet_t_3(unsigned int ibuf_len, char* ibuf, Rank iinlet_rank)
{
  PDR pdrs;
  Packet_t* packet = (Packet_t*)malloc(sizeof(Packet_t));
  assert(packet != NULL);
  
  // initialize values
  packet->stream_id = 0;
  packet->fmt_str = NULL;
  packet->buf = ibuf;
  packet->buf_len = ibuf_len;
  packet->inlet_rank = iinlet_rank;
  packet->destroy_data = true;
  packet->data_elements = new_empty_vector_t();

  mrn_dbg_func_begin();
  
  if (packet->buf_len == 0) 
    return NULL;

  pdrmem_create(&pdrs, packet->buf, packet->buf_len, PDR_DECODE);

  if (!Packet_pdr_packet(&pdrs, packet)) {
    mrn_dbg(1, mrn_printf(FLF,stderr, "pdr_packet() failed\n"));
    error(ERR_PACKING,UnknownRank, "pdr_packet() failed\n");
  }

  mrn_dbg(3, mrn_printf(FLF, stderr,  
                        "Packet(%p): src:%u, stream_id:%d, tag:%d, fmt:%s\n", packet, packet->src_rank, packet->stream_id, packet->tag, packet->fmt_str));

  return packet;

}

void Packet_set_DestroyData(Packet_t * packet, int dd)
{
    packet->destroy_data = dd;
}

int Packet_get_Tag(Packet_t* packet)
{
    return packet->tag;
}

unsigned short Packet_get_StreamId(Packet_t* packet)
{
    return packet->stream_id;
}

char* Packet_get_FormatString(Packet_t* packet) 
{
    return packet->fmt_str;
}

void Packet_ArgList2DataElementArray(Packet_t* packet, va_list arg_list)
{
  DataElement_t* cur_elem = new_DataElement_t();
  char* fmt;
  char* delim = " \t\n%";
  char* tok;
    
  mrn_dbg_func_begin();

  fmt = (char*)malloc(sizeof(char)*(strlen(packet->fmt_str)+1));
  assert(fmt);
  if (packet->fmt_str != NULL)
        strncpy(fmt, packet->fmt_str, strlen(packet->fmt_str)+1);

  tok = strtok(fmt, delim);

  while (tok != NULL) {
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

	packet = Packet_pushBackElement(packet, cur_elem);

    tok = strtok(NULL, delim);
  }

  free(cur_elem);
  free(fmt);

  mrn_dbg_func_end();

}

Packet_t* Packet_pushBackElement(Packet_t* packet, DataElement_t* cur_elem)
{
	DataElement_t* new_elem = (DataElement_t*)malloc(sizeof(DataElement_t));
    assert(new_elem);
	*new_elem = *cur_elem;
	pushBackElement(packet->data_elements, new_elem);

	return packet;
}

bool_t Packet_pdr_packet( PDR * pdrs, Packet_t * pkt )
{
    
    unsigned int i;
    bool_t retval = 0;
    DataElement_t * cur_elem;
    char* op_str;
    char* fmt;
    char* delim = " \t\n%";
    char* tok;
    void **vp;
    char **cp;

    op_str = (char*)malloc(sizeof(char)*9);
    assert(op_str);
    if( pdrs->p_op == PDR_ENCODE )
        strcpy(op_str, "ENCODING");
    else if( pdrs->p_op == PDR_DECODE )
        strcpy(op_str,"DECODING");
    else
        strcpy(op_str,"FREEING");
    mrn_dbg( 3, mrn_printf(FLF, stderr, "op: %s\n", op_str ));
    free(op_str);

    /* Process Packet Header into/out of the pdr mem (see Packet.h for header layout) */
  

    if( pdr_uint16( pdrs, &( pkt->stream_id ) ) == false ) {
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

    if( !pkt->fmt_str) {
        mrn_dbg( 3, mrn_printf(FLF, stderr,
                    "No data in message. just header info\n" ));
        return true;
    }

    if (pkt->fmt_str) {
        fmt = (char*)malloc(sizeof(char)*(strlen(pkt->fmt_str)+1));
        assert(fmt);
        strncpy(fmt, pkt->fmt_str, strlen(pkt->fmt_str)+1);
    } else {
        fmt = "";
    }

    i = 0;
    tok = strtok(fmt, delim);
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
                           &( cur_elem->array_len ), INT32_MAX,
                           sizeof( uchar_t ), ( pdrproc_t ) pdr_uchar );
            break;

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
                           &( cur_elem->array_len ), INT32_MAX,
                           sizeof( uint16_t ), ( pdrproc_t ) pdr_uint16 );
            break;

        case INT32_T:
        case UINT32_T:
            retval =
                pdr_uint32( pdrs, ( uint32_t * ) ( &( cur_elem->val.d ) ) );
            break;
        case INT32_ARRAY_T:
        case UINT32_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval =
                pdr_array( pdrs, &cur_elem->val.p,
                           &( cur_elem->array_len ), INT32_MAX,
                           sizeof( uint32_t ), ( pdrproc_t ) pdr_uint32 );
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
                                &( cur_elem->array_len ), INT32_MAX,
                                sizeof( uint64_t ), ( pdrproc_t ) pdr_uint64 );
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
                           &( cur_elem->array_len ), INT32_MAX,
                           sizeof( float ), ( pdrproc_t ) pdr_float );
            break;
        case DOUBLE_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval =
                pdr_array( pdrs, &cur_elem->val.p,
                           &( cur_elem->array_len ), INT32_MAX,
                           sizeof( double ), ( pdrproc_t ) pdr_double );
            break;
        case STRING_ARRAY_T:
            if( pdrs->p_op == PDR_DECODE ) {
                cur_elem->val.p = NULL;
            }
            retval = pdr_array( pdrs, &cur_elem->val.p,
                            &(cur_elem->array_len), INT32_MAX,
                            sizeof(char*),
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
            Packet_pushBackElement(pkt, cur_elem);
        }

        tok = strtok(NULL, delim);
        i++;
    }

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

int Packet_ExtractVaList(Packet_t* packet, char* fmt, va_list arg_list)
{
  mrn_dbg(3, mrn_printf(FLF, stderr, "In ExtractvalList(%p)\n", packet));

  Packet_DataElementArray2ArgList(packet, arg_list);

  mrn_dbg(3, mrn_printf(FLF, stderr, "Packet_ExtractVaList(%p) succeeded\n", packet));
  
  return 0;
}

void Packet_DataElementArray2ArgList(Packet_t* packet, va_list arg_list)
{
  int i = 0;
  int array_len = 0;
  DataElement_t* cur_elem;
  void *tmp_ptr;
  void *tmp_array;
  char* fmt;
  char* delim = " \t\n%";
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

  fmt = (char*)malloc(sizeof(char)*(strlen(packet->fmt_str)+1));
  assert(fmt != NULL);
  if (packet->fmt_str != NULL)
    strncpy(fmt, packet->fmt_str, strlen(packet->fmt_str)+1);

  tok = strtok(fmt, delim);
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

      case CHAR_ARRAY_T:
      case UCHAR_ARRAY_T: {
          tmp_ptr = ( void * )va_arg( arg_list, void ** );
          assert( tmp_ptr != NULL );
          array_len = cur_elem->array_len * sizeof(char);
          if( array_len > 0 ) {
             tmp_array = malloc(array_len);
             assert( tmp_array != NULL );
             memcpy( tmp_array, cur_elem->val.p, array_len );
          }
          else
             tmp_array = NULL;
          *( ( const void ** )tmp_ptr ) = tmp_array;
          tmp_ptr = ( void * )va_arg( arg_list, int * );
          assert( tmp_ptr != NULL );
          *( ( int * )tmp_ptr ) = cur_elem->array_len;
          break;
      }

      case INT32_ARRAY_T:
      case UINT32_ARRAY_T: {
          tmp_ptr = ( void * )va_arg( arg_list, void ** );
          assert( tmp_ptr != NULL );
          array_len = cur_elem->array_len * sizeof(int);
          if( array_len > 0 ) {
             tmp_array = malloc(array_len);
             assert( tmp_array != NULL );
             memcpy( tmp_array, cur_elem->val.p, array_len );
          }
          else
             tmp_array = NULL;
          *( ( const void ** )tmp_ptr ) = tmp_array;
          tmp_ptr = ( void * )va_arg( arg_list, int * );
          assert( tmp_ptr != NULL );
          *( ( int * )tmp_ptr ) = cur_elem->array_len;
          break;
      }

      case INT16_ARRAY_T:
      case UINT16_ARRAY_T: {
          tmp_ptr = ( void * )va_arg( arg_list, void ** );
          assert( tmp_ptr != NULL );
          array_len = cur_elem->array_len * sizeof(short int);
          if( array_len > 0 ) {
             tmp_array = malloc(array_len);
             assert( tmp_array != NULL );
             memcpy( tmp_array, cur_elem->val.p, array_len );
          }
          else
             tmp_array = NULL;
          *( ( const void ** )tmp_ptr ) = tmp_array;
          tmp_ptr = ( void * )va_arg( arg_list, int * );
          assert( tmp_ptr != NULL );
          *( ( int * )tmp_ptr ) = cur_elem->array_len;
          break;
      }

      case INT64_ARRAY_T:
      case UINT64_ARRAY_T: {
          tmp_ptr = ( void * )va_arg( arg_list, void ** );
          assert( tmp_ptr != NULL );
          array_len = cur_elem->array_len * sizeof(int64_t);
          if( array_len > 0 ) {
             tmp_array = malloc(array_len);
             assert( tmp_array != NULL );
             memcpy( tmp_array, cur_elem->val.p, array_len );
          }
          else
             tmp_array = NULL;
          *( ( const void ** )tmp_ptr ) = tmp_array;
          tmp_ptr = ( void * )va_arg( arg_list, int * );
          assert( tmp_ptr != NULL );
          *( ( int * )tmp_ptr ) = cur_elem->array_len;
          break;
      }

      case FLOAT_ARRAY_T: {
          tmp_ptr = ( void * )va_arg( arg_list, void ** );
          assert( tmp_ptr != NULL );
          array_len = cur_elem->array_len * sizeof(float);
          if( array_len > 0 ) {
             tmp_array = malloc(array_len);
             assert( tmp_array != NULL );
             memcpy( tmp_array, cur_elem->val.p, array_len );
          }
          else
             tmp_array = NULL;
          *( ( const void ** )tmp_ptr ) = tmp_array;
          tmp_ptr = ( void * )va_arg( arg_list, int * );
          assert( tmp_ptr != NULL );
          *( ( int * )tmp_ptr ) = cur_elem->array_len;
          break;
      }

      case DOUBLE_ARRAY_T: {
          tmp_ptr = ( void * )va_arg( arg_list, void ** );
          assert( tmp_ptr != NULL );
          array_len = cur_elem->array_len * sizeof(double);
          if( array_len > 0 ) {
             tmp_array = malloc(array_len);
             assert( tmp_array != NULL );
             memcpy( tmp_array, cur_elem->val.p, array_len );
          }
          else
             tmp_array = NULL;
          *( ( const void ** )tmp_ptr ) = tmp_array;
          tmp_ptr = ( void * )va_arg( arg_list, int * );
          assert( tmp_ptr != NULL );
          *( ( int * )tmp_ptr ) = cur_elem->array_len;
          break;
      }

      case STRING_ARRAY_T: {
          tmp_ptr = ( void * )va_arg( arg_list, void ** );
          assert( tmp_ptr != NULL );
          array_len = cur_elem->array_len * sizeof(char*);
          if( array_len > 0 ) {
             tmp_array = malloc(array_len);
             assert( tmp_array != NULL );
             for(j = 0; j < cur_elem->array_len; j++ ) {
                (( char ** ) tmp_array)[j] = strdup( ((const char **)(cur_elem->val.p))[j] );
                assert( (( char ** ) tmp_array)[j] != NULL );
             }
          }
          else
             tmp_array = NULL;
          *( ( const void ** )tmp_ptr ) = tmp_array;
          tmp_ptr = ( void * )va_arg( arg_list, int * );
          assert( tmp_ptr != NULL );
          *( ( int * )tmp_ptr ) = cur_elem->array_len;
          break;
      }

      default:
          assert( 0 );
      }
      i++;

      tok = strtok(NULL, delim);
  }

  mrn_dbg_func_end();
  return;
}

int Packet_ExtractArgList(Packet_t* packet, char* ifmt_str, ... )
{
  va_list arg_list;

  mrn_dbg(2, mrn_printf(FLF, stderr, "fmt:\"%s\"\n", ifmt_str));

  va_start(arg_list, ifmt_str);
  Packet_DataElementArray2ArgList(packet, arg_list);
  va_end(arg_list);
  
  return 0;
}

