/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
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

    // create topology update stream
    Stream* s = _network->new_Stream( TOPOL_STRM_ID, NULL, 0, 
                                      TFILTER_TOPO_UPDATE, 
                                      SFILTER_TIMEOUT, 
                                      TFILTER_TOPO_UPDATE_DOWNSTREAM );
    assert( s != NULL );

    // establish data connection w/ parent
    if( init_newChildDataConnection( _network->get_ParentNode() ) == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                               "init_newChildDataConnection() failed\n") );
    }

    // start event detection thread, which establishes event connection
    if( ! EventDetector::start( _network ) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                               "EventDetector::start() failed\n") );
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
    uint32_t num_backends;
    unsigned int stream_id;
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
    else { // PROT_NEW_STREAM or PROT_NEW_INTERNAL_STREAM

        if( ipacket->unpack("%ud %ad %d %d %d", 
                            &stream_id, &backends, &num_backends, 
                            &us_filter_id, &sync_id, &ds_filter_id) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "unpack() failed\n" ));
            return -1;
        }
    }

    if(us_filter_id > UINT16_MAX || us_filter_id < 0) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Filter ID too large\n"));
        return -1;
    }
    if(sync_id > UINT16_MAX || sync_id < 0) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Filter ID too large\n"));
        return -1;
    }
    if(ds_filter_id > UINT16_MAX || ds_filter_id < 0) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Filter ID too large\n"));
        return -1;
    }

    _network->new_Stream( stream_id, backends, num_backends, 
                          (unsigned short)us_filter_id,
                          (unsigned short)sync_id,
                          (unsigned short)ds_filter_id );

    if( backends != NULL )
        free( backends );

    if( tag == PROT_NEW_INTERNAL_STREAM ) {
        // send ack to parent
        if( ! ack_ControlProtocol(PROT_NEW_STREAM_ACK, true) ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, 
                                  "ack_ControlProtocol(PROT_NEW_STREAM_ACK) failed\n"));
        }
    }

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

    if(Network::is_UserStreamId(stream_id)) {
        if(strm != NULL) {
            // close user streams
            strm->close();
        } else {
            mrn_dbg( 1, mrn_printf(FLF, stderr, "stream %u lookup failed\n", 
                        stream_id) );
            return -1;
        }
    } else {
        if(strm != NULL) {
            delete strm;
        }
    }

    mrn_dbg_func_end();
    return 0;
}

int BackEndNode::proc_newFilter( PacketPtr ipacket ) const
{
    int retval = 0;
    uint32_t nfuncs = 0;
    char* so_file = NULL;
    char** funcs = NULL;
    unsigned short* fids = NULL; 
    mrn_dbg_func_begin();

    retval = ipacket->unpack( "%s %as %auhd",
                              &so_file,
                              &funcs, &nfuncs,
                              &fids, &nfuncs );

    std::vector<unsigned> error_funcs;

    if( retval == 0 ) {
        for( unsigned u=0; u < nfuncs; u++ ) {
            int rc = Filter::load_FilterFunc(_network->GetFilterInfo(), fids[u], so_file, funcs[u] );
            if( rc == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "Filter::load_FilterFunc(%s,%s) failed.\n",
                                       so_file, funcs[u]) );

                error_funcs.push_back(u);
            }
            free( funcs[u] );
        }
        // Write out an error packet with the function names
        // This is so that the FE can print the error to make it
        // obvious what has failed.
        if (error_funcs.size() > 0) {
            char ** hostnames = new char * [error_funcs.size()];
            unsigned * func_cstr = new unsigned[error_funcs.size()];
            char ** so_filename = new char * [error_funcs.size()];
            for (unsigned u = 0; u < error_funcs.size(); u++) {
                func_cstr[u] = error_funcs[u];
                hostnames[u] = strdup(_network->get_LocalHostName().c_str());
                so_filename[u] = strdup(so_file);
            }
            unsigned length = (unsigned)error_funcs.size();
            PacketPtr packet(new Packet(CTL_STRM_ID, PROT_EVENT, "%as %as %aud",
                                hostnames, length, so_filename, length, func_cstr, length));
            _network->send_PacketToParent(packet);
        } else {
            char ** emptySend = NULL;
            unsigned ** func_empty = NULL;
            PacketPtr packet(new Packet(CTL_STRM_ID, PROT_EVENT, "%as %as %aud",
                                emptySend, 0, emptySend, 0, func_empty, 0));
            _network->send_PacketToParent(packet);
        }
        free( funcs );
        free( fids );
        free( so_file );
    }
    _network->flush_PacketsToParent();
    mrn_dbg_func_end();
    return retval;
}


} // namespace MRN
