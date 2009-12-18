/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdlib.h>
#include <assert.h>

#include "mrnet/Network.h"
#include "BackEndNode.h"
#include "ChildNode.h"
#include "FilterDefinitions.h"
#include "mrnet/NetworkTopology.h"
#include "PeerNode.h"
#include "Protocol.h"
#include "mrnet/Stream.h"
#include "mrnet/Types.h"
#include "Utils.h"

#include "xplat/NetUtils.h"

Port defaultTopoPort = 26500;

BackEndNode_t* new_BackEndNode_t(Network_t* inetwork, 
                              char* imyhostname,
                              Rank imyrank,
                              char* iphostname,
                              Port ipport,
                              Rank iprank)
{
  BackEndNode_t* be = (BackEndNode_t*)malloc(sizeof(BackEndNode_t));
  assert(be != NULL);
  be->network = inetwork;
  be->myhostname = imyhostname;
  be->myrank = imyrank;
  be->myport = UnknownPort;
  be->phostname = iphostname;
  be->pport = ipport;
  be->prank = iprank;
  be->incarnation = 0;

  PeerNode_t* parent = Network_new_PeerNode(inetwork, iphostname, ipport, iprank, true, true);
  be->network->parent = parent;

  be->network->local_hostname = imyhostname;
  be->network->local_rank = imyrank;
  be->network->local_back_end_node = be;
  
  be->network->network_topology = new_NetworkTopology_t(inetwork, imyhostname, UnknownPort, imyrank, true);

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

  mrn_dbg(1, mrn_printf(FLF, stderr, "CreateBackEndNode about to call ChildNode_init_newChildDataConnection\n"));
  // establish data connection with parent
  mrn_dbg(5, mrn_printf(FLF, stderr, "about to connect to parent, %s\n", be->network->parent->hostname));
  if (ChildNode_init_newChildDataConnection(be, be->network->parent, UnknownRank) == -1) {
    mrn_dbg (1, mrn_printf(FLF, stderr,
                            "init_newBEDataConnection() failed\n"));
    return NULL;
  }

  // send new subtree report
  mrn_dbg( 5, mrn_printf(FLF, stderr, "Sending new child report.\n"));
  if (ChildNode_send_NewSubTreeReport(be) == -1) {
    mrn_dbg(1, mrn_printf(FLF, stderr,
                          "ChildNode_send_newSubTreeReport() failed\n"));
  }
  mrn_dbg( 5, mrn_printf(FLF, stderr,
                        "ChildNode_send_newSubTreeReport() succeeded!\n"));

  return be;

}

int BackEndNode_proc_DeleteSubTree(BackEndNode_t* be, Packet_t* packet)
{
  mrn_dbg_func_begin();

  int goaway = 0;
  char delete_backend;
  
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
    mrn_dbg(1, mrn_printf(FLF, stderr, "Back-end exiting ... \n"));
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

  mrn_dbg_func_begin();

  tag = packet->tag;

  if (tag == PROT_NEW_HETERO_STREAM) {
    char* us_filters;
    char* sync_filters;
    char* ds_filters;
    Rank me = be->network->local_rank;

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

  mrn_dbg_func_begin();

  stream_id = ipacket->stream_id;
  Stream_t* strm = Network_get_Stream(be->network, stream_id);
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

  mrn_dbg_func_begin();
  stream_id = ipacket->stream_id;

  Stream_t* strm = Network_get_Stream(be->network, stream_id);
  if (strm == NULL) {
    mrn_dbg(1, mrn_printf(FLF, stderr, "stream %d lookup failed\n", stream_id));
    return -1;
  }
  Stream_set_FilterParams(strm, false, ipacket);

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
  
}

int BackEndNode_proc_NewParentReportFromParent(BackEndNode_t* be, Packet_t* ipacket)
{
  Rank child_rank, parent_rank;

  Packet_unpack(ipacket, "%ud %ud", &child_rank, &parent_rank);

  Network_change_Parent(be->network, child_rank, parent_rank);
}

int BackEndNode_proc_DataFromParent(BackEndNode_t* be, Packet_t* ipacket)
{
  mrn_dbg_func_begin();

  int retval = 0;

  Stream_t* stream = Network_get_Stream(be->network, (unsigned int)ipacket->stream_id);

  if (stream == NULL) {
    mrn_dbg(1, mrn_printf(FLF, stderr, "stream %d lookup failed\n", ipacket->stream_id));
    return -1;
  }

  Packet_t* opacket = (Packet_t*)malloc(sizeof(Packet_t));
  assert(opacket != NULL);
  Stream_push_Packet(stream, ipacket, opacket, false);
 
  mrn_dbg(5, mrn_printf(FLF, stderr, "adding Packet on stream[%d]\n", stream->id));
  Stream_add_IncomingPacket(stream, opacket);
  mrn_dbg(5, mrn_printf(FLF, stderr, "opacket's stream_id=%d\n", opacket->stream_id));

  mrn_dbg_func_end();

  return retval;

}
