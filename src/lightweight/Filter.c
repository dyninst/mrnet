/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "Filter.h"
#include "FilterDefinitions.h"
#include "mrnet_lightweight/NetworkTopology.h"
#include "mrnet_lightweight/Stream.h"
#include "utils_lightweight.h"

/* NOTE: Currently, we do not support filtering at lightweight backend
 * nodes, so these functions are in place only as placeholders
 * if we choose to implement the functionality at a later time. */

Filter_t* new_Filter_t(unsigned int UNUSED(iid))
{
    return NULL;
}

void delete_Filter_t(Filter_t* UNUSED(filter))
{
    
}

int Filter_push_Packets(Filter_t* UNUSED(filter), 
                        vector_t* ipackets, 
                        vector_t* opackets,
                        vector_t* opackets_reverse,
                        TopologyLocalInfo_t * topol_info,
                        int igoing_upstream)
{
    void * filter_state = NULL;
    Packet_t * params = NULL;
    Packet_t * ipacket;
    unsigned int stream_id;
	
    ipacket = (Packet_t*)(ipackets->vec[0]);
    assert(ipacket);

    mrn_dbg_func_begin();

    // Special case packets on topology update stream
    stream_id = Packet_get_StreamId(ipacket);
    if( stream_id == TOPOL_STRM_ID ) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "stream_id = 1, executing tfilter_TopoUpdate\n"));
        tfilter_TopoUpdate(ipackets, 
                           opackets, opackets_reverse, 
                           filter_state, params, topol_info, igoing_upstream); 
    } else {
        pushBackElement(opackets, ipacket);
    }

    mrn_dbg_func_end();

    return 0;
}

void Filter_set_FilterParams(Filter_t* UNUSED(filter), Packet_t* UNUSED(iparams))
{

}   

