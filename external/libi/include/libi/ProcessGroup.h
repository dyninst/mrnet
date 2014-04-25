/****************************************************************************
 *  Copyright © 2011 The Regents of the University of New Mexico            *
 *  All Rights Reserved                                                     *
 *  Created by Josh Goehner and Dorian Arnold                               *
 *  Department of Computer Science                                          *
 *  Detailed usage rights in "LICENSE" file.                                *
 ****************************************************************************/

#ifndef LIBI_PROCESS_GROUP_H
#define LIBI_PROCESS_GROUP_H

#include <cstdlib>
#include <string>
#include <vector>
#include "libi/libi_api_std.h"
#include "libi/debug.h"
#include "xplat/NetUtils.h"

namespace LIBI{

enum topo_state { NONE, MASTER_CONNECTED, ALL_CONNECTED, FAILURE };
class ProcessGroup
{
public:
    ProcessGroup();
    ~ProcessGroup();

    bool isLaunched;
    bool isFinalized;
    char** host_list;
    unsigned int host_list_size;
    unsigned int ndist;

    //front end functions
    libi_rc_e launch(dist_req_t distributions[], int nreq, env_t* env);

    libi_rc_e sendUsrData(void* msgbuf, int msgbuflen);

    libi_rc_e recvUsrData(void** msgbuf, int* msgbuflen);

    libi_rc_e getHostlist(char*** hostlist,unsigned int *size);

    libi_rc_e finalize();

private:
    static int fe_count;
    int lmon_session;
    int master_fd;
    topo_state report;
    int num_procs;

    debug* dbg;
};

}

#endif // LIBI_PROCESS_H
