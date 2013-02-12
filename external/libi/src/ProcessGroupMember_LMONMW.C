/****************************************************************************
 *  Copyright © 2011 The Regents of the University of New Mexico            *
 *  All Rights Reserved                                                     *
 *  Created by Josh Goehner and Dorian Arnold                               *
 *  Department of Computer Science                                          *
 *  Detailed usage rights in "LICENSE" file.                                *
 ****************************************************************************/

#include <cassert>
#include "libi/ProcessGroupMember.h"
#include "libi/debug.h"
#include "lmon_api/common.h"
#include "lmon_api/lmon_api_std.h"
#include "lmon_api/lmon_mw.h"

using namespace LIBI;
using namespace XPlat;

typedef struct _PackBuf {
    int len;
    void* buf;
} PackBuf;

int packmw(void *data, void *buf, int buf_max, int *buf_len);
int unpackmw(void *buf, int buf_len, void *data);
static debug dbg(false);

ProcessGroupMember::ProcessGroupMember(){
    isInit=false;
}

libi_rc_e ProcessGroupMember::init(int *argc, char ***argv ){
    dbg.getParametersFromEnvironment();


	/*dbg.print(ERRORS, FL, "PGM_LMONMW::init : argc = %d\n", *argc);
	for (int i = 0; i < *argc; i++){
		if(*argv[i] != NULL)
		{
		dbg.print(ERRORS, FL, "PGM_LMONMW::init : argv[%d] = %s\n", i, *argv[i]);
		}
		else{
			dbg.print(ERRORS, FL, "PGM_LMONMW::init : argv[%d] = NULL\n", i);
		}
	}
	*/
    dbg.print(ERRORS, FL, "PGM_LMONMW::init : About to LMON_mw_init");
    lmon_rc_e rc = LMON_mw_init(LMON_VERSION, argc, argv);
    if (rc != LMON_OK){
        dbg.print(ERRORS, FL, "PGM_LMONMW::init Could not initialize LMON MW");
        return LIBI_EINIT;
    }

    rc = LMON_mw_regPackForMwToFe(packmw);
    if (rc != LMON_OK){
        dbg.print(ERRORS, FL, "Could not register packmw");
        return LIBI_EINIT;
    }

    rc = LMON_mw_regUnpackForFeToMw(unpackmw);
    if (rc != LMON_OK){
        dbg.print(ERRORS, FL, "Could not register unpackmw");
        return LIBI_EINIT;
    }

    rc = LMON_mw_handshake(NULL);
    if (rc != LMON_OK){
        dbg.print(ERRORS, FL, "LMON_mw_handshake() Failed");
        return LIBI_EINIT;
    }

    rc = LMON_mw_ready(NULL);
    if (rc != LMON_OK){
        dbg.print(ERRORS, FL, "LMON_mw_ready() failed");
        return LIBI_EINIT;
    }
    char id[15];
    int r;
    rc = LMON_mw_getMyRank(&r);
    sprintf(id,"%i",r);
    dbg.set_identifier(id);
    dbg.getParametersFromEnvironment();

    isInit = true;
    return LIBI_OK;
}

libi_rc_e ProcessGroupMember::finalize(){
    if( !isInit )
        return LIBI_EINIT;

    lmon_rc_e rc = LMON_mw_finalize();
    if (rc != LMON_OK){
        dbg.print(ERRORS, FL, "Could not finalize");
        return LIBI_NO;
    }

    return LIBI_OK;
}

libi_rc_e ProcessGroupMember::recvUsrData(void** msgbuf, int* msgbuflen){
    if( !isInit )
        return LIBI_EINIT;

    PackBuf* pb = new PackBuf();
    lmon_rc_e rc;

    rc = LMON_mw_recvUsrData((void*)pb);
    if (rc != LMON_OK){
        dbg.print(ERRORS, FL, "Failed to receive user data");
        msgbuf = NULL;
        msgbuflen = 0;
        return LIBI_NO;
    }
    else{
        (*msgbuf) = malloc(pb->len);
        memcpy((*msgbuf), pb->buf, pb->len);
        //msgbuf = (void*)strndup((char*)pb->buf, pb->len);
        *msgbuflen = pb->len;
    }

    return LIBI_OK;
}

