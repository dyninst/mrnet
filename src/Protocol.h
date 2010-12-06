/****************************************************************************
 *  Copyright 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#ifndef __protocol_h
#define __protocol_h 1

#ifdef __cplusplus
namespace MRN
{
#endif

enum ProtocolTags {
/*  0 */     PROT_SUBTREE_INFO_REQ = FirstSystemTag,
/*  1 */     PROT_NEW_SUBTREE,
/*  2 */     PROT_NEW_SUBTREE_RPT,
/*  3 */     PROT_SHUTDOWN,
/*  4 */     PROT_SHUTDOWN_ACK,
/*  5 */     PROT_NEW_STREAM,
/*  6 */     PROT_NEW_HETERO_STREAM,
/*  7 */     PROT_DEL_STREAM,
/*  8 */     PROT_CLOSE_STREAM,
/*  9 */     PROT_NEW_FILTER,
/* 10 */     PROT_SET_FILTERPARAMS_UPSTREAM_TRANS,
/* 11 */     PROT_SET_FILTERPARAMS_UPSTREAM_SYNC,
/* 12 */     PROT_SET_FILTERPARAMS_DOWNSTREAM,
/* 13 */     PROT_EVENT,
/* 14 */     PROT_GET_LEAF_INFO,
/* 15 */     PROT_CONNECT_LEAVES,
/* 16 */     PROT_KILL_SELF,
/* 17 */     PROT_NEW_CHILD_FD_CONNECTION,
/* 18 */     PROT_NEW_CHILD_DATA_CONNECTION,
/* 19 */     PROT_FAILURE_RPT,
/* 20 */     PROT_RECOVERY_RPT,
/* 21 */     PROT_NEW_PARENT_RPT,
/* 22 */     PROT_TOPOLOGY_RPT,
/* 23 */     PROT_TOPOLOGY_ACK,
/* 24 */     PROT_PORT_UPDATE,
/* 25 */     PROT_PORT_UPDATE_ACK,
/* 26 */     PROT_ENABLE_PERFDATA,
/* 27 */     PROT_DISABLE_PERFDATA,
/* 28 */     PROT_COLLECT_PERFDATA,
/* 29 */     PROT_PRINT_PERFDATA,
/* 30 */     PROT_GUI_INIT,
/* 31 */     PROT_GUI_KILL_NODE,
/* 32 */     PROT_GUI_CPUPERCENT,
/* 33 */     PROT_ENABLE_RECOVERY,
/* 34 */     PROT_DISABLE_RECOVERY,
/* 35 */     PROT_TOPO_UPDATE,
/* 36 */     PROT_SUBTREE_INITDONE_RPT,
/* 37 */     PROT_NET_SETTINGS
};

#ifdef __cplusplus
} // namespace MRN
#endif 

#endif /* __protocol_h */
