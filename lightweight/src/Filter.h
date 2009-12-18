/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__filter_h)
#define __filter_h

#include "mrnet/NetworkTopology.h"
#include "mrnet/Packet.h"

typedef struct {
  unsigned short id;
  void* filter_state;
   Packet_t* params;
  char* fmt_str;
} Filter_t;


 Filter_t* new_Filter_t(unsigned short iid);

int Filter_push_Packets( Filter_t* filter, 
                         Packet_t* ipacket, 
                         Packet_t* opacket,
                         TopologyLocalInfo_t topol_info);

void Filter_set_FilterParams( Filter_t* filter,  Packet_t* iparams);

#endif /* __filter_h */
