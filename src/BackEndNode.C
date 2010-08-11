/****************************************************************************
 *  Copyright 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/


#include "BackEndNode.h"
#include "EventDetector.h"
#include "Filter.h"
#include "PeerNode.h"
#include "Router.h"
#include "utils.h"
#include "mrnet/MRNet.h"

namespace MRN
{

/*=====================================================*/
/*  BackEndNode CLASS METHOD DEFINITIONS            */
/*=====================================================*/

BackEndNode::BackEndNode( Network * inetwork, 
                          std::string imyhostname, Rank imyrank,
                          std::string iphostname, Port ipport, Rank iprank )
    : CommunicationNode( imyhostname, UnknownPort, imyrank ),
      ChildNode( inetwork, imyhostname, imyrank, iphostname, ipport, iprank )
{
    _network->set_LocalHostName( _hostname  );
    _network->set_LocalRank( _rank );
    _network->set_BackEndNode( this );
    _network->set_NetworkTopology( new NetworkTopology( inetwork, _hostname, _port, _rank, true ) );
    
    NetworkTopology* nt=_network->get_NetworkTopology();

    //establish data connection w/ parent
    if( init_newChildDataConnection( _network->get_ParentNode() ) == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                               "init_newChildDataConnection() failed\n" ));
        return;
    }

    //start event detection thread
    if( ! EventDetector::start( _network ) ) {
      mrn_dbg( 1, mrn_printf(FLF, stderr, "start_EventDetector() failed\n" ));
      error( ERR_INTERNAL, _rank, "start_EventDetector failed\n" );
      return;
    }
    
    if( nt != NULL )
    {
      if( ! nt->isInTopology(imyhostname, _port, imyrank) ) {
          //if backend already not in the topology. this is false in back end attach cases

          _network->new_Stream(1, NULL, 0, TFILTER_TOPO_UPDATE, SFILTER_TIMEOUT, TFILTER_TOPO_UPDATE_DOWNSTREAM );
          mrn_dbg( 5, mrn_printf(FLF, stderr, "Backend not already in the topology\n") );

          //new topo propagation code - create a new update packet
          Stream *s = _network->get_Stream(1); // getting handle for topology propagation stream
          int type = TOPO_NEW_BE;
          char *host_arr = strdup(imyhostname.c_str());
          uint32_t* send_iprank = (uint32_t*) malloc(sizeof(uint32_t));
          *send_iprank = iprank;
          uint32_t* send_myrank = (uint32_t*) malloc(sizeof(uint32_t));
          *send_myrank = imyrank;
          uint16_t* send_port = (uint16_t*)malloc(sizeof(uint16_t));
          *send_port = _port;

          s->send( PROT_TOPO_UPDATE, "%ad %aud %aud %as %auhd", 
                   &type, 1, send_iprank, 1, send_myrank, 1, &host_arr,1, send_port, 1);
          free(host_arr);
      }	
      else
          mrn_dbg( 5, mrn_printf(FLF, stderr, "Backend already in the topology\n") );

    }  

    if( send_SubTreeInitDoneReport() == -1 ) {
           mrn_dbg( 1, mrn_printf(FLF, stderr, "send_SubTreeInitDoneReport() failed\n") );
    }

}

BackEndNode::~BackEndNode(void)
{
}

int BackEndNode::proc_DataFromParent(PacketPtr ipacket) const
{
    int retval = 0;
    Stream * stream = _network->get_Stream( ipacket->get_StreamId() );
    if( stream == NULL ){
        mrn_dbg( 1, mrn_printf(FLF, stderr, "stream %d lookup failed\n",
                               ipacket->get_StreamId() ));
        return -1;
    }

    std::vector<PacketPtr> opackets, opackets_reverse;
    stream->push_Packet( ipacket, opackets, opackets_reverse, false );

    if( ! opackets_reverse.empty() ) {
        if( ChildNode::_network->send_PacketsToParent( opackets_reverse ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "parent.send() failed()\n" ));
            retval = -1;
        }
    }
    
    std::vector<PacketPtr>::iterator pi = opackets.begin();
    for( ; pi != opackets.end() ; pi ++ )
        stream->add_IncomingPacket(*pi);

    return retval;
}

