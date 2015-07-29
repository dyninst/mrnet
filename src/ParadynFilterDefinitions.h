/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined( paradynfilterdefinitions_h )
#define paradynfilterdefinitions_h 1

#include <vector>
#include "mrnet/ParadynFilterIds.h"
#include "mrnet/Packet.h"

namespace MRN
{

extern const char * save_LocalClockSkewUpstream_format_string;
void save_LocalClockSkewUpstream( const std::vector < PacketPtr >&,
                                  std::vector < PacketPtr >&,
                                  void**, PacketPtr&, const TopologyLocalInfo& );

extern const char * save_LocalClockSkewDownstream_format_string;
void save_LocalClockSkewDownstream( const std::vector < PacketPtr >&,
                                    std::vector < PacketPtr >&,
                                    void**, PacketPtr&, const TopologyLocalInfo& );

extern const char * get_ClockSkew_format_string;
void get_ClockSkew( const std::vector < PacketPtr >&,
                    std::vector < PacketPtr >&,
                    void**, PacketPtr&, const TopologyLocalInfo& );

extern const char * tfilter_PDUIntEqClass_format_string;
void tfilter_PDUIntEqClass( const std::vector < PacketPtr >&,
                           std::vector < PacketPtr >&,
                           void**, PacketPtr&, const TopologyLocalInfo& );
}

#endif /* paradynfilterdefinitions_h */
