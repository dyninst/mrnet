/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdlib.h>
#include <assert.h>

#include "mrnet_lightweight/Network.h"
#include "BackEndNode.h"
#include "ChildNode.h"
#include "FilterDefinitions.h"
#include "mrnet_lightweight/NetworkTopology.h"
#include "PeerNode.h"
#include "Protocol.h"
#include "mrnet_lightweight/Stream.h"
#include "mrnet_lightweight/Types.h"
#include "utils_lightweight.h"

#include "xplat_lightweight/NetUtils.h"

Port defaultTopoPort = 26500;

BackEndNode_t* new_BackEndNode_t(Network_t* inetwork, 
                                 char* imyhostname,
                                 Rank imyrank,
                                 char* iphostname,
                                 Port ipport,
                                 Rank iprank)
{
    BackEndNode_t* be;
    PeerNode_t* parent;

    be = (BackEndNode_t*)malloc(sizeof(BackEndNode_t));
    assert(be != NULL);
    be->network = inetwork;
    be->myhostname = imyhostname;
    be->myrank = imyrank;
    be->myport = UnknownPort;
    be->phostname = iphostname;
    be->pport = ipport;
    be->prank = iprank;
    be->incarnation = 0;

    parent = Network_new_PeerNode(inetwork, iphostname, ipport, iprank, true, true);
    be->network->parent = parent;

    be->network->local_hostname = imyhostname;
    be->network->local_rank = imyrank;
    be->network->local_back_end_node = be;

    be->network->network_topology = new_NetworkTopology_t(inetwork, imyhostname, UnknownPort, imyrank, true);

    NetworkTopology_t * nt = Network_get_NetworkTopology(be->network);

    // establish data connection with parent
    if (ChildNode_init_newChildDataConnection(be, be->network->parent, UnknownRank) == -1) {
        mrn_dbg (1, mrn_printf(FLF, stderr,
                    "init_newBEDataConnection() failed\n"));
        return NULL;
    }

    if (nt != NULL) {
        // if backend not alraedy in the topology--this is false in backend-attach cases  
        if ( ! NetworkTopology_isInTopology(nt, imyhostname, UnknownPort, imyrank) ) {
            Network_new_Stream(be->network, 1, NULL, 0, 0, 0, 0);
            mrn_dbg( 5, mrn_printf(FLF, stderr, "Backend not already in the topology\n") );

            // New topology propagation code - create a new update packet
            Stream_t* s = Network_get_Stream(be->network, 1); // get topol prop stream
            int type = TOPO_NEW_BE;
            char * host_arr = strdup(imyhostname);
            uint32_t * send_iprank = (uint32_t*)malloc(sizeof(uint32_t));
            *send_iprank = iprank;
            uint32_t * send_myrank = (uint32_t*)malloc(sizeof(uint32_t));
            *send_myrank = imyrank;
            uint16_t * send_port = (uint16_t*)malloc(sizeof(uint16_t));
            *send_port = UnknownPort;

            Stream_send(s, PROT_TOPO_UPDATE, "%ad %aud %aud %as %auhd",
                        &type, 1, send_iprank,
                        1, send_myrank,
                        1, &host_arr,
                        1, send_port, 1);
            free(host_arr);
        } else {
            mrn_dbg( 5, mrn_printf(FLF, stderr, "Backend already in the topology\n") );
        }

        if( ChildNode_send_SubTreeInitDoneReport(be) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "ChildNode_send_SubTreeInitDoneReport() failed\n") );
        }
    } 
    return be; 
}


BackEndNode_t* CreateBackEndNode ( Network_t* inetwork,
        char* imy_hostname,
        Rank imy_rank,
        char* iphostname,
        Port ipport,
        Rank iprank) 
{

    // create the new backend node
    BackEndNode_t* be = new_BackEndNode_t(inetwork, imy_hostname, imy_rank, iphostname, ipport, iprank); 

    return be;
}

int BackEndNode_proc_DeleteSubTree(BackEndNode_t* be, Packet_t* packet)
{
  int goaway = 0;
  char delete_backend;
  
  mrn_dbg_func_begin();

  Packet_unpack(packet, "%c", &delete_backend);
  if (delete_backend == 't') {
      mrn_dbg(3, mrn_printf(FLF, stderr, "Back-end will exit\n"));
      goaway = 1;
  }

  // processes will be exiting -- disable failure recovery
  Network_disable_FailureRecovery(be->network);

  // Send ack to parent
  if (!ChildNode_ack_DeleteSubTree(be)) {
    mrn_dbg(1, mrn_printf(FLF, stderr, "ChildNode_ack_DeleteSubTree() failed\n"));
  }
  
  // kill topology  
  Network_shutdown_Network(be->network);

  if (goaway) {
    mrn_dbg(5, mrn_printf(FLF, stderr, "Back-end exiting ... \n"));
    exit(0);
  }

  mrn_dbg_func_end();
  return 0;
}