int BackEndNode::proc_newStream( PacketPtr ipacket ) const
{
    unsigned int num_backends;
    Rank *backends;
    int stream_id, tag;
    int ds_filter_id, us_filter_id, sync_id;

    mrn_dbg_func_begin();

    tag = ipacket->get_Tag();

    if( tag == PROT_NEW_HETERO_STREAM ) {

        char* us_filters;
        char* sync_filters;
        char* ds_filters;
        Rank me = _network->get_LocalRank();

        if( ipacket->ExtractArgList( "%d %ad %s %s %s", 
                                     &stream_id, &backends, &num_backends, 
                                     &us_filters, &sync_filters, &ds_filters ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "ExtractArgList() failed\n" ));
            return -1;
        }
        
        if( ! Stream::find_FilterAssignment(std::string(us_filters), me, us_filter_id) ) {
            mrn_dbg( 3, mrn_printf(FLF, stderr, 
                                   "Stream::find_FilterAssignment(upstream) failed, using default\n" ));
            us_filter_id = TFILTER_NULL;
        }
        if( ! Stream::find_FilterAssignment(std::string(ds_filters), me, ds_filter_id) ) {
            mrn_dbg( 3, mrn_printf(FLF, stderr, 
                                   "Stream::find_FilterAssignment(downstream) failed, using default\n" ));
            ds_filter_id = TFILTER_NULL;
        }
        if( ! Stream::find_FilterAssignment(std::string(sync_filters), me, sync_id) ) {
            mrn_dbg( 3, mrn_printf(FLF, stderr, 
                                   "Stream::find_FilterAssignment(sync) failed, using default\n" ));
            sync_id = SFILTER_WAITFORALL;
        }

    } else if( tag == PROT_NEW_STREAM ) {

        if( ipacket->ExtractArgList( "%d %ad %d %d %d", 
                                     &stream_id, &backends, &num_backends, 
                                     &us_filter_id, &sync_id, &ds_filter_id ) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "ExtractArgList() failed\n" ));
            return -1;
        }
    }
    _network->new_Stream( stream_id, backends, num_backends, 
                          us_filter_id, sync_id, ds_filter_id );

    mrn_dbg_func_end();
    return 0;
}

int BackEndNode::proc_FilterParams( FilterType ftype, PacketPtr &ipacket ) const
{
    int stream_id;

    mrn_dbg_func_begin();

    stream_id = ipacket->get_StreamId();
    Stream* strm = _network->get_Stream( stream_id );
    if( strm == NULL ){
        mrn_dbg( 1, mrn_printf(FLF, stderr, "stream %d lookup failed\n",
                               stream_id ));
        return -1;
    }

    strm->set_FilterParams( ftype, ipacket );

    mrn_dbg_func_end();
    return 0;
}

int BackEndNode::proc_deleteStream(PacketPtr ipacket) const
{
    int stream_id;
    Stream * strm;

    mrn_dbg_func_begin();

    if (ipacket->ExtractArgList("%d", &stream_id) == -1) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "ExtractArgList() failed\n" ));
        return -1;
    } 

    strm = _network->get_Stream(stream_id);
    if (strm == NULL) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "stream %d lookup failed\n", 
                    stream_id));
        return -1;
    } 

    strm->_was_shutdown = true;

    mrn_dbg_func_end();

    return 0;
}


int BackEndNode::proc_DeleteSubTree( PacketPtr ipacket ) const
{
    mrn_dbg_func_begin();

    bool goaway = false;
    char delete_backend;
    ipacket->unpack( "%c", &delete_backend );
    if( delete_backend == 't' ) {
        mrn_dbg( 5, mrn_printf(FLF, stderr, "Backend will exit\n" ));
        goaway = true;
    }

    // processes will be exiting -- disable failure recovery
    _network->disable_FailureRecovery();

    // kill threads, topology, and events
    _network->shutdown_Network();

    if( goaway ) {
        mrn_dbg(3, mrn_printf(FLF, stderr, "Backend exiting!\n"));
        exit(0);
    }
   
    // exit recv thread from parent
    mrn_dbg(5, mrn_printf(FLF, stderr, "I'm going away now!\n"));
    XPlat::Thread::Exit(NULL);

    return 0;
}

int BackEndNode::proc_FailureReportFromParent( PacketPtr ipacket ) const
{
    Rank failed_rank;

    ipacket->unpack( "%uhd", &failed_rank ); 

    _network->remove_Node( failed_rank );

    return 0;
}

int BackEndNode::proc_NewParentReportFromParent( PacketPtr ipacket ) const
{
    Rank child_rank, parent_rank;

    ipacket->unpack( "%ud &ud", &child_rank, &parent_rank ); 

    _network->change_Parent( child_rank, parent_rank );

    return 0;
}

int BackEndNode::proc_newFilter( PacketPtr ipacket ) const
{
    int retval = 0;
    unsigned short fid = 0;
    const char *so_file = NULL, *func = NULL;

    mrn_dbg_func_begin();

    if( ipacket->ExtractArgList( "%uhd %s %s", &fid, &so_file, &func ) == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "ExtractArgList() failed\n" ));
        return -1;
    }

    retval = Filter::load_FilterFunc( fid, so_file, func );
    if( retval == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                    "Filter::load_FilterFunc() failed.\n" ));
    }

    mrn_dbg_func_end();
    return retval;
}


} // namespace MRN
