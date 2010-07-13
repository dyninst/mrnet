/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "Filter.h"
#include "utils_lightweight.h"

/* NOTE: Currently, we do not support filtering at lightweight backend
 * nodes, so these functions are in place only as placeholders
 * if we choose to implement the functionality at a later time. */

Filter_t* new_Filter_t(unsigned short iid)
{
  return NULL;
}

void delete_Filter_t(Filter_t* filter)
{
    
}

int Filter_push_Packets(Filter_t* filter, 
                        Packet_t* ipacket, 
                        Packet_t* opacket,
                        TopologyLocalInfo_t topol_info)
{
    *opacket = *ipacket;
    return 0;
}

void Filter_set_FilterParams(Filter_t* filter, Packet_t* iparams)
{
}   