int BackEndNode_proc_newStream(BackEndNode_t* be, Packet_t* packet)
{
    unsigned int num_backends;
    Rank *backends;
    int stream_id, tag;
    int ds_filter_id, us_filter_id, sync_id;
    char* us_filters;
    char* sync_filters;
    char* ds_filters;
    Rank me;

    mrn_dbg_func_begin();

    tag = packet->tag;

    if (tag == PROT_NEW_HETERO_STREAM) {
        me = be->network->local_rank;

        if (Packet_ExtractArgList(packet, "%d %ad %s %s %s",
            &stream_id, &backends, &num_backends, 
            &us_filters, &sync_filters, &ds_filters) == -1) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "Packet_ExtractArgList() failed\n"));
            return -1;
        }
  
        if (!Stream_find_FilterAssignment(us_filters, me, us_filter_id)) {
            mrn_dbg(3, mrn_printf(FLF, stderr, "Stream_find_FilterAssignment(upstream) failed, using default\n"));
            us_filter_id = TFILTER_NULL;
        }
    
        if (!Stream_find_FilterAssignment(ds_filters, me, ds_filter_id)) {
            mrn_dbg(3, mrn_printf(FLF, stderr, "Stream_find_FilterAssignment(downstream) failed, using default\n"));
            ds_filter_id = TFILTER_NULL;
        }
        if (!Stream_find_FilterAssignment(sync_filters, me, sync_id)) {
            mrn_dbg(3, mrn_printf(FLF, stderr, "Stream_find_FilterAssignment(sync) failed, using default\n"));
            sync_id = SFILTER_WAITFORALL;
        }
    }

    else if (tag == PROT_NEW_STREAM) {
        if (Packet_ExtractArgList(packet, "%d %ad %d %d %d", 
                                  &stream_id, &backends,&num_backends,
                                  &us_filter_id, &sync_id, &ds_filter_id) == -1) 
        {
            mrn_dbg(1, mrn_printf(FLF, stderr, "Packet_ExtractArgList() failed\n"));
            return -1;
        }
    }

    Network_new_Stream(be->network, stream_id, backends, num_backends,
                          us_filter_id, sync_id, ds_filter_id);

    mrn_dbg_func_end();

    return 0;
}

int BackEndNode_proc_UpstreamFilterParams(BackEndNode_t* be, 
                                          Packet_t* ipacket)
{
  int stream_id;
  Stream_t* strm;

  mrn_dbg_func_begin();

  stream_id = ipacket->stream_id;
  strm = Network_get_Stream(be->network, stream_id);
  if (strm == NULL) {
    mrn_dbg(1, mrn_printf(FLF, stderr, "stream %d lookup failed\n", stream_id));
    return -1;
  }
  Stream_set_FilterParams(strm, true, ipacket);
  mrn_dbg_func_end();
  return 0;
  
}

int BackEndNode_proc_DownstreamFilterParams(BackEndNode_t* be, Packet_t* ipacket)
{
  int stream_id;
  Stream_t* strm;

  mrn_dbg_func_begin();
  stream_id = ipacket->stream_id;

  strm = Network_get_Stream(be->network, stream_id);
  if (strm == NULL) {
    mrn_dbg(1, mrn_printf(FLF, stderr, "stream %d lookup failed\n", stream_id));
    return -1;
  }
  Stream_set_FilterParams(strm, false, ipacket);

  mrn_dbg_func_end();
  return 0;
  
}

int BackEndNode_proc_deleteStream(BackEndNode_t* be, Packet_t* ipacket)
{
    int stream_id;
    Stream_t * strm;
    
    mrn_dbg_func_begin();

    if (Packet_ExtractArgList(ipacket, "%d", &stream_id) == -1) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Packet_ExtractArgList() failed\n"));
        return -1;
    }
    
    strm = Network_get_Stream(be->network, stream_id);
    if (strm == NULL) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "stream %d lookup failed\n", stream_id));
        return -1;
    }

    strm->_was_shutdown = 1;

    mrn_dbg_func_end();
    return 0;
}

int BackEndNode_proc_newFilter(BackEndNode_t* be, Packet_t* ipacket)
{
  int retval = 0;
  unsigned short fid = 0;
  char* so_file = NULL, *func = NULL;

  mrn_dbg_func_begin();

  if (Packet_ExtractArgList(ipacket, "%uhd %s %s", &fid, &so_file, &func) == -1) {
    mrn_dbg(1, mrn_printf(FLF, stderr, "ExtractArgList() failed\n"));
    return -1;
  }

  mrn_dbg_func_end();
  return fid;
}

int BackEndNode_proc_FailureReportFromParent(BackEndNode_t* be, Packet_t* ipacket)
{
  Rank failed_rank;
  Packet_unpack(ipacket, "%uhd", &failed_rank);
  Network_remove_Node(be->network, failed_rank, false);

  return 0;
}

int BackEndNode_proc_NewParentReportFromParent(BackEndNode_t* be, Packet_t* ipacket)
{
  Rank child_rank, parent_rank;

  Packet_unpack(ipacket, "%ud %ud", &child_rank, &parent_rank);

  Network_change_Parent(be->network, child_rank, parent_rank);

  return 0;
}

int BackEndNode_proc_DataFromParent(BackEndNode_t* be, Packet_t* ipacket)
{
  int retval = 0;
  Stream_t* stream;
  Packet_t* opacket;
  vector_t * opackets = new_empty_vector_t();
  vector_t * opackets_reverse = new_empty_vector_t();
  int i;

  mrn_dbg_func_begin();

  stream = Network_get_Stream(be->network, (unsigned int)ipacket->stream_id);

  if (stream == NULL) {
    mrn_dbg(1, mrn_printf(FLF, stderr, "stream %d lookup failed\n", ipacket->stream_id));
    return -1;
  }

  if (Stream_push_Packet(stream, ipacket, opackets, opackets_reverse, false) == -1) {
    return -1;
  }

  for (i = 0; i < opackets->size; i++) {
      opacket = opackets->vec[i];
      mrn_dbg(5, mrn_printf(FLF, stderr, "adding Packet on stream[%d]\n", stream->id));
      Stream_add_IncomingPacket(stream, opacket);
  }
  
  mrn_dbg_func_end();

  return retval;

}
