/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__filterdefinitions_h)
#define __filterdefinitions_h 1

#include "mrnet_lightweight/FilterIds.h"
#include "mrnet_lightweight/Packet.h"
#include "mrnet_lightweight/NetworkTopology.h"
#include "xplat_lightweight/vector.h"

extern FilterId TFILTER_NULL;

extern FilterId SFILTER_WAITFORALL;
extern FilterId SFILTER_DONTWAIT;
extern FilterId SFILTER_TIMEOUT;

void tfilter_TopoUpdate(vector_t * ipackets,
        vector_t* opackets,
        vector_t* opackets_reverse,
        void ** v,
        Packet_t* pkt,
        TopologyLocalInfo_t * info,
        int igoingupstream);

#endif
