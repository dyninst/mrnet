/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__filter_h)
#define __filter_h

#include "mrnet_lightweight/NetworkTopology.h"
#include "mrnet_lightweight/Packet.h"
#include "vector.h"

struct Filter_t {
  unsigned short id;
  void* filter_state;
   Packet_t* params;
  char* fmt_str;
} ;

typedef struct Filter_t Filter_t;

Filter_t* new_Filter_t(unsigned short iid);

void delete_Filter_t(Filter_t* filter);

int Filter_push_Packets( Filter_t* filter, 
                         Packet_t* ipacket, 
                         vector_t* opackets,
                         vector_t* opackets_reverse,
                         TopologyLocalInfo_t topol_info);

void Filter_set_FilterParams( Filter_t* filter,  Packet_t* iparams);

#endif /* __filter_h */
