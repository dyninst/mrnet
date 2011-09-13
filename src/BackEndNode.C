/****************************************************************************
 *  Copyright 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
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
    _network->set_NetworkTopology( new NetworkTopology(inetwork, _hostname, 
                                                       _port, _rank, true) );
    
    NetworkTopology* nt = _network->get_NetworkTopology();

    //establish data connection w/ parent
    if( init_newChildDataConnection( _network->get_ParentNode() ) == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                               "init_newChildDataConnection() failed\n" ));
        return;
    }

    //start event detection thread
    if( ! EventDetector::start( _network ) ) {
        error( ERR_INTERNAL, _rank, "start_EventDetector() failed" );
        return;
    }
    
    if( nt != NULL ) {
    
      if( ! nt->in_Topology(imyhostname, _port, imyrank) ) {
          // backend attach case
          mrn_dbg( 5, mrn_printf(FLF, stderr, 
                                 "Backend not already in the topology\n") );

          //new be - send topology update packet
          Stream* s = _network->new_Stream( TOPOL_STRM_ID, NULL, 0, 
	                                    TFILTER_TOPO_UPDATE, 
                                            SFILTER_TIMEOUT, 
                                            TFILTER_TOPO_UPDATE_DOWNSTREAM );
          assert( s != NULL );
          int type = NetworkTopology::TOPO_NEW_BE;
          char *host_arr = strdup( imyhostname.c_str() );
          uint32_t send_iprank = iprank;
          uint32_t send_myrank = imyrank;
          uint16_t send_port = _port;

          s->send( PROT_TOPO_UPDATE, "%ad %aud %aud %as %auhd", 
                   &type, 1, &send_iprank, 1, &send_myrank, 1, 
                   &host_arr, 1, &send_port, 1 );

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
        mrn_dbg( 1, mrn_printf(FLF, stderr, "stream %d lookup failed for pkt with tag %d\n",
                               ipacket->get_StreamId(), ipacket->get_Tag() ));
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
    unsigned int num_backends, stream_id;
    Rank *backends;
    int tag, ds_filter_id, us_filter_id, sync_id;

    mrn_dbg_func_begin();

    tag = ipacket->get_Tag();

    if( tag == PROT_NEW_HETERO_STREAM ) {

        char *us_filters = NULL;
        char *sync_filters = NULL;
        char *ds_filters = NULL;
        Rank me = _network->get_LocalRank();

        if( ipacket->unpack("%ud %ad %s %s %s", 
                            &stream_id, &backends, &num_backends, 
                            &us_filters, &sync_filters, &ds_filters) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "unpack() failed\n") );
            return -1;
        }
        
        if( ! Stream::find_FilterAssignment(std::string(us_filters), me, us_filter_id) ) {
            mrn_dbg( 3, mrn_printf(FLF, stderr, 
                                   "Stream::find_FilterAssignment(upstream) failed\n") );
            us_filter_id = TFILTER_NULL;
        }
        if( ! Stream::find_FilterAssignment(std::string(ds_filters), me, ds_filter_id) ) {
            mrn_dbg( 3, mrn_printf(FLF, stderr, 
                                   "Stream::find_FilterAssignment(downstream) failed\n") );
            ds_filter_id = TFILTER_NULL;
        }
        if( ! Stream::find_FilterAssignment(std::string(sync_filters), me, sync_id) ) {
            mrn_dbg( 3, mrn_printf(FLF, stderr, 
                                   "Stream::find_FilterAssignment(sync) failed\n") );
            sync_id = SFILTER_WAITFORALL;
        }

        if( us_filters != NULL )
            free( us_filters );

        if( sync_filters != NULL )
            free( sync_filters );

        if( ds_filters != NULL )
            free( ds_filters );
    } 
    else if( tag == PROT_NEW_STREAM ) {

        if( ipacket->unpack("%ud %ad %d %d %d", 
                            &stream_id, &backends, &num_backends, 
                            &us_filter_id, &sync_id, &ds_filter_id) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "unpack() failed\n" ));
            return -1;
        }
    }

    _network->new_Stream( stream_id, backends, num_backends, 
                          us_filter_id, sync_id, ds_filter_id );

    if( backends != NULL )
        free( backends );

    mrn_dbg_func_end();
    return 0;
}

int BackEndNode::proc_FilterParams( FilterType ftype, PacketPtr &ipacket ) const
{
    unsigned int stream_id;
    Stream* strm;

    mrn_dbg_func_begin();

    stream_id = ipacket->get_StreamId();
    strm = _network->get_Stream( stream_id );
    if( strm == NULL ){
        mrn_dbg( 1, mrn_printf(FLF, stderr, "stream %u lookup failed\n",
                               stream_id) );
        return -1;
    }

    strm->set_FilterParams( ftype, ipacket );

    mrn_dbg_func_end();
    return 0;
}

int BackEndNode::proc_deleteStream( PacketPtr ipacket ) const
{
    unsigned int stream_id;
    Stream* strm;

    mrn_dbg_func_begin();

    stream_id = (*ipacket)[0]->get_uint32_t();
    strm = _network->get_Stream( stream_id );
    if( strm == NULL ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "stream %u lookup failed\n", 
                               stream_id) );
        return -1;
    } 

    if( stream_id < USER_STRM_BASE_ID ) {
        // kill internal streams
        delete strm;
    }
    else {
        // close user streams
        strm->close();
    }
    mrn_dbg_func_end();
    return 0;
}


int BackEndNode::proc_DeleteSubTree( PacketPtr ) const
{
    mrn_dbg_func_begin();

    // processes will be exiting -- disable failure recovery
    _network->disable_FailureRecovery();

    // kill threads, topology, and events
    _network->shutdown_Network();

    // exit recv/EDT thread
    mrn_dbg(5, mrn_printf(FLF, stderr, "I'm going away now!\n"));
    _network->free_ThreadState();
    XPlat::Thread::Exit(NULL);

    return 0;
}

int BackEndNode::proc_newFilter( PacketPtr ipacket ) const
{
    int retval = 0;
    unsigned nfuncs = 0;
    char* so_file = NULL;
    char** funcs = NULL;
    unsigned short* fids = NULL;

    mrn_dbg_func_begin();

    retval = ipacket->unpack( "%s %as %auhd",
                              &so_file,
                              &funcs, &nfuncs,
                              &fids, &nfuncs );
    if( retval == 0 ) {
        for( unsigned u=0; u < nfuncs; u++ ) {
            int rc = Filter::load_FilterFunc( fids[u], so_file, funcs[u] );
            if( rc == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "Filter::load_FilterFunc(%s,%s) failed.\n",
                                       so_file, funcs[u]) );
            }
            free( funcs[u] );
        }
        free( funcs );
        free( fids );
        free( so_file );
    }

    mrn_dbg_func_end();
    return retval;
}


} // namespace MRN
