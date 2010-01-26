/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "Filter.h"
#include "utils.h"

Filter_t* new_Filter_t(unsigned short iid)
{
  Filter_t* new_filter = (Filter_t*)malloc(sizeof(Filter_t));
  assert(new_filter);

  new_filter->id=iid;
  new_filter->filter_state = NULL;
  new_filter->params = (Packet_t*)malloc(sizeof(Packet_t));
  assert(new_filter->params);

  return new_filter;
}

int Filter_push_Packets(Filter_t* filter, 
                        Packet_t* ipacket, 
                        Packet_t* opacket,
                        TopologyLocalInfo_t topol_info)
{
    mrn_dbg_func_begin();
    *opacket = *ipacket;
    mrn_dbg(3, mrn_printf(FLF, stderr, "NULL FILTER: returning %d packets\n", 1));
    mrn_dbg_func_end();
    return 0;
}

void Filter_set_FilterParams(Filter_t* filter, Packet_t* iparams)
{
    mrn_dbg_func_begin();
    filter->params = iparams;
    mrn_dbg_func_end();
}   

