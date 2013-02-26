/****************************************************************************
 *  Copyright © 2011 The Regents of the University of New Mexico            *
 *  All Rights Reserved                                                     *
 *  Created by Josh Goehner and Dorian Arnold                               *
 *  Department of Computer Science                                          *
 *  Detailed usage rights in "LICENSE" file.                                *
 ****************************************************************************/

#include <cassert>
#include <vector>
#include <sys/time.h>
#include "libi/ProcessGroup.h"
#include "lmon_api/common.h"
#include "lmon_api/lmon_api_std.h"
#include "lmon_api/lmon_fe.h"
#include <stdio.h>
#include <unistd.h>

using namespace LIBI;
using namespace XPlat;

typedef struct _PackBuf {
    int len;
    void* buf;
} PackBuf;

int ProcessGroup::fe_count = 0;

int xplat_copy_pack(void *data, void *buf, int buf_max, int *buf_len);
int xplat_copy_unpack(void *buf, int buf_len, void *data);
void convertLIBItoLMON(dist_req_t libi_req[], int libi_nreq, dist_request_t lmon_req[]);
lmon_daemon_env_t* createLMONEnv(env_t* env, int* nEnv);

ProcessGroup::ProcessGroup(){
    dbg = new debug(true);
    dbg->getParametersFromEnvironment();
    char id[15];
    sprintf(id, "FE%i", fe_count);
    dbg->set_identifier(id);

    lmon_rc_e rc;
    isLaunched = false;
    isFinalized = false;
    //serial_size = 0;
    //serial = NULL;

    if(fe_count == 0){
        rc = LMON_fe_init(LMON_VERSION);
        if (rc != LMON_OK)
            dbg->print(ERRORS, FL, "Failed to initialize Launchmon\n");
    }
    fe_count++;

    rc = LMON_fe_createSession(&lmon_session);
    if (rc != LMON_OK)
        dbg->print(ERRORS, FL, "Failed to create session.\n");

    rc = LMON_fe_regPackForFeToMw(lmon_session, xplat_copy_pack);
    if (rc != LMON_OK)
        dbg->print(ERRORS, FL, "Failed to register pack function.\n");

    rc = LMON_fe_regUnpackForMwToFe(lmon_session, xplat_copy_unpack);
    if (rc != LMON_OK)
        dbg->print(ERRORS, FL, "Failed to register unpack function.\n");

}


libi_rc_e ProcessGroup::launch(dist_req_t distributions[], int nreq, env_t* env){
    dbg->print(ERRORS, FL, "In the ProcessGroup_LMONMW->launch method\n");
	lmon_rc_e rc;
    int nEnv;
    lmon_daemon_env_t* lmonEnv;
    dist_request_t lmon_dist[nreq];

	//debug to print out env
	/*env_t* tempe = env;
	fprintf(stderr, "About to print env:\n");
    while ((*tempe).next != NULL)
	{
        fprintf(stderr,"%s = %s\n", (*tempe).name, (*tempe).value);
		tempe=(*tempe).next;
	}*/

    lmonEnv = createLMONEnv(env, &nEnv);
    convertLIBItoLMON(distributions, nreq, lmon_dist);

    if(nEnv>0){
        rc = LMON_fe_putToMwDaemonEnv(lmon_session, lmonEnv, nEnv);
        if (rc != LMON_OK)
            dbg->print(ERRORS, FL, "Failed to set environment.\n");
    }

	
    dbg->print(ERRORS, FL, "Next Line: LMON_fe_launchMwDaemons.\n");
    rc = LMON_fe_launchMwDaemons(lmon_session, lmon_dist, nreq, NULL, NULL);
    if (rc != LMON_OK){
        dbg->print(ERRORS, FL, "Failed to LMON_fe_launchMwDaeons.\n");
        return LIBI_ELAUNCH;
    }
    isLaunched = true;

    return LIBI_OK;
}

libi_rc_e ProcessGroup::sendUsrData(void *msgbuf, int msgbuflen){
    PackBuf* pb = new PackBuf();
    pb->len = msgbuflen;
    pb->buf = malloc(msgbuflen);
    memcpy(pb->buf, msgbuf, msgbuflen);

    lmon_rc_e rc = LMON_fe_sendUsrDataMw(lmon_session, (void*)pb);
    if (rc != LMON_OK){
		switch (rc){
			case LMON_EBDARG:
				printf("Invalid args\n");
				break;
			case LMON_ENOPLD:
				printf("Message contains no user payload\n");
				break;
			case LMON_ENCLLB:
				printf("No pack or unpack function registered\n");
				break;
			case LMON_ENEGCB:
				printf("Pack or unpack function returned negative value\n");
				break;
			case LMON_ENOMEM:
				printf("Out of Memory\n");
				break;
			default:
				printf("Other Error\n");
		}
        dbg->print(ERRORS, FL, "PG: Failed to send user data.\n");
        return LIBI_NO;
    }

    //delete pb;adsfasdfasdf
    return LIBI_OK;
}

