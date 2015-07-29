/******************************************************************************
 *  Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                   Detailed MRNet usage rights in "LICENSE" file.           *
 ******************************************************************************/

#include "utils.h"
#include "FrontEndNode.h"
#include "ParsedGraph.h"
#include "RSHBackEndNode.h"
#include "RSHFrontEndNode.h"
#include "RSHInternalNode.h"
#include "RSHNetwork.h"

#include "mrnet/MRNet.h"
#include "xplat/Process.h"

namespace MRN
{

//----------------------------------------------------------------------------
// base class factory methods
// creates objects of our specialization
Network*
Network::CreateNetworkFE( const char * itopology,
                          const char * ibackend_exe,
                          const char **ibackend_argv,
                          const std::map< std::string, std::string >* iattrs,
                          bool irank_backends,
                          bool iusing_mem_buf )
{
    // endianTest();

    Network* net = new RSHNetwork(iattrs);
    net->init_FrontEnd( itopology,
                        ibackend_exe,
                        ibackend_argv,
                        iattrs,
                        irank_backends,
                        iusing_mem_buf );
    return net;
}


Network*
Network::CreateNetworkBE( int argc, char* argv[] )
{
    // Get parent/local info from end of BE args
   
    if( argc >= 6 ) {
        const char* phostname = argv[argc-5];
	Port pport = (Port)strtoul( argv[argc-4], NULL, 10 );
	Rank prank = (Rank)strtoul( argv[argc-3], NULL, 10 );
	const char* myhostname = argv[argc-2];
	Rank myrank = (Rank)strtoul( argv[argc-1], NULL, 10 );

	return new RSHNetwork( phostname, pport, prank,
			       myhostname, myrank, false );
    }
    else {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "Not enough args, please provide parent/local info\n"));
        return NULL;
    }
}


Network*
Network::CreateNetworkIN( int argc, char* argv[] )
{
    // parse arguments
    if( argc >= 5 ) {
        const char* phostname = argv[0];
        Port pport = (Port)strtoul( argv[1], NULL, 10 );
        Rank prank = (Rank)strtoul( argv[2], NULL, 10 );
        const char* myhostname = argv[3];
        Rank myrank = (Rank)strtoul( argv[4], NULL, 10 );

        return new RSHNetwork( phostname, pport, prank,
                               myhostname, myrank, true );
    }
    else {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "Not enough args, please provide parent/local info\n"));
        return NULL;
    }
}

FrontEndNode*
RSHNetwork::CreateFrontEndNode( Network* n,
                                std::string ihost,
                                Rank irank )
{
    return new RSHFrontEndNode( n, ihost, irank );
}

BackEndNode*
RSHNetwork::CreateBackEndNode(Network* inetwork, 
                                std::string imy_hostname, 
                                Rank imy_rank,
                                std::string iphostname, 
                                Port ipport, 
                                Rank iprank )
{
    return new RSHBackEndNode( inetwork,
                               imy_hostname,
                               imy_rank,
                               iphostname,
                               ipport,
                               iprank );
}

InternalNode*
RSHNetwork::CreateInternalNode( Network* inetwork, 
                                std::string imy_hostname,
                                Rank imy_rank,
                                std::string iphostname,
                                Port ipport, 
                                Rank iprank,
                                int idataSocket,
                                Port idataPort )
{
    XPlat_Socket listeningSocket = idataSocket;
    Port listeningPort = idataPort;

    // we build and bind our listening socket now
    // because other platforms need to have this information
    // in their initialization code, and it doesn't hurt us
    // to bind it early
    int bRet = bindPort( &listeningSocket, &listeningPort, true /*non-blocking*/ );
    if( bRet == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, 
                               "Failure: unable to instantiate network\n") );
        return NULL;
    }
    assert( listeningPort != UnknownPort );

    return new RSHInternalNode( inetwork,
                                imy_hostname,
                                imy_rank,
                                iphostname,
                                ipport,
                                iprank,
                                listeningSocket,
                                listeningPort );
}

//----------------------------------------------------------------------------
// RSHNetwork methods

