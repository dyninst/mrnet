/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
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

RSHParentNode::RSHParentNode(void)
{
}

RSHParentNode::~RSHParentNode(void)
{
}

int 
RSHParentNode::proc_PacketFromChildren( PacketPtr cur_packet )
{
    int retval = 0;
    
    int tag = cur_packet->get_Tag();
    switch( tag ) {
     case PROT_LAUNCH_SUBTREE: {
         // child is requesting the launch info
         Rank child_rank = cur_packet->get_InletNodeRank();
         PeerNodePtr child = _network->get_PeerNode( child_rank );
         send_LaunchInfo( child );
         break;
     }
     default:
         retval = ParentNode::proc_PacketFromChildren( cur_packet );
    }

    return retval;
}

int 
RSHParentNode::send_LaunchInfo( PeerNodePtr child_node ) const
{
    mrn_dbg_func_begin();
    child_node->send( _launch_pkt );
    child_node->flush();
    return 0;
}

int 
RSHParentNode::proc_LaunchSubTree( PacketPtr ipacket )
{
    const char *commnode_path = NULL;
    const char *backend_exe = NULL;
    const char **backend_argv;
    uint32_t backend_argc;
    int rc;  
    DataType dt;

    mrn_dbg_func_begin();

    _launch_pkt = ipacket;

    commnode_path = (*ipacket)[0]->get_string(); 
    backend_exe = (*ipacket)[1]->get_string();
    backend_argv = (const char**) (*ipacket)[2]->get_array( &dt, &backend_argc );

    bool have_backend_exe = ( backend_exe != NULL );
    std::string backend_exe_str( have_backend_exe ? backend_exe : NULL_STRING );

    NetworkTopology* nettop = _network->get_NetworkTopology();
    if( NULL == nettop ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "internal error, topology is NULL\n") );
        return -1;
    }

    std::string myhost = get_HostName();
    std::string topol = nettop->get_TopologyString();
    Rank myrank = get_Rank();

    SerialGraph sg( topol );
    SerialGraph *cur_sg, *my_sg;
    my_sg = sg.get_MySubTree( myhost, UnknownPort, myrank );
    if( NULL == my_sg ) {
        my_sg = sg.get_MySubTree( myhost, _network->get_LocalPort(), myrank );
        if( NULL == my_sg ) {// I must have attached, since I'm not in the topology
            mrn_dbg_func_end();
            return 0;
        }
    }

    my_sg->set_ToFirstChild( );
    cur_sg = my_sg->get_NextChild();
    for( ; cur_sg; cur_sg = my_sg->get_NextChild() ) {

        subtreereport_sync.Lock( );
        _num_children++;
        subtreereport_sync.Unlock( );

        std::string child_hostname = cur_sg->get_RootHostName(); 
        Rank child_rank = cur_sg->get_RootRank();

        if( ! cur_sg->is_RootBackEnd() ) {
            mrn_dbg( 5, mrn_printf(FLF, stderr, "launching internal node ...\n") );
            rc = launch_InternalNode( child_hostname, child_rank, commnode_path );
            if( -1 == rc ) {
                mrn_dbg( 1, mrn_printf(FLF, stderr,
                                       "launch_InternalNode() failed\n") );
                return rc;
            }
        }
        else {
            if( have_backend_exe ) {
                mrn_dbg( 5, mrn_printf(FLF, stderr, "launching backend '%s'\n",
                                       backend_exe_str.c_str()) ); 
                std::vector < std::string > args;
                for( uint64_t i=0; i < backend_argc; i++ ) {
                    args.push_back( backend_argv[i] );
                }
                rc = launch_Application( child_hostname, child_rank,
                                         backend_exe_str, args );
                if( -1 == rc ) {
                    mrn_dbg( 1, mrn_printf(FLF, stderr,
                                           "launch_Application() failed\n") );
                    return rc;
                }
            }
            else {
                // BE attach case
                mrn_dbg( 5, mrn_printf(FLF, stderr, "launching internal node ...\n") );
                rc = launch_InternalNode( child_hostname, child_rank, commnode_path );
                if( -1 == rc ) {
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
    char parent_port_str[16];
    char parent_rank_str[16];
    char rank_str[16];

    snprintf( parent_port_str, sizeof(parent_port_str), "%hu", get_Port() );
    snprintf( parent_rank_str, sizeof(parent_rank_str), "%u", get_Rank() );
    snprintf( rank_str, sizeof(rank_str), "%u", irank );

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Launching %s:%d ...\n",
                           ihostname.c_str(), irank) );

    // set up arguments for the new process
    std::vector <std::string> args;
    args.push_back( icommnode_exe );
    args.push_back( get_HostName() );
    args.push_back( parent_port_str );
    args.push_back( parent_rank_str );
    args.push_back( ihostname );
    args.push_back( rank_str );

    if( XPlat::Process::Create( ihostname, icommnode_exe, args ) != 0 ){
        int err = XPlat::Process::GetLastError();
        
        error( ERR_SYSTEM, get_Rank(), "XPlat::Process::Create('%s','%s'): %s",
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
    char parent_port_str[16];
    char parent_rank_str[16];
    char rank_str[16];

    mrn_dbg( 3, mrn_printf(FLF, stderr, 
                           "Launching application on %s:%d (\"%s\")\n",
                           ihostname.c_str(), irank, ibackend_exe.c_str()) );
  
    snprintf( parent_port_str, sizeof(parent_port_str), "%d", get_Port() );
    snprintf( parent_rank_str, sizeof(parent_rank_str), "%d", get_Rank() );
    snprintf( rank_str, sizeof(rank_str), "%d", irank );

    // set up arguments for new process: copy to get the cmd in front
    
    // set up arguments for the new process
    std::vector< std::string > new_args;
    new_args.push_back( ibackend_exe );
    for(unsigned int i=0; i<ibackend_args.size(); i++){
        new_args.push_back(ibackend_args[i]);
    }
    new_args.push_back( get_HostName() );
    new_args.push_back( parent_port_str );
    new_args.push_back( parent_rank_str );
    new_args.push_back( ihostname );
    new_args.push_back( rank_str );

    mrn_dbg( 5, mrn_printf(FLF, stderr, "Creating '%s' on %s:%d\n",
                           ibackend_exe.c_str(), ihostname.c_str(), irank) );
  
    if( XPlat::Process::Create(ihostname, ibackend_exe, new_args) != 0 ){
        int err = XPlat::Process::GetLastError();
        mrn_dbg( 1, mrn_printf(FLF, stderr, 
                               "XPlat::Process::Create() failed with '%s'\n",
                               strerror(err)) );
        return -1;
    }

    mrn_dbg( 3, mrn_printf(FLF, stderr, "Successful app launch on %s:%d\n",
                           ihostname.c_str(), irank) );
    
    return 0;
}

} // namespace MRN