libi_rc_e ProcessGroupMember::sendUsrData(void *msgbuf, int *msgbuflen){
    if( !isInit )
        return LIBI_EINIT;

    PackBuf* pb = new PackBuf();
    
    if( LMON_mw_amIMaster() == LMON_YES ){
        pb->len = *msgbuflen;
        pb->buf = malloc(*msgbuflen);
        memcpy(pb->buf, msgbuf, *msgbuflen);
    }
    
    lmon_rc_e rc = LMON_mw_sendUsrData((void*)pb);
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
        delete pb;
        dbg.print(ERRORS, FL, "PGM: Failed to send user data");
        return LIBI_NO;
    }

    delete pb;
    return LIBI_OK;
}

libi_rc_e ProcessGroupMember::amIMaster(){
    if( !isInit )
        return LIBI_EINIT;
    
    lmon_rc_e rc = LMON_mw_amIMaster();
    //lmon_rc_e LMON_mw_amIMaster ();
    if(rc == LMON_YES)
        return LIBI_YES;
    else
        return LIBI_NO;
}

libi_rc_e ProcessGroupMember::getMyRank(int* rank){
    if( !isInit )
        return LIBI_EINIT;
    
    lmon_rc_e rc = LMON_mw_getMyRank(rank);
    //lmon_rc_e LMON_mw_getMyRank(int *rank);
    if (rc != LMON_OK){
        dbg.print(ERRORS, FL, "Failed to get rank");
        return LIBI_NO;
    }

    return LIBI_OK;
}

libi_rc_e ProcessGroupMember::getSize(int* size){
    if( !isInit )
        return LIBI_EINIT;
    
    lmon_rc_e rc = LMON_mw_getSize(size);
    if (rc != LMON_OK){
        dbg.print(ERRORS, FL, "Failed to get size");
        return LIBI_NO;
    }

    return LIBI_OK;
}

libi_rc_e ProcessGroupMember::broadcast(void* buf,int numbyte){
    if( !isInit )
        return LIBI_EINIT;
    
    lmon_rc_e rc = LMON_mw_broadcast(buf, numbyte);
    if (rc != LMON_OK){
        dbg.print(ERRORS, FL, "Problems with broadcast");
        return LIBI_NO;
    }

    return LIBI_OK;
}

libi_rc_e ProcessGroupMember::gather(void *sendbuf, int numbyte_per_elem, void* recvbuf){
    if( !isInit )
        return LIBI_EINIT;
    
    lmon_rc_e rc = LMON_mw_gather(sendbuf, numbyte_per_elem, recvbuf);
    if (rc != LMON_OK){
        dbg.print(ERRORS, FL, "Problems with gather");
        return LIBI_NO;
    }
    
    return LIBI_OK;
}

libi_rc_e ProcessGroupMember::scatter(void *sendbuf, int numbyte_per_element, void* recvbuf){
    if( !isInit )
        return LIBI_EINIT;

    lmon_rc_e rc = LMON_mw_scatter(sendbuf, numbyte_per_element, recvbuf);
    if (rc != LMON_OK){
        dbg.print(ERRORS, FL, "Problems with scatter");
        return LIBI_NO;
    }
    
    return LIBI_OK;
}

libi_rc_e ProcessGroupMember::barrier(){
    if( !isInit )
        return LIBI_EINIT;

    lmon_rc_e rc = LMON_mw_barrier();
    if (rc != LMON_OK){
        dbg.print(ERRORS, FL, "Barrier failed");
        return LIBI_NO;
    }
    
    return LIBI_OK;
}

/*
 LaunchMon call back functions.
 */
int packmw(void *data, void *buf, int buf_max, int *buf_len){
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

int unpackmw(void *buf, int buf_len, void *data){
    ((PackBuf*)data)->buf = malloc(buf_len);
    memcpy(((PackBuf*)data)->buf, buf, buf_len);
    ((PackBuf*)data)->len=buf_len;
    return 0;
}

