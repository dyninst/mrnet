/******************************************************************************
 *  Copyright © 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
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
    endianTest();

    Network* net = new RSHNetwork;
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
    endianTest();
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
    assert( argc >= 5 );
    const char* phostname = argv[0];
    Port pport = (Port)strtoul( argv[1], NULL, 10 );
    Rank prank = (Rank)strtoul( argv[2], NULL, 10 );
    const char* myhostname = argv[3];
    Rank myrank = (Rank)strtoul( argv[4], NULL, 10 );

    return new RSHNetwork( phostname, pport, prank,
                           myhostname, myrank, true );
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
    int listeningSocket = idataSocket;
    Port listeningPort = idataPort;

    // we build and bind our listening socket now
    // because other platforms need to have this information
    // in their initialization code, and it doesn't hurt us
    // to bind it early
    int bRet = bindPort( &listeningSocket, &listeningPort );
    if( bRet == -1 )
    {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "Failure: unable to instantiate network\n" ));
        error( ERR_INTERNAL, UnknownRank, "" );
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

RSHNetwork::RSHNetwork( void )
{
    // nothing else to do
}


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



void
RSHNetwork::Instantiate( ParsedGraph* _parsed_graph,
                         const char* mrn_commnode_path,
                         const char* ibackend_exe,
                         const char** ibackend_args,
                         unsigned int backend_argc,
                         const std::map< std::string, std::string >* /* iattrs */ )
{
    // save the serialized graph string in a variable on the stack,
    // so that we don't build a packet with a pointer into a temporary
    std::string sg = _parsed_graph->get_SerializedGraphString();

    get_NetworkTopology()->reset(sg,false);
    NetworkTopology* nt=get_NetworkTopology();
    
    if( nt != NULL )
    {
       NetworkTopology::Node* localnode = nt->find_Node( get_LocalRank() );
       localnode->set_Port(get_LocalPort() );
    }   

    PacketPtr packet( new Packet( 0, PROT_NEW_SUBTREE, "%s%s%s%as", sg.c_str( ),
                                  mrn_commnode_path, ibackend_exe, ibackend_args,
                                  backend_argc ) );
    
    mrn_dbg(5, mrn_printf(FLF, stderr, "Instantiating network ... \n" ));

    RSHFrontEndNode* fe = dynamic_cast<RSHFrontEndNode*>( get_LocalFrontEndNode() );
    assert( fe != NULL );

    if( fe->proc_newSubTree( packet ) == -1 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Failure: FrontEndNode::proc_newSubTree()!\n" ));
        error( ERR_INTERNAL, UnknownRank, "");
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

} // namespace MRN