// FE constructor
RSHNetwork::RSHNetwork( const std::map< std::string, std::string >* iattrs )
    : Network()
{
    // check attributes for XPlat RSH settings
    if( iattrs != NULL ) {
        std::map< std::string, std::string >::const_iterator iter = iattrs->begin();
        for( ; iter != iattrs->end(); iter++ ) {
            const char* cstr = iter->first.c_str();
            if( 0 == strncmp(cstr, "XPLAT_", 6) ) {
                if( 0 == strcmp(cstr, "XPLAT_RSH") ) {
                    _network_settings[ XPLAT_RSH ] = iter->second;
                }
                else if( 0 == strcmp(cstr, "XPLAT_RSH_ARGS") ) {
                    _network_settings[ XPLAT_RSH_ARGS ] = iter->second;
                }
                else if( 0 == strcmp(cstr, "XPLAT_REMCMD") ) {
                    _network_settings[ XPLAT_REMCMD ] = iter->second;
                }
            }
        }

    }

    // initialize XPlat from environment if not passed in iattrs
    char* envval;
    if( _network_settings.find(XPLAT_RSH) == _network_settings.end() ) {
        envval = getenv( "XPLAT_RSH" );
        if( envval != NULL )
            _network_settings[ XPLAT_RSH ] = std::string( envval );
    }    
    if( _network_settings.find(XPLAT_RSH_ARGS) == _network_settings.end() ) {
        envval = getenv( "XPLAT_RSH_ARGS" );
        if( envval != NULL )
            _network_settings[ XPLAT_RSH_ARGS ] = std::string( envval );
    }
    if( _network_settings.find(XPLAT_REMCMD) == _network_settings.end() ) {
        envval = getenv( "XPLAT_REMCMD" );
        if( envval != NULL )
            _network_settings[ XPLAT_REMCMD ] = std::string( envval );
    }
}

// BE and IN constructor
RSHNetwork::RSHNetwork( const char* phostname, Port pport, Rank prank,
                        const char* myhostname, Rank myrank, bool isInternal )
{
    if( isInternal ) {
        // initialize as a Network IN
        Network::init_InternalNode( phostname,
                                    pport,
                                    prank,
                                    myhostname,
                                    myrank );

        RSHInternalNode* in = dynamic_cast<RSHInternalNode*>( get_LocalInternalNode() );
        if( in == NULL )
            mrn_dbg( 1, mrn_printf(FLF, stderr, "internal-node dynamic_cast failed\n") );
        else
            in->request_LaunchInfo();
    }
    else {
        // initialize as a Network BE
        Network::init_BackEnd( phostname,
                               pport,
                               prank,
                               myhostname,
                               myrank );
    }
}

void RSHNetwork::init_NetSettings(void)
{
    // XPlat RSH settings
    std::map< net_settings_key_t, std::string >::iterator eit;
    eit = _network_settings.find( XPLAT_RSH );
    if( eit != _network_settings.end() )
        XPlat::Process::set_RemoteShell( eit->second );

    eit = _network_settings.find( XPLAT_RSH_ARGS );
    if( eit != _network_settings.end() )
        XPlat::Process::set_RemoteShellArgs( eit->second );

    eit = _network_settings.find( XPLAT_REMCMD );
    if( eit != _network_settings.end() )
        XPlat::Process::set_RemoteCommand( eit->second );

    // still need to process standard settings
    Network::init_NetSettings();
}

bool
RSHNetwork::Instantiate( ParsedGraph* _parsed_graph,
                         const char* mrn_commnode_path,
                         const char* backend_exe,
                         const char** backend_args,
                         unsigned int backend_argc,
                         const std::map< std::string, std::string >* /* iattrs */ )
{
    bool have_backends = ( strlen(backend_exe) != 0 );
    std::string sg = _parsed_graph->get_SerializedGraphString( have_backends );

    NetworkTopology* nt = get_NetworkTopology();
    if( nt != NULL ) {
        nt->reset( sg, false );
        NetworkTopology::Node* localnode = nt->find_Node( get_LocalRank() );
        localnode->set_Port( get_LocalPort() );
    }   

    RSHFrontEndNode* fe = dynamic_cast<RSHFrontEndNode*>( get_LocalFrontEndNode() );
    if( fe == NULL ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "front-end dynamic_cast failed\n") );
        return false;
    }

    mrn_dbg( 5, mrn_printf(FLF, stderr, "Instantiating network\n") );

    PacketPtr packet( new MRN::Packet(CTL_STRM_ID, PROT_LAUNCH_SUBTREE, "%s %s %as", 
                                      mrn_commnode_path, 
                                      backend_exe, 
                                      backend_args, backend_argc) );    
    if( fe->proc_LaunchSubTree(packet) == -1 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "failed to spawn subtree\n") );
        error( ERR_INTERNAL, get_LocalRank(), "failed to spawn subtree");
        return false;
    }

    return true;
}

} // namespace MRN

