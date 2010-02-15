/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/


#if !defined(__stream_h)
#define __stream_h 1

#include "mrnet/Network.h"
#include "mrnet/Packet.h"
#include "mrnet/Types.h"

struct vector_t;
struct PerfDataMgr_t;
struct Filter_t;

struct Stream_t{
  Network_t* network;
  unsigned short id;
  int sync_filter_id;
  struct Filter_t* sync_filter;
  int us_filter_id;
  struct Filter_t* us_filter;
  int ds_filter_id;
  struct Filter_t *ds_filter;
  Packet_t* incoming_packet_buffer;
  struct vector_t* peers; // peers in stream
  struct PerfDataMgr_t* perf_data;
} ;

typedef struct Stream_t Stream_t;

Stream_t* new_Stream_t(Network_t* net,
                      int iid, 
                      Rank *ibackends,
                      unsigned int inum_backends,
                      int ius_filter_id,
                      int isync_filter_id,
                      int ids_filter_id);

unsigned int Stream_get_Id(Stream_t* stream);

int Stream_find_FilterAssignment(char* assignments, 
                                Rank me, 
                                int filter_id);

Packet_t* Stream_get_IncomingPacket(Stream_t* stream);

int Stream_push_Packet(Stream_t* stream,
                      Packet_t* ipacket,
                      Packet_t* opacket,
                      int igoing_upstream);

void Stream_add_IncomingPacket(Stream_t* stream, Packet_t* packet);

int Stream_send(Stream_t* stream, int itag, const char *iformat_str, ... );

int Stream_send_aux(Stream_t* stream, int itag, char* ifmt, Packet_t* ipacket);

int Stream_flush(Stream_t* stream);

void Stream_set_FilterParams(Stream_t* stream, int upstream, Packet_t* iparams);

int Stream_enable_PerfData(Stream_t* stream, 
                            perfdata_metric_t metric,
                            perfdata_context_t context);

int Stream_disable_PerfData(Stream_t* stream, 
                            perfdata_metric_t metric,
                            perfdata_context_t context);

Packet_t* Stream_collect_PerfData(Stream_t* stream, 
                                perfdata_metric_t metric,
                                perfdata_context_t context,
                                int aggr_strm_id);

void Stream_print_PerfData(Stream_t* stream,
                            perfdata_metric_t metric,
                            perfdata_context_t context);
int Stream_remove_Node(Stream_t* stream, Rank irank);

#endif /* __stream_h */
