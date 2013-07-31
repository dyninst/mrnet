/****************************************************************************
 *  Copyright © 2011 The Regents of the University of New Mexico            *
 *  All Rights Reserved                                                     *
 *  Created by Josh Goehner and Dorian Arnold                               *
 *  Department of Computer Science                                          *
 *  Detailed usage rights in "LICENSE" file.                                *
 ****************************************************************************/

#ifndef LIBI_PROCESS_GROUP_MEMBER_H
#define LIBI_PROCESS_GROUP_MEMBER_H

#include <cstdlib>
#include <string>
#include <vector>
#include "xplat/Types-unix.h"
#include "xplat/NetUtils.h"
#include "ProcessGroup.h"

namespace LIBI
{

class ProcessGroupMember
{
public:
    ProcessGroupMember();
    ~ProcessGroupMember();

    libi_rc_e init(int *argc, char ***argv);

    libi_rc_e finalize();

    libi_rc_e recvUsrData(void **msgbuf, int *msgbuflen);

    libi_rc_e sendUsrData(void *msgbuf, int *msgbuflen);

    libi_rc_e amIMaster();

    libi_rc_e getMyRank(int *rank);

    libi_rc_e getSize(int *size);

    libi_rc_e broadcast(void* buf,int numbyte);

    libi_rc_e gather(void *sendbuf, int numbyte_per_elem, void* recvbuf);

    libi_rc_e scatter(void *sendbuf, int numbyte_per_element, void* recvbuf);

    libi_rc_e barrier();

    libi_rc_e isInitialized();

private:
    bool isInit;

    bool isMaster;
    unsigned int numProcs;
    int myRank;
    //Changed 7/23/12 by Taylor
	//int parent_fd;
	XPlat_Socket parent_fd;
    int num_children;
    int * child_fds;
    int * child_rank;
    int * descendants;
    int subtree_size;
};



} // namespace LIBI

#endif // LIBI_PROCESS_H
