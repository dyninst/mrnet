/****************************************************************************
 * Copyright © 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <iostream>
#include "utils.h"
#include "ChildNode.h"
#include "RSHParentNode.h"
#include "SerialGraph.h"
#include "xplat/Process.h"
#include "xplat/Error.h"
#include "mrnet/MRNet.h"

namespace MRN
{

/*====================================================*/
/*  ParentNode CLASS METHOD DEFINITIONS            */
/*====================================================*/
RSHParentNode::RSHParentNode( Network* inetwork,
                              std::string const& ihostname,
                              Rank irank )
  : CommunicationNode( ihostname, UnknownPort, irank ),
    ParentNode( inetwork, ihostname, irank )
{
    // nothing else to do
}

RSHParentNode::~RSHParentNode( void )
{
    // nothing else to do
}

int RSHParentNode::proc_PortUpdates( PacketPtr ipacket ) const
{
    mrn_dbg_func_begin();

    Stream* port_strm = NULL;

    NetworkTopology* net_topo = _network->get_NetworkTopology();
    unsigned net_size = net_topo->get_NumNodes();

    if( _network->is_LocalNodeFrontEnd() && (net_size > 1) ) {
        // create a waitforall topology update stream
        Communicator* bcast_comm = _network->get_BroadcastCommunicator();
        port_strm = _network->new_InternalStream( bcast_comm, TFILTER_TOPO_UPDATE, 
                                                  SFILTER_WAITFORALL );
        if( NULL == port_strm ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr, 
                                   "failed to create port update stream\n") );
            return -1;
        }
        assert( port_strm->get_Id() == PORT_STRM_ID );
    }

    // request port updates from children
    if( ( _network->send_PacketToChildren(ipacket) == -1 ) ||
        ( _network->flush_PacketsToChildren() == -1 ) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, 
                               "send/flush_PacketToChildren() failed\n") );
        return -1;
    }

    if( _network->is_LocalNodeFrontEnd() && (net_size > 1) ) {
        // block until updates received, then kill the stream
        int tag;
        PacketPtr p;
        port_strm->recv( &tag, p );
        delete port_strm;

        // broadcast the accumulated updates
        _network->send_TopologyUpdates();
    }

    mrn_dbg_func_end();
    return 0;
}

int 
RSHParentNode::proc_newSubTree( PacketPtr ipacket )
{
    const char *byte_array = NULL;
    const char *backend_exe = NULL;
    const char *commnode_path = NULL;
    const char **backend_argv;
    uint32_t backend_argc;
    int rc;
  
    DataType dt;

    mrn_dbg_func_begin();

    byte_array = (*ipacket)[0]->get_string();
    commnode_path = (*ipacket)[1]->get_string(); 
    backend_exe = (*ipacket)[2]->get_string();
    backend_argv = (const char**) (*ipacket)[3]->get_array( &dt, &backend_argc );

    SerialGraph sg( byte_array );
    
    // update the local port in the topology string to actual dynamic port
    sg.set_Port( _network->get_LocalHostName(), 
                 _network->get_LocalPort(), 
                 _network->get_LocalRank() );
    std::string new_topo = sg.get_ByteArray();

    PacketPtr packet( new MRN::Packet(CTL_STRM_ID, PROT_LAUNCH_SUBTREE, "%s %s %s %as", 
                                      new_topo.c_str(), commnode_path, 
                                      backend_exe, backend_argv, backend_argc) );  

    _initial_subtree_packet = packet;
    
    bool have_backend_exe = ( backend_exe != NULL );
    std::string backend_exe_str( have_backend_exe ? backend_exe : NULL_STRING );

    SerialGraph *cur_sg, *my_sg;
    my_sg = sg.get_MySubTree( _hostname, _network->get_LocalPort(), _rank );
    if( my_sg == NULL ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "get_MySubTree() failed\n") );
        return -1;
    }

    my_sg->set_ToFirstChild( );
    cur_sg = my_sg->get_NextChild();
    for( ; cur_sg; cur_sg = my_sg->get_NextChild() ) {

        subtreereport_sync.Lock( );
        _num_children++;
        subtreereport_sync.Unlock( );

        std::string cur_node_hostname = cur_sg->get_RootHostName( ); 
        Rank cur_node_rank = cur_sg->get_RootRank( );

        if( ! cur_sg->is_RootBackEnd() ) {
            mrn_dbg( 5, mrn_printf(FLF, stderr, "launching internal node ...\n") );
            rc = launch_InternalNode( cur_node_hostname, cur_node_rank, commnode_path );
            if( rc == -1 ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "launch_InternalNode() failed\n") );
                return rc;
            }
        }
        else {
            if( have_backend_exe ) {
                mrn_dbg( 5, mrn_printf(FLF, stderr, "launching backend_exe: \"%s\"\n",
                                       backend_exe_str.c_str()) ); 
                std::vector < std::string > args;
                for( uint64_t i=0; i < backend_argc; i++ ) {
                    args.push_back( backend_argv[i] );
                }
                rc = launch_Application( cur_node_hostname, cur_node_rank,
                                         backend_exe_str, args );
                if( rc == -1 ) {
                    mrn_dbg( 1, mrn_printf(FLF, stderr,
                                           "launch_Application() failed\n") );
                    return rc;
                }
            }
            else {
                // BE attach case
                mrn_dbg( 5, mrn_printf(FLF, stderr, "launching internal node ...\n") );
                rc = launch_InternalNode( cur_node_hostname, cur_node_rank, commnode_path );
                if( rc == -1 ) {
                    mrn_dbg( 1, mrn_printf(FLF, stderr,
                                           "launch_InternalNode() failed\n") );
                    return rc;
                }
            }
        }
        delete cur_sg;
    }
    delete my_sg;

    mrn_dbg_func_end();
    return 0;
}