//caller responsible for freeing msgbuf and msgbuflen
libi_rc_e ProcessGroup::recvUsrData(void** msgbuf, int* msgbuflen){
    PackBuf* pb = new PackBuf();

    lmon_rc_e rc = LMON_fe_recvUsrDataMw(lmon_session, (void*)pb);
    if (rc != LMON_OK){
        *msgbuf = NULL;
        *msgbuflen = 0;
        dbg->print(ERRORS, FL, "FE Failed to receive user data.\n");
        return LIBI_NO;
    }
    else{
        *msgbuf = pb->buf;
        *msgbuflen = pb->len;
    }
    return LIBI_OK;
}

libi_rc_e ProcessGroup::getHostlist(char*** hostlist, unsigned int* size){
    lmon_rc_e rc;
    rc = LMON_fe_getMwHostlistSize(lmon_session, size);
    if (rc != LMON_OK){
        dbg->print(ERRORS, FL, "Could not retrieve host list size.\n");
        return LIBI_NO;
    }

    *hostlist = (char**)malloc( sizeof(char*) * (*size) );

    rc = LMON_fe_getMwHostlist(lmon_session, (*hostlist), size, *size);

    if (rc != LMON_OK){
        dbg->print(ERRORS, FL, "Could not retrieve host list.\n");
        return LIBI_NO;
    }

    return LIBI_OK;
}


libi_rc_e ProcessGroup::finalize(){
    libi_rc_e ret = LIBI_OK;
    
    isFinalized=true;

    return ret;
}

/*
 LaunchMon call back functions.
 */
int xplat_copy_pack(void *data, void *buf, int buf_max, int *buf_len){
    PackBuf *pb = (PackBuf *)data;
    int length = 0;
    if(pb->len < buf_max)
        length = pb->len;
    else
        length = buf_max;
    //If the buffer is too large it will be truncated

    memcpy(buf, pb->buf, length);

    *buf_len = length;
    return 0;
}

int xplat_copy_unpack(void *buf, int buf_len, void *data){
    ((PackBuf*)data)->buf = malloc(buf_len);
    memcpy(((PackBuf*)data)->buf, buf, buf_len);
    ((PackBuf*)data)->len=buf_len;
    return 0;
}

void convertLIBItoLMON(dist_req_t libi_req[], int libi_nreq, dist_request_t lmon_req[]){
    int j;
    for(int i = 0; i<libi_nreq; i++){
        std::vector<char*> lmon_hls;
        host_t* cur = libi_req[i].hd;
        while(cur != NULL){
            for(j = 0; j < cur->nproc; j++){
                lmon_hls.push_back(cur->hostname);
            }
            cur = cur->next;
        }
        
        if(lmon_hls.size()>0){
            lmon_req[i].md = LMON_MW_HOSTLIST;
            lmon_req[i].mw_daemon_path = strdup(libi_req[i].proc_path);

            //copy arguments
            int count = 0;
            if(libi_req[i].proc_argv != NULL){
                while(libi_req[i].proc_argv[count] != NULL){
                    count++;
                }
                lmon_req[i].d_argv = (char **)malloc((count+1)*sizeof(char *));
                int c = 0;
                while(libi_req[i].proc_argv[c] != NULL){
                    lmon_req[i].d_argv[c] = strdup(libi_req[i].proc_argv[c]);
                    c++;
                }
                lmon_req[i].d_argv[c] = NULL;
            }
            else
                lmon_req[i].d_argv = NULL;

            lmon_req[i].ndaemon = -1;
            lmon_req[i].block = -1;
            lmon_req[i].cyclic = -1;
            lmon_req[i].optkind = hostlists;

            //create char** from std::vector<char*>
            lmon_req[i].option.hl = (char**)malloc( (lmon_hls.size()+1) * sizeof(char*));
            for(int k = 0; k<lmon_hls.size(); k++){
                lmon_req[i].option.hl[k] = strdup(lmon_hls[k]);
            }
            lmon_req[i].option.hl[lmon_hls.size()] = NULL;
        }
    }
}

lmon_daemon_env_t* createLMONEnv(env_t* env, int* nEnv){
    lmon_daemon_env_t* l = NULL;
    env_t* e = env;
    int count = 0;
    while(e != NULL){
        count ++;
        e = e->next;
    }

    if(count > 0){
        l = (lmon_daemon_env_t*)malloc(count * sizeof(lmon_daemon_env_t));
        e = env;
        for(int i = 0; i < count && e != NULL; i++){
            l[i].envName = e->name;
            l[i].envValue = e->value;
            l[i].next = NULL;
            e = e->next;
        }
    }

    (*nEnv) = count;
    return l;
}
