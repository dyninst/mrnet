/****************************************************************************
 *  Copyright 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
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
  unsigned int buf_len;
  Rank inlet_rank;
  int destroy_data;
  struct vector_t* data_elements;
} Packet_t;


Packet_t* Packet_construct(Packet_t* packet);

Packet_t* new_Packet_t(Rank isrc, unsigned int istream_id, int itag, 
                       const char* fmt, va_list arg_list);

Packet_t* new_Packet_t_2(unsigned int istream_id, int itag, const char* fmt, ...);

Packet_t* new_Packet_t_3(unsigned int ihdr_len, char* ihdr, 
                         unsigned int ibuf_len, char* ibuf, 
                         Rank iinlet_rank);

void Packet_set_DestroyData(Packet_t * packet, int dd);

int Packet_get_Tag(Packet_t* packet);
unsigned short Packet_get_StreamId(Packet_t* packet);
char* Packet_get_FormatString(Packet_t* packet);

Packet_t* Packet_pushBackElement(Packet_t* packet, DataElement_t* cur_elem);

int Packet_ExtractVaList(Packet_t* packet, const char* fmt, va_list arg_list);
void Packet_ArgList2DataElementArray(Packet_t* packet, va_list arg_list);
void Packet_DataElementArray2ArgList(Packet_t* packet, va_list arg_list);

bool_t Packet_pdr_packet_header(struct PDR *pdrs,  Packet_t* pkt);
bool_t Packet_pdr_packet_data(struct PDR *pdrs,  Packet_t* pkt);

int Packet_unpack(Packet_t* packet, const char *ifmt_str, ...);

#endif /* __packet_h */
