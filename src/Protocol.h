/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#ifndef __protocol_h
#define __protocol_h 1

namespace MRN
{

enum ProtocolTags {
    PROT_SUBTREE_INFO_REQ = FirstSystemTag,
    PROT_NEW_SUBTREE,
    PROT_NEW_SUBTREE_RPT,
    PROT_SHUTDOWN,
    PROT_SHUTDOWN_ACK,
    PROT_NEW_STREAM,
    PROT_NEW_HETERO_STREAM,
    PROT_DEL_STREAM,
    PROT_CLOSE_STREAM,
    PROT_NEW_FILTER,
    PROT_SET_FILTERPARAMS_UPSTREAM_TRANS,
    PROT_SET_FILTERPARAMS_UPSTREAM_SYNC,
    PROT_SET_FILTERPARAMS_DOWNSTREAM,
    PROT_EVENT,
    PROT_GET_LEAF_INFO,
    PROT_CONNECT_LEAVES,
    PROT_KILL_SELF,
    PROT_NEW_CHILD_FD_CONNECTION,
    PROT_NEW_CHILD_DATA_CONNECTION,
    PROT_FAILURE_RPT,
    PROT_RECOVERY_RPT,
    PROT_NEW_PARENT_RPT,
    PROT_TOPOLOGY_RPT,
    PROT_TOPOLOGY_ACK,
    PROT_ENABLE_PERFDATA,
    PROT_DISABLE_PERFDATA,
    PROT_COLLECT_PERFDATA,
    PROT_PRINT_PERFDATA,
    PROT_GUI_INIT,
    PROT_GUI_KILL_NODE,
    PROT_GUI_CPUPERCENT
};

} // namespace MRN

#endif /* __protocol_h */
