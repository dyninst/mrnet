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
/*  2 */     PROT_SHUTDOWN,
/*  3 */     PROT_SHUTDOWN_ACK,
/*  4 */     PROT_NEW_STREAM,
/*  5 */     PROT_NEW_HETERO_STREAM,
/*  6 */     PROT_DEL_STREAM,
/*  7 */     PROT_CLOSE_STREAM,
/*  8 */     PROT_NEW_FILTER,
/*  9 */     PROT_SET_FILTERPARAMS_UPSTREAM_TRANS,
/* 10 */     PROT_SET_FILTERPARAMS_UPSTREAM_SYNC,
/* 11 */     PROT_SET_FILTERPARAMS_DOWNSTREAM,
/* 12 */     PROT_EVENT,
/* 13 */     PROT_GET_LEAF_INFO,
/* 14 */     PROT_CONNECT_LEAVES,
/* 15 */     PROT_KILL_SELF,
/* 16 */     PROT_NEW_CHILD_FD_CONNECTION,
/* 17 */     PROT_NEW_CHILD_DATA_CONNECTION,
/* 18 */     PROT_FAILURE_RPT,
/* 19 */     PROT_RECOVERY_RPT,
/* 20 */     PROT_NEW_PARENT_RPT,
/* 21 */     PROT_TOPOLOGY_RPT,
/* 22 */     PROT_TOPOLOGY_ACK,
/* 23 */     PROT_PORT_UPDATE,
/* 24 */     PROT_ENABLE_PERFDATA,
/* 25 */     PROT_DISABLE_PERFDATA,
/* 26 */     PROT_COLLECT_PERFDATA,
/* 27 */     PROT_PRINT_PERFDATA,
/* 28 */     PROT_ENABLE_RECOVERY,
/* 29 */     PROT_DISABLE_RECOVERY,
/* 30 */     PROT_TOPO_UPDATE,
/* 31 */     PROT_SUBTREE_INITDONE_RPT,
/* 32 */     PROT_NET_SETTINGS,
/* 33 */     PROT_LAST
};

#ifdef __cplusplus
} // namespace MRN
#endif 

#endif /* __protocol_h */
