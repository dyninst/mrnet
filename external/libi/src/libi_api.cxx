/****************************************************************************
 *  Copyright © 2011 The Regents of the University of New Mexico            *
 *  All Rights Reserved                                                     *
 *  Created by Josh Goehner and Dorian Arnold                               *
 *  Department of Computer Science                                          *
 *  Detailed usage rights in "LICENSE" file.                                *
 ****************************************************************************/

#include <string>
#include <vector>
#include "libi/libi_api.h"
#include "libi/ProcessGroup.h"
#include "libi/ProcessGroupMember.h"
#include <stdio.h>

using namespace LIBI;

typedef struct _libi_conn_desc_t {
    char* hostname;
    int sendFD;
    int receiveFD;
} libi_conn_desc_t;

typedef struct _libi_sess_container_t{
    libi_sess_handle_t sess;
    libi_env_t* env;
    _libi_sess_container_t* next;
} libi_sess_container_t;

libi_sess_container_t* sessions = NULL;
libi_sess_container_t** latest = NULL;

ProcessGroupMember* libi_member;

libi_rc_e LIBI_fe_init(int ver){

    sessions = (libi_sess_container_t*)malloc(sizeof(libi_sess_container_t));
    latest = &sessions;

    return LIBI_OK;
}

libi_rc_e LIBI_fe_createSession(libi_sess_handle_t& sessionHandle){

    if(sessions == NULL || latest == NULL){
        return LIBI_EINIT;
    }

    (*latest)->next = (libi_sess_container_t*)malloc(sizeof(libi_sess_container_t));
    latest = &((*latest)->next);

    (*latest)->sess = new ProcessGroup();
    (*latest)->env = NULL;
    (*latest)->next = NULL;
    
    sessionHandle = (*latest)->sess;
    
    return LIBI_OK;
}

libi_rc_e LIBI_fe_launch(proc_dist_req_t distributions[], int nreq ){
    libi_rc_e rc;
    if(sessions == NULL){
        return LIBI_EINIT;
    }

    int i;
    std::vector<libi_sess_container_t*> us;
    std::vector<libi_sess_container_t*>::iterator iter;
    for(i=0; i<nreq; i++){
        bool found=false;
        libi_sess_handle_t cur = distributions[i].sessionHandle;
        for(iter = us.begin(); iter != us.end() && !found; iter++){
            if((*iter)->sess == cur)
                found=true;
        }
        if(!found){
            libi_sess_container_t* s = sessions;
            bool correct = false;
            while(s!=NULL && !correct){
                if(s->sess == cur)
                    correct = true;
                else
                    s = s->next;
            }
            if(!correct)
                return LIBI_ESESS;
            us.push_back(s);

            if( ( (ProcessGroup*)cur )->isLaunched )
                return LIBI_ESESSALREADYLAUNCHED;
        }
    }

	// Fill in temp with the .hd .proc_path and .proc_argv then call process group launch
    for(iter = us.begin(); iter != us.end(); iter++){
        dist_req_t* temp = (dist_req_t*)malloc(nreq*sizeof(dist_req_t));
        int count = 0;
        for(i = 0; i<nreq; i++){
            if((*iter)->sess == distributions[i].sessionHandle){
                temp[count].hd = (host_t*)distributions[i].hd;
                temp[count].proc_path = distributions[i].proc_path;
                temp[count].proc_argv = distributions[i].proc_argv;
                count++;
            }
        }
        ProcessGroup* pg = (ProcessGroup*)(*iter)->sess;
		// Launch dist_req_t[count]
        rc = pg->launch(temp, count, (*iter)->env);
        if( rc != LIBI_OK )
            return rc;
    }

    return LIBI_OK;
}

libi_rc_e LIBI_fe_getMaxMsgBufLen(int *msgbufmax){
    (*msgbufmax) = LIBI_MAX_BUFFER_LENGTH;
    return LIBI_OK;
}

