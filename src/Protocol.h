/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#ifndef __protocol_h
#define __protocol_h 1

#ifdef __cplusplus
namespace MRN {
#endif

enum ProtocolTags {
/*  0 */     PROT_FIRST = FirstSystemTag,
/*  1 */     PROT_LAUNCH_SUBTREE,
/*  2 */     PROT_SUBTREE_INITDONE_RPT,
/*  3 */     PROT_SHUTDOWN,
/*  4 */     PROT_SHUTDOWN_ACK,
/*  5 */     PROT_NEW_STREAM,
/*  6 */     PROT_NEW_HETERO_STREAM,
/*  7 */     PROT_NEW_INTERNAL_STREAM,
/*  8 */     PROT_NEW_STREAM_ACK,
/*  9 */     PROT_DEL_STREAM,
/* 10 */     PROT_CLOSE_STREAM,
/* 11 */     PROT_NEW_FILTER,
/* 12 */     PROT_SET_FILTERPARAMS_UPSTREAM_TRANS,
/* 13 */     PROT_SET_FILTERPARAMS_UPSTREAM_SYNC,
/* 14 */     PROT_SET_FILTERPARAMS_DOWNSTREAM,
/* 15 */     PROT_EVENT,
/* 16 */     PROT_KILL_SELF, // deprecated
/* 17 */     PROT_NEW_CHILD_FD_CONNECTION,
/* 18 */     PROT_NEW_CHILD_DATA_CONNECTION,
/* 19 */     PROT_FAILURE_RPT,
/* 20 */     PROT_RECOVERY_RPT,
/* 21 */     PROT_PORT_UPDATE,
/* 22 */     PROT_ENABLE_PERFDATA,
/* 23 */     PROT_DISABLE_PERFDATA,
/* 24 */     PROT_COLLECT_PERFDATA,
/* 25 */     PROT_PRINT_PERFDATA,
/* 26 */     PROT_ENABLE_RECOVERY,
/* 27 */     PROT_DISABLE_RECOVERY,
/* 28 */     PROT_TOPO_UPDATE,
/* 29 */     PROT_NET_SETTINGS,
/* 30 */     PROT_EDT_SHUTDOWN,
/* 31 */     PROT_EDT_REMOTE_SHUTDOWN,
/* 32 */     PROT_LAST
};

#ifdef __cplusplus
} // namespace MRN
#endif 

#endif /* __protocol_h */
