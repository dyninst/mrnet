/****************************************************************************
 *  Copyright © 2011 The Regents of the University of New Mexico            *
 *  All Rights Reserved                                                     *
 *  Created by Josh Goehner and Dorian Arnold                               *
 *  Department of Computer Science                                          *
 *  Detailed usage rights in "LICENSE" file.                                *
 ****************************************************************************/

#ifndef LIBI_API_H
#define	LIBI_API_H

#include "libi/libi_common.h"
#include "libi/libi_api_std.h"
#include "libi/ProcessGroup.h"

using namespace LIBI;

typedef struct ProcessGroup* libi_sess_handle_t;
typedef LIBI::host_t host_dist_t;
typedef LIBI::env_t libi_env_t;

typedef struct _sess_env_t{
    libi_sess_handle_t sessionHandle;
    libi_env_t* env;
} sess_env_t;

typedef struct _proc_dist_req_t{
    libi_sess_handle_t sessionHandle; /* only intra-session communication */
    char* proc_path;                  /* null terminated path to the proc */
    char** proc_argv;                 /* null terminated proc arguments */
    host_dist_t* hd;                  /* hostname and number of procs */
} proc_dist_req_t;





libi_rc_e LIBI_fe_init(int ver);

libi_rc_e LIBI_fe_createSession(libi_sess_handle_t& sessionHandle);

libi_rc_e LIBI_fe_launch(proc_dist_req_t distributions[],int nreq);

libi_rc_e LIBI_fe_getMaxMsgBufLen(int* msgbufmax);

libi_rc_e LIBI_fe_sendUsrData(libi_sess_handle_t sessionHandle, void* msgbuf, int msgbuflen);

libi_rc_e LIBI_fe_recvUsrData(libi_sess_handle_t sessionHandle, void** msgbuf, int* msgbuflen);

libi_rc_e LIBI_fe_getHostlistSize(libi_sess_handle_t sessionHandle, unsigned int* size);

libi_rc_e LIBI_fe_getHostlist(libi_sess_handle_t sessionHandle, char*** hostlist, unsigned int* size);

libi_rc_e LIBI_fe_addToSessionEnvironment(libi_sess_handle_t sessionHandle, libi_env_t* envVars);

libi_rc_e LIBI_fe_finalize(libi_sess_handle_t sessionHandle);


//group member functions

libi_rc_e LIBI_init(int ver, int *argc, char ***argv);

libi_rc_e LIBI_finalize();

libi_rc_e LIBI_recvUsrData(void **msgbuf, int *msgbuflen);

libi_rc_e LIBI_sendUsrData(void *msgbuf, int *msgbuflen);

libi_rc_e LIBI_amIMaster();

libi_rc_e LIBI_getMyRank(int *rank);

libi_rc_e LIBI_getSize(int *size);

libi_rc_e LIBI_broadcast(void* buf,int numbyte);

libi_rc_e LIBI_gather(void *sendbuf, int numbyte_per_elem, void* recvbuf);

libi_rc_e LIBI_scatter(void *sendbuf, int numbyte_per_element, void* recvbuf);

libi_rc_e LIBI_barrier();

#endif	/* LIBI_API_H */

