/****************************************************************************
 * Copyright © 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <iostream>

#include "utils.h"
#include "RSHParentNode.h"
#include "SerialGraph.h"
#include "xplat/Process.h"
#include "xplat/Error.h"


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

int 
RSHParentNode::proc_newSubTree( PacketPtr ipacket )
{
    char *byte_array = NULL;
    char *backend_exe = NULL;
    char *commnode_path=NULL;
    char **backend_argv;
    unsigned int backend_argc;

    mrn_dbg_func_begin();

    // _initial_subtree_packet = ipacket;

    if( ipacket->ExtractArgList( "%s%s%s%as", &byte_array, &commnode_path,
                               &backend_exe, &backend_argv, &backend_argc ) == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "ExtractArgList() failed\n" ));
        return -1;
    }

    SerialGraph sg( byte_array );
    
    //change the bytearray extracted from the packet - the local port should be changed to actual dynamic local port
    //assign the changed byte array to initial subtree packet
    //assign the changed byte array to local network Topology
    
    sg.set_Port(_network->get_LocalHostName(), _network->get_LocalPort(), _network->get_LocalRank() );
    std::string new_topo = sg.get_ByteArray();

    _network->reset_Topology( new_topo );

    PacketPtr packet( new Packet( 0, PROT_NEW_SUBTREE, "%s%s%s%as", new_topo.c_str( ),
                                      commnode_path, backend_exe, backend_argv, backend_argc ) );  

    _initial_subtree_packet = packet;

    Stream* topo_stream= _network->get_Stream(1);
    topo_stream->send( PROT_TOPO_UPDATE ,"%ad %aud %aud %as %auhd", 
                       1, 1, NULL , 1, _network->get_LocalRank() , 1 , NULL , 1, _network->get_LocalPort(), 1 );


    std::string backend_exe_str( ( backend_exe == NULL ) ? "" : backend_exe );

    SerialGraph *cur_sg, *my_sg;

    //use "UnknownPort" in lookup since this is what serialgraph was created w/
    my_sg = sg.get_MySubTree( _hostname, _network->get_LocalPort(), _rank );
    if( my_sg == NULL ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "get_MySubTree() failed\n" ));
        return -1;
    }
    my_sg->set_ToFirstChild( );
    
    for( cur_sg = my_sg->get_NextChild( ); cur_sg;
         cur_sg = my_sg->get_NextChild( ) ) {
        subtreereport_sync.Lock( );
        _num_children++;
        subtreereport_sync.Unlock( );

        std::string cur_node_hostname = cur_sg->get_RootHostName( ); 
        Rank cur_node_rank = cur_sg->get_RootRank( );

        if( !cur_sg->is_RootBackEnd( ) ) {
            mrn_dbg( 5, mrn_printf(FLF, stderr, "launching internal node ...\n" ));
            launch_InternalNode( cur_node_hostname, cur_node_rank, commnode_path );
        }
        else {
            if( backend_exe_str.length( ) > 0 ) {
                mrn_dbg( 5, mrn_printf(FLF, stderr, "launching backend_exe: \"%s\"\n",
                                       backend_exe_str.c_str() )); 
                std::vector < std::string > args;
                for(unsigned int i=0; i<backend_argc; i++){
                    args.push_back( backend_argv[i] );
                }
                if( launch_Application( cur_node_hostname, cur_node_rank,
                                        backend_exe_str, args ) == -1 ){
                    mrn_dbg( 1, mrn_printf(FLF, stderr,
                                "launch_application() failed\n" ));
                    return -1;
                }
            }
            else {
                // BE attach case
                mrn_dbg( 5, mrn_printf(FLF, stderr, "launching internal node ...\n" ));
                launch_InternalNode( cur_node_hostname, cur_node_rank, commnode_path );
            }
        }
    }

    mrn_dbg( 3, mrn_printf(FLF, stderr, "proc_newSubTree() succeeded\n" ));
    return 0;
}

int 
RSHParentNode::launch_InternalNode( std::string ihostname, Rank irank,
                                     std::string icommnode_exe )
    const
{
    char parent_port_str[128];
    snprintf(parent_port_str, sizeof(parent_port_str), "%d", _port);
    char parent_rank_str[128];
    snprintf(parent_rank_str, sizeof(parent_rank_str), "%d", _rank);
    char rank_str[128];
    snprintf(rank_str, sizeof(rank_str), "%d", irank );

    mrn_dbg(3, mrn_printf(FLF, stderr, "Launching %s:%d ...\n",
                          ihostname.c_str(), irank ));

    // set up arguments for the new process
    std::vector <std::string> args;
    args.push_back(icommnode_exe);
    args.push_back(_hostname);
    args.push_back(parent_port_str);
    args.push_back( parent_rank_str );
    args.push_back(ihostname);
    args.push_back(rank_str);

#if READY
    std::cout << "Manually create process:\n";
    for( unsigned int i = 0; i < args.size(); i++ )
    {
        std::cout << args[i] << '\n';
    }
    std::cout << std::endl;
#else
    if( XPlat::Process::Create( ihostname, icommnode_exe, args ) != 0 ){
        int err = XPlat::Process::GetLastError();
        
        error( ERR_SYSTEM, get_Rank(), "XPlat::Process::Create(%s %s): %s\n",
               ihostname.c_str(), icommnode_exe.c_str(),
               XPlat::Error::GetErrorString( err ).c_str() );
        mrn_dbg(1, mrn_printf(FLF, stderr,
                              "XPlat::Process::Create(%s %s): %s\n",
                              ihostname.c_str(), icommnode_exe.c_str(),
                              XPlat::Error::GetErrorString( err ).c_str() ));
        return -1;
    }
#endif // READY

    mrn_dbg(3, mrn_printf(FLF, stderr, "Successful launch of %s:%d\n",
                          ihostname.c_str(), irank ));

    return 0;
}


int
RSHParentNode::launch_Application( std::string ihostname, Rank irank, std::string &ibackend_exe,
                                    std::vector <std::string> &ibackend_args)
    const
{

    mrn_dbg(3, mrn_printf(FLF, stderr, "Launching application on %s:%d (\"%s\")\n",
                          ihostname.c_str(), irank, ibackend_exe.c_str()));
  
    char parent_port_str[128];
    snprintf(parent_port_str, sizeof( parent_port_str), "%d", _port);
    char parent_rank_str[128];
    snprintf(parent_rank_str, sizeof( parent_rank_str), "%d", _rank );
    char rank_str[128];
    snprintf(rank_str, sizeof(rank_str), "%d", irank );

    // set up arguments for new process: copy to get the cmd in front

    std::vector<std::string> new_args;

    new_args.push_back( ibackend_exe );
    for(unsigned int i=0; i<ibackend_args.size(); i++){
        new_args.push_back(ibackend_args[i]);
    }
    new_args.push_back( _hostname );
    new_args.push_back( parent_port_str );
    new_args.push_back( parent_rank_str );
    new_args.push_back( ihostname );
    new_args.push_back( rank_str );

    mrn_dbg(5, mrn_printf(FLF, stderr, "Creating \"%s\" on \"%s:%d\"\n",
                          ibackend_exe.c_str(), ihostname.c_str(), irank ));
  
#if READY
    std::cout << "Manually create process:\n";
    for( unsigned int i = 0; i < new_args.size(); i++ )
    {
        std::cout << new_args[i] << '\n';
    }
    std::cout << std::endl;
#else
    if( XPlat::Process::Create( ihostname, ibackend_exe, new_args ) != 0 ){
        mrn_dbg(1, mrn_printf(FLF, stderr, "XPlat::Process::Create() failed\n"));
        int err = XPlat::Process::GetLastError();
        error( ERR_SYSTEM, irank, "XPlat::Process::Create(%s %s): %s\n",
               ihostname.c_str(), ibackend_exe.c_str(),
               XPlat::Error::GetErrorString( err ).c_str() );
        return -1;
    }
#endif // READY
    mrn_dbg(3, mrn_printf(FLF, stderr, "Successful launch of app on %s:%d\n",
                          ihostname.c_str(), irank));

    return 0;
}

int
RSHParentNode::proc_SubTreeInfoRequest( PacketPtr  ipacket ) const
{
    //send the packet containing the initial subtree to requesting node
    mrn_dbg(5, mrn_printf(FLF, stderr, "sending initial subtree packet:\n"
                          "\t%s\n", (*_initial_subtree_packet)[0]->get_string() ));
    PeerNodePtr outlet = _network->get_PeerNode( ipacket->get_InletNodeRank() );
    if( ( outlet->send( _initial_subtree_packet ) == -1 ) ||
        ( outlet->flush() == -1 ) ){
        mrn_dbg(1, mrn_printf(FLF, stderr, "send()/flush() failed\n" ));
        return -1;
    }

    return 0;
}

int
RSHParentNode::proc_PacketFromChildren( PacketPtr cur_packet )
{
    int retval = 0;

    switch ( cur_packet->get_Tag(  ) ) {
    case PROT_SUBTREE_INFO_REQ:
        if( proc_SubTreeInfoRequest( cur_packet ) == -1 ){
            mrn_dbg( 1, mrn_printf(FLF, stderr,
                                   "proc_SubTreeInfoRequest() failed\n" ));
            retval = -1;
        }
        break;
    default:
        retval = ParentNode::proc_PacketFromChildren( cur_packet );
    }
    return retval;
}



} // namespace MRN
