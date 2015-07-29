/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/


#if !defined(__stream_h)
#define __stream_h 1

#include "mrnet_lightweight/Packet.h"
#include "mrnet_lightweight/Types.h"

#define INVALID_STREAM_ID ((unsigned int)-1)
extern const unsigned int CTL_STRM_ID;
extern const unsigned int TOPOL_STRM_ID;
extern const unsigned int PORT_STRM_ID;
extern const unsigned int USER_STRM_BASE_ID;
extern const unsigned int INTERNAL_STRM_BASE_ID;

struct vector_t;
struct Network_t;
struct PerfDataMgr_t;
struct Filter_t;
struct vector_t;

struct Stream_t {
    struct Filter_t* sync_filter;
    struct Filter_t* us_filter;
    struct Filter_t* ds_filter;
    struct vector_t* incoming_packet_buffer;
    struct PerfDataMgr_t* perf_data;
    struct Network_t* network;
    unsigned int id;
    unsigned int sync_filter_id;
    unsigned int us_filter_id;
    unsigned int ds_filter_id;
    char _was_closed;
};

typedef struct Stream_t Stream_t;

Stream_t* new_Stream_t(struct Network_t* net,
                       unsigned int iid, 
                       Rank *ibackends,
                       unsigned int inum_backends,
                       unsigned int ius_filter_id,
                       unsigned int isync_filter_id,
                       unsigned int ids_filter_id);

/* BEGIN PUBLIC API */

char Stream_is_Closed(Stream_t* stream);

void delete_Stream_t(Stream_t * stream);

unsigned int Stream_get_Id(Stream_t* stream);

int Stream_send(Stream_t* stream, int itag, const char *iformat_str, ...);
int Stream_send_packet(Stream_t* stream, Packet_t* ipacket);

int Stream_flush(Stream_t* stream);

int Stream_recv(Stream_t * stream, int *otag, Packet_t* opacket, bool_t blocking);

/* END PUBLIC API */

int Stream_find_FilterAssignment(char* assignments, 
                                 Rank me, 
                                 int filter_id);

Packet_t* Stream_get_IncomingPacket(Stream_t* stream);

int Stream_push_Packet(Stream_t* stream,
                       Packet_t* ipacket,
                       struct vector_t* opackets,
                       struct vector_t* opackets_reverse,
                       int igoing_upstream);

void Stream_add_IncomingPacket(Stream_t* stream, Packet_t* packet);

int Stream_send_aux(Stream_t* stream, int itag, const char* ifmt, Packet_t* ipacket);

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
                                  unsigned int aggr_strm_id);

void Stream_print_PerfData(Stream_t* stream,
                           perfdata_metric_t metric,
                           perfdata_context_t context);
int Stream_remove_Node(Stream_t* stream, Rank irank);

//DEPRECATED -- renamed is_ShutDown to is_Closed
char Stream_is_ShutDown(Stream_t* stream);

#endif /* __stream_h */