int 
RSHParentNode::launch_InternalNode( std::string ihostname, Rank irank,
                                    std::string icommnode_exe ) const
{
    char parent_port_str[128];
    snprintf(parent_port_str, sizeof(parent_port_str), "%d", _port);
    char parent_rank_str[128];
    snprintf(parent_rank_str, sizeof(parent_rank_str), "%d", _rank);
    char rank_str[128];
    snprintf(rank_str, sizeof(rank_str), "%d", irank );

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Launching %s:%d ...\n",
                           ihostname.c_str(), irank) );

    // set up arguments for the new process
    std::vector <std::string> args;

    args.push_back(icommnode_exe);
    args.push_back(_hostname);
    args.push_back(parent_port_str);
    args.push_back( parent_rank_str );
    args.push_back(ihostname);
    args.push_back(rank_str);

    if( XPlat::Process::Create( ihostname, icommnode_exe, args ) != 0 ){
        int err = XPlat::Process::GetLastError();
        
        error( ERR_SYSTEM, get_Rank(), "XPlat::Process::Create(%s %s): %s",
               ihostname.c_str(), icommnode_exe.c_str(),
               XPlat::Error::GetErrorString( err ).c_str() );
        return -1;
    }

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Successful launch of %s:%d\n",
                           ihostname.c_str(), irank) );

    return 0;
}


int
RSHParentNode::launch_Application( std::string ihostname, Rank irank, 
                                   std::string &ibackend_exe,
                                   std::vector <std::string> &ibackend_args ) const
{

    mrn_dbg( 3, mrn_printf(FLF, stderr, 
                           "Launching application on %s:%d (\"%s\")\n",
                           ihostname.c_str(), irank, ibackend_exe.c_str()) );
  
    char parent_port_str[128];
    snprintf( parent_port_str, sizeof(parent_port_str), "%d", _port );

    char parent_rank_str[128];
    snprintf( parent_rank_str, sizeof(parent_rank_str), "%d", _rank );

    char rank_str[128];
    snprintf( rank_str, sizeof(rank_str), "%d", irank );

    // set up arguments for new process: copy to get the cmd in front
    
    // set up arguments for the new process
    std::vector< std::string > new_args;

    new_args.push_back( ibackend_exe );
    for(unsigned int i=0; i<ibackend_args.size(); i++){
        new_args.push_back(ibackend_args[i]);
    }
    new_args.push_back( _hostname );
    new_args.push_back( parent_port_str );
    new_args.push_back( parent_rank_str );
    new_args.push_back( ihostname );
    new_args.push_back( rank_str );

    mrn_dbg( 5, mrn_printf(FLF, stderr, "Creating \"%s\" on \"%s:%d\"\n",
                           ibackend_exe.c_str(), ihostname.c_str(), irank) );
  
    if( XPlat::Process::Create( ihostname, ibackend_exe, new_args ) != 0 ){
        int err = XPlat::Process::GetLastError();
        mrn_dbg( 1, mrn_printf(FLF, stderr, 
                               "XPlat::Process::Create() failed with '%s'\n",
                               strerror(err)) );
        return -1;
    }

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Successful launch of app on %s:%d\n",
                           ihostname.c_str(), irank) );
    
    return 0;
}

int
RSHParentNode::proc_SubTreeInfoRequest( PacketPtr ipacket ) const
{
    //send the packet containing the initial subtree to requesting node
    mrn_dbg( 5, mrn_printf(FLF, stderr, 
                           "sending initial subtree packet:\n\t%s\n", 
                           (*_initial_subtree_packet)[0]->get_string()) );
    PeerNodePtr outlet = _network->get_PeerNode( ipacket->get_InletNodeRank() );
    outlet->send( _initial_subtree_packet );
    if( outlet->flush() == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "flush() failed\n") );
        return -1;
    }
    return 0;
}


int
RSHParentNode::proc_PacketFromChildren( PacketPtr cur_packet )
{
    int retval = 0;

    switch( cur_packet->get_Tag() ) {
    case PROT_SUBTREE_INFO_REQ:
        if( proc_SubTreeInfoRequest(cur_packet) == -1 ) {
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "proc_SubTreeInfoRequest() failed\n") );
	    retval = -1;
	}
	break;

    default:
        retval = ParentNode::proc_PacketFromChildren( cur_packet );
	break;
    }
    return retval;
}

} // namespace MRN