libi_rc_e LIBI_fe_sendUsrData(libi_sess_handle_t sessionHandle, void *msgbuf, int msgbuflen){
    ProcessGroup* pg = (ProcessGroup*)sessionHandle;

    return pg->sendUsrData(msgbuf, msgbuflen);
}

libi_rc_e LIBI_fe_recvUsrData(libi_sess_handle_t sessionHandle, void **msgbuf, int *msgbuflen){
    ProcessGroup* s = (ProcessGroup*)sessionHandle;

    return s->recvUsrData(msgbuf, msgbuflen);
}

libi_rc_e LIBI_fe_addToSessionEnvironment(libi_sess_handle_t sessionHandle, libi_env_t* envVars){
    if(envVars != NULL){
        for(libi_sess_container_t* cur = sessions; cur != NULL; cur=cur->next){
            if(cur->sess == sessionHandle){
                libi_env_t** e = &cur->env;
                while((*e) != NULL)
                    e = &(*e)->next;
                libi_env_t* newV = envVars;
                while(newV != NULL){
                    (*e) = (libi_env_t*)malloc(sizeof(libi_env_t));
                    (*e)->name = strdup(newV->name);
                    (*e)->value = strdup(newV->value);
                    (*e)->next = NULL;
                    e = &(*e)->next;
                    newV = newV->next;
                }
            }
        }
    }

    return LIBI_OK;
}


/*
 is this function needed?
 */
libi_rc_e LIBI_fe_getHostlistSize(libi_sess_handle_t sessionHandle, unsigned int *size){
    if(sessions == NULL)
        return LIBI_EINIT;

    if(!sessionHandle)
        return LIBI_ESESS;

    if(!sessionHandle->isLaunched)
        return LIBI_ELAUNCH;  //the session has not been launched
    
    (*size) = sessionHandle->host_list_size;

    return LIBI_OK;
}

libi_rc_e LIBI_fe_getHostlist(libi_sess_handle_t sessionHandle, char*** hostlist, unsigned int* size){
    if(sessions == NULL)
        return LIBI_EINIT;

    if(!sessionHandle)
        return LIBI_ESESS;
    
    if(!sessionHandle->isLaunched)
        return LIBI_ELAUNCH;  //the session has not been launched

    ProcessGroup* pg = sessionHandle;
    pg->getHostlist(hostlist, size);

    return LIBI_OK;
}

libi_rc_e LIBI_init(int ver, int *argc, char ***argv){
	
    libi_member = new ProcessGroupMember();

    return libi_member->init( argc, argv );
}

libi_rc_e LIBI_finalize(){
    return libi_member->finalize();
}

libi_rc_e LIBI_recvUsrData(void **msgbuf, int *msgbuflen){
    return libi_member->recvUsrData(msgbuf, msgbuflen);
}

libi_rc_e LIBI_sendUsrData(void *msgbuf, int *msgbuflen){
    return libi_member->sendUsrData(msgbuf, msgbuflen);
}

libi_rc_e LIBI_amIMaster(){
    return libi_member->amIMaster();
}

libi_rc_e LIBI_getMyRank(int *rank){
    return libi_member->getMyRank(rank);
}

libi_rc_e LIBI_getSize(int *size){
    return libi_member->getSize(size);
}

libi_rc_e LIBI_broadcast(void* buf,int numbyte){
    return libi_member->broadcast(buf, numbyte);
}

libi_rc_e LIBI_gather(void *sendbuf, int numbyte_per_elem, void* recvbuf){
    return libi_member->gather(sendbuf, numbyte_per_elem, recvbuf);
}

libi_rc_e LIBI_scatter(void *sendbuf, int numbyte_per_element, void* recvbuf){
    return libi_member->scatter(sendbuf, numbyte_per_element, recvbuf);
}

libi_rc_e LIBI_barrier(){
    return libi_member->barrier();
}







