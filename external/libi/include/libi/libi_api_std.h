/****************************************************************************
 *  Copyright © 2011 The Regents of the University of New Mexico            *
 *  All Rights Reserved                                                     *
 *  Created by Josh Goehner and Dorian Arnold                               *
 *  Department of Computer Science                                          *
 *  Detailed usage rights in "LICENSE" file.                                *
 ****************************************************************************/

#ifndef LIBI_API_STD_H
#define	LIBI_API_STD_H

#include "libi/libi_common.h"

#undef  LIBI_VERSION
#define LIBI_VERSION 000100  //preliminary implementaion 0.1.0

#define LIBI_MAX_BUFFER_LENGTH   4194304 /* 4 MB */

namespace LIBI{
    typedef struct _host_t {
        char* hostname;		/* the host name */
        unsigned int nproc;		/* number of this proc on this host */
        _host_t* next;
    } host_t;

    typedef struct _env_t {
        char* name;		/* environment variable name */
        char* value;		/* environment variable value */
        _env_t* next;
    } env_t;

    typedef struct _dist_req_t{
        char* proc_path;		/* null terminated path to the proc */
        char** proc_argv;		/* null terminated proc arguments */
        host_t* hd;		/* hostname and number of procs */
    } dist_req_t;
}

typedef enum _libi_rc_e {
    LIBI_OK = 0,                /* everything is ok */
    LIBI_YES,                   /* yes */
    LIBI_NO,                    /* no */
    LIBI_NOTIMPL,               /* requested operation not implemented yet */
    LIBI_EINIT,                 /* error with the LIBI initialization */
    LIBI_ELAUNCH,               /* error with the launch */
    LIBI_ESESS,                 /* incorrect session handle */
    LIBI_EFRONTENDONLY,         /* only available to the front end */
    LIBI_ESESSALREADYLAUNCHED,  /* can not launch more procs into a previously launched session */
    LIBI_EIO                    /* error with the IO */
} libi_rc_e;

#endif	/* LIBI_API_STD_H */

