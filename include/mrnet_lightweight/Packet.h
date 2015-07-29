/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/


#if !defined(__packet_h)
#define __packet_h 1

#include <stdarg.h>

#include "mrnet_lightweight/DataElement.h"
#include "mrnet_lightweight/Types.h"

struct vector_t;
struct PDR;

typedef struct {
  uint32_t stream_id;
  int32_t tag; // application/protocol level ID
  Rank src_rank;  // null terminated string
  char* fmt_str;  // null terminated string
  char* hdr;  // the packed header
  unsigned int hdr_len;
  char* buf;  // the packed data payload
  uint64_t buf_len;
  Rank inlet_rank;
  struct vector_t* data_elements;
  char byteorder;
} Packet_t;

/* BEGIN PUBLIC API */

Packet_t* new_Packet_t(Rank isrc, unsigned int istream_id, int itag, 
                       const char* fmt, va_list arg_list);

Packet_t* new_Packet_t_2(unsigned int istream_id, int itag, const char* fmt, ...);

int Packet_get_Tag(Packet_t* packet);
void Packet_set_Tag(Packet_t* packet, int tag);

unsigned int Packet_get_StreamId(Packet_t* packet);
void Packet_set_StreamId(Packet_t* packet, unsigned int strm_id);

char* Packet_get_FormatString(Packet_t* packet);

int Packet_unpack(Packet_t* packet, const char *ifmt_str, ...);
int Packet_unpack_valist(Packet_t* packet, va_list arg_list, const char *ifmt_str);

void delete_Packet_t(Packet_t* packet);
void Packet_set_DestroyData(Packet_t * packet, int dd);

/* END PUBLIC API */

Packet_t* Packet_construct(Packet_t* packet);


Packet_t* new_Packet_t_3(unsigned int ihdr_len, char* ihdr, 
                         uint64_t ibuf_len, char* ibuf, 
                         Rank iinlet_rank);

void Packet_pushBackElement(Packet_t* packet, DataElement_t* cur_elem);

int Packet_ExtractVaList(Packet_t* packet, const char* fmt, va_list arg_list);
void Packet_ArgList2DataElementArray(Packet_t* packet, va_list arg_list);
void Packet_DataElementArray2ArgList(Packet_t* packet, va_list arg_list);

bool_t Packet_pdr_packet_header(struct PDR *pdrs,  Packet_t* pkt);
bool_t Packet_pdr_packet_data(struct PDR *pdrs,  Packet_t* pkt);

#endif /* __packet_h */
