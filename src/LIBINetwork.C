/******************************************************************************
 *  Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                   Detailed MRNet usage rights in "LICENSE" file.           *
 ******************************************************************************/

#include "utils.h"
#include "FrontEndNode.h"
#include "ParsedGraph.h"
#include "LIBINetwork.h"
#include "FrontEndNode.h"
#include "InternalNode.h"
#include "ParentNode.h"
#include "BackEndNode.h"
#include "ChildNode.h"

#include "mrnet/MRNet.h"
#include "xplat/Process.h"
#include "LIBINetwork.h"
#include "libi/libi_api.h"

using namespace std;

namespace MRN
{


extern const char* mrnBufPtr;
extern unsigned int mrnBufRemaining;

// Added by Taylor:
const int MIN_DYNAMIC_PORT = 1024;

Network*
Network::CreateNetworkFE( const char * itopology,
                          const char * ibackend_exe,
                          const char **ibackend_argv,
                          const std::map< std::string, std::string >* iattrs,
                          bool irank_backends,
                          bool iusing_mem_buf )
{
    mrn_dbg_func_begin();

    //debug
    if( NULL != ibackend_argv ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "argv in CreateNetworkFE:\n"));
        for (int k=0; ibackend_argv[k] != NULL; k++)
            mrn_dbg(1, mrn_printf(FLF, stderr, "argv[%d] = %s\n", k, ibackend_argv[k]));
    }

    Network* net = new LIBINetwork;
    net->init_FrontEnd( itopology,
                        ibackend_exe,
                        ibackend_argv,
                        iattrs,
                        irank_backends,
                        iusing_mem_buf );

    mrn_dbg_func_end();
    return net;
}

Network*
Network::CreateNetworkBE( int argc, char* argv[] )
{
    mrn_dbg_func_begin();
    char* myhostname = NULL;
    Rank myrank;
    Port myport;
    int mysocket;
    char* phostname = NULL;
    Port pport=UnknownPort;
    Rank prank=UnknownRank;
    int num_children;

   	//debug
    int k;
    mrn_dbg(1, mrn_printf(FLF, stderr, "argc in CreateNetworkBE = %d\n", argc));
    for (k = 0; k< argc; k++){
        mrn_dbg(1, mrn_printf(FLF, stderr, "argv[%d] = %s\n", k, argv[k]));
    }

    Network* net = new LIBINetwork();
    ((LIBINetwork*)net)->get_parameters( argc, argv, false, myhostname, myport, myrank, mysocket, phostname, pport, prank, num_children );
    if( !myhostname || prank == UnknownRank || pport == UnknownPort )
        return NULL;

    net->init_BackEnd(phostname, pport, prank, myhostname, myrank );
	mrn_dbg_func_end();
    return net;
}

Network*
Network::CreateNetworkIN( int argc, char* argv[] )
{
	mrn_dbg_func_begin();
    char* myhostname = NULL;
    Rank myrank;
    Port myport;
    int mysocket = -1;
    char* phostname = NULL;
    Port pport=UnknownPort;
    Rank prank=UnknownRank;
    int num_children;

   //debug
    int k;
    mrn_dbg(1, mrn_printf(FLF, stderr, "argc in CreateNetworkIN = %d\n", argc));
    for (k = 0; k< argc; k++){
        mrn_dbg(1, mrn_printf(FLF, stderr, "argv[%d] = %s\n", k, argv[k]));
    }

    Network* net = new LIBINetwork();
    ((LIBINetwork*)net)->get_parameters( argc, argv, true,
                    myhostname, myport, myrank,
                    mysocket,
                    phostname, pport, prank,
                    num_children );
    mrn_dbg( 2, mrn_printf(FLF, stderr, "host %s rank %i using socket %i on port %i ...\n", myhostname, myrank, mysocket, myport ) );

    if( !myhostname || prank == UnknownRank || pport == UnknownPort )
        return NULL;

    net->init_InternalNode( phostname, pport, prank, myhostname, myrank,
                            mysocket, myport );


    InternalNode* in = net->get_LocalInternalNode();

    mrn_dbg( 2, mrn_printf(FLF, stderr, "expecting %i children ...\n", num_children ) );
    in->set_numChildrenExpected( num_children );

    mrn_dbg( 2, mrn_printf(FLF, stderr, "Waiting for subtrees to report ...\n") );
    if( ! in->waitfor_SubTreeInitDoneReports() )
        mrn_dbg( 1, mrn_printf(FLF, stderr, "waitfor_SubTreeReports() failed\n") );
    else
        mrn_dbg( 2, mrn_printf(FLF, stderr, "All subtrees reported.\n") );

    //must send reports upwards
    if( net->get_LocalChildNode()->send_SubTreeInitDoneReport() == -1 )
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                               "send_SubTreeInitDoneReport() failed\n") );
	mrn_dbg_func_end();
    return net;
}

FrontEndNode*
LIBINetwork::CreateFrontEndNode( Network* n,
                                std::string ihost,
                                Rank irank )
{
    return new FrontEndNode( n, ihost, irank );
}

BackEndNode*
LIBINetwork::CreateBackEndNode(Network* inetwork,
                                std::string imy_hostname, 
                                Rank imy_rank,
                                std::string iphostname, 
                                Port ipport, 
                                Rank iprank )
{
    return new BackEndNode( inetwork,
                               imy_hostname,
                               imy_rank,
                               iphostname,
                               ipport,
                               iprank );
}

InternalNode*
LIBINetwork::CreateInternalNode( Network* inetwork,
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

    assert( listeningPort != UnknownPort );
    return new InternalNode( inetwork,
                                imy_hostname,
                                imy_rank,
                                iphostname,
                                ipport,
                                iprank,
                                listeningSocket,
                                listeningPort );
}
//----------------------------------------------------------------------------
// LIBINetwork methods

/* Instantiate is called by Network::init_FrontEnd
 * This is the first significant divergence from 
 * the RSHNetwork class.  Particularly the bootstrapping
 */

bool
LIBINetwork::Instantiate( ParsedGraph* _parsed_graph,
                         const char* mrn_commnode_path,
                         const char* ibackend_exe,
                         const char** ibackend_args,
                         unsigned int backend_argc,
                         const std::map< std::string, std::string >* /* iattrs */ )

{
    mrn_dbg_func_begin();
    bool have_backends = ( strlen(ibackend_exe) != 0 );
    std::string sg = _parsed_graph->get_SerializedGraphString( have_backends );

    NetworkTopology* nt = get_NetworkTopology();
    
    if( nt != NULL ) {
        nt->reset( sg, false );
        NetworkTopology::Node* localnode = nt->find_Node( get_LocalRank() );
        localnode->set_Port( get_LocalPort() );
    }

    FrontEndNode* fe = get_LocalFrontEndNode();
    assert( fe != NULL );

    fe->set_numChildrenExpected( nt->get_Root()->get_NumChildren() );

    mrn_dbg( 2, mrn_printf(FLF, stderr, "Instantiating network ... \n") );

	// Just creates the libi sessions data type
    if( LIBI_fe_init( LIBI_VERSION ) != LIBI_OK ){
        mrn_dbg( 1, mrn_printf(FLF, stderr, "Failed to init LIBI.\n") );
        return false;
    }

    int num_dists = 0;
    // Two proc_dist one for MW one for BE's
    proc_dist_req_t proc_distribution[2];
    libi_sess_handle_t mw_sess;
    libi_sess_handle_t be_sess;
    host_dist_t* mw_hosts = NULL;
    host_dist_t* be_hosts = NULL;
    bool launchBE = have_backends;
    bool launchMW = false;
    libi_env_t* mrnet_env = NULL;

    host_dist_t** mw_tail = &mw_hosts;
    host_dist_t** be_tail = &be_hosts;

    // Populates each node in mw_tail, be_tail with some basic info:
    // hostname, nproc, next 
	CreateHostDistributions( nt->get_Root(), true, !launchBE, mw_tail, be_tail );

    if( _network_settings.size() > 0 ){
        mrnet_env = (libi_env_t*)malloc( sizeof( libi_env_t ) );
        libi_env_t* cur = mrnet_env;
        std::map< net_settings_key_t, std::string >::iterator _settings_iter = _network_settings.begin();
        while( _settings_iter != _network_settings.end() ){
            cur->name = strdup( get_NetSettingName( _settings_iter->first ).c_str() );
            cur->value = strdup( _settings_iter->second.c_str() );
            _settings_iter++;
            if( _settings_iter != _network_settings.end() ){
                cur->next = (libi_env_t*)malloc( sizeof( libi_env_t ) );
                cur = cur->next;
            }
            else
                cur->next = NULL;
        }
    }

	/* Each call to LIBI_fe_createSession creates a new libi process group object
	 * Addtionally the pointers to latest are updated
	 */ 

    //mw
    if( mw_hosts != NULL ){
        launchMW = true;
        if( LIBI_fe_createSession( mw_sess ) != LIBI_OK ){
            mrn_dbg( 1, mrn_printf(FLF, stderr, "Failed to create session.\n") );
            return false;
        }

        proc_distribution[num_dists].sessionHandle = mw_sess;
        proc_distribution[num_dists].proc_path = strdup( mrn_commnode_path );
        proc_distribution[num_dists].proc_argv = NULL;
        proc_distribution[num_dists].hd = mw_hosts;
        num_dists++;

        if( mrnet_env != NULL){
            if( LIBI_fe_addToSessionEnvironment( mw_sess, mrnet_env ) != LIBI_OK ){
                mrn_dbg( 1, mrn_printf(FLF, stderr, "Failed to add environment to MW session.\n") );
            }
        }
    }

    //be
    if( launchBE ){
        if( LIBI_fe_createSession( be_sess ) != LIBI_OK ){
            mrn_dbg( 1, mrn_printf(FLF, stderr, "Failed to create session.\n") );
            return false;
        }
        proc_distribution[num_dists].sessionHandle = be_sess;
        proc_distribution[num_dists].proc_path = strdup( ibackend_exe );
        proc_distribution[num_dists].proc_argv = NULL;  //TODO: create null term char**
        proc_distribution[num_dists].hd = be_hosts;
        num_dists++;

        if( mrnet_env != NULL){
            if( LIBI_fe_addToSessionEnvironment( be_sess, mrnet_env ) != LIBI_OK ){
                mrn_dbg( 1, mrn_printf(FLF, stderr, "Failed to add environment to BE session.\n") );
            }
        }
    }

	// At this point proc_distribution just has the hostname in .hd and ibackend_exe in .proc_path
	// Within the LIBI_fe_launch method we need to be launching a BE with 4 MRNet arguments
    if( num_dists > 0 ){
        mrn_dbg( 2, mrn_printf(FLF, stderr, "LIBI launch\n") );
        if( LIBI_fe_launch( proc_distribution, num_dists ) != LIBI_OK ){
            mrn_printf(FLF, stderr, "Failed to launch procs.\n");
            return false;
        }
	    mrn_dbg( 1, mrn_printf(FLF, stderr, "Passed the LIBI_fe_launch.\n") );	
    }
    void* parents;
    int parents_size = sizeof(Rank) + sizeof(Port);

    parents = malloc( parents_size );
    char* cur = (char*)parents;
    Rank r = get_LocalRank();
    Port p = get_LocalPort();
    memcpy( (void*)cur, (void*)&r, sizeof(Rank) );
    cur += sizeof(Rank);
    memcpy( (void*)cur, (void*)&p, sizeof(Port) );

	// This is where I believe the MRNet topology is passed to nodes and they form connections.
	mrn_dbg( 1, mrn_printf(FLF, stderr, "Right before bootstrap_comm... \n") );
    if( launchMW ){
        mrn_dbg( 2, mrn_printf(FLF, stderr, "LIBI MW communication\n") );
        if( !bootstrap_communication_distributed( mw_sess, nt, true, launchBE, parents, parents_size ) ){
            mrn_dbg( 1, mrn_printf(FLF, stderr, "Failed to communicate with MW.\n") );
            return false;
        }
    }
    if( launchBE ){
        mrn_dbg( 2, mrn_printf(FLF, stderr, "LIBI BE communication\n") );
        if( !bootstrap_communication_distributed( be_sess, nt, false, launchBE, parents, parents_size ) ){
            mrn_dbg( 1, mrn_printf(FLF, stderr, "Failed to communicate with BE.\n") );
            return false;
        }
    }

    mrn_dbg( 2, mrn_printf(FLF, stderr, "MRNet instantiated\n") );
	mrn_dbg_func_end();
    return true;
}

void
LIBINetwork::get_parameters( int argc, char* argv[], bool isForMW,
                     char* &myhostname, Port &myport, Rank &myrank,
                     int &mysocket,
                     char* &phostname, Port &pport, Rank &prank,
                     int &mynumchild )
{
    int size, libi_rank, i;
    bool master = false;
    bool includeLeaves=false;
    char* cur;
    void* parents;
    int parents_size;
    int mapping_size = sizeof(Rank) + sizeof(Port);
    char* topo;
    int topo_size;
    SerialGraph* sg;

    //debug
    int k;
    mrn_dbg(1, mrn_printf(FLF, stderr, "argc in LIBI_init = %d\n", argc));
    for (k = 0; k< argc; k++){
        mrn_dbg(1, mrn_printf(FLF, stderr, "argv[%d] = %s\n", k, argv[k]));
    }

    //initialize LIBI
    char* type = "MW";
    if( ! isForMW )
        type = "BE";

    if( !isForMW && argc >= 6 ) {
        //backend attach case, may need to rethink what we use to identify this
        phostname = argv[argc-5];
		pport = (Port)strtoul( argv[argc-4], NULL, 10 );
		prank = (Rank)strtoul( argv[argc-3], NULL, 10 );
		myhostname = argv[argc-2];
		myrank = (Rank)strtoul( argv[argc-1], NULL, 10 );

        if( pport >= MIN_DYNAMIC_PORT ){
            if( prank != myrank ){
                mrn_dbg(2, mrn_printf(FLF, stderr, "Using args for backend attach mode \n" ) );
                return;
            } else
                mrn_dbg(2, mrn_printf(FLF, stderr, "myrank = prank = %i \n", prank ) );
        } else
            mrn_dbg(2, mrn_printf(FLF, stderr, "non-dynamic port %i (%s) \n", pport, argv[argc-4] ) );
    }

    if( LIBI_init( LIBI_VERSION , &argc, &argv) != LIBI_OK ){
        mrn_printf(FLF, stderr, "Failure: unable to initialize LIBI\n");
        return;
    }

    if( LIBI_getSize(&size) != LIBI_OK ){
        mrn_dbg(1, mrn_printf(FLF, stderr, "Failure: unable to get size\n") );
        return;
    }
    if( LIBI_getMyRank(&libi_rank) != LIBI_OK ){
        mrn_dbg(1, mrn_printf(FLF, stderr, "Failure: unable to get size\n") );
        return;
    }
    if( LIBI_amIMaster() == LIBI_YES )
        master = true;


    if( LIBI_recvUsrData( (void**)&topo, &topo_size ) != LIBI_OK ){
        mrn_dbg(1, mrn_printf(FLF, stderr, "Failure: unable to receive mapping\n") );
        return;
    }

    if( LIBI_broadcast( &topo_size, sizeof(int) ) ){
        mrn_dbg(1, mrn_printf(FLF, stderr, "Failure: unable to broadcast\n") );
        return;
    }
    if( !master )
        topo = (char*)malloc( topo_size );
    if( LIBI_broadcast( topo, topo_size ) ){
        mrn_dbg(1, mrn_printf(FLF, stderr, "Failure: unable to broadcast\n") );
        return;
    }
    if( topo[0] == 'y')
        includeLeaves=true;
    topo++;
    topo_size--;

    void* hbuf;
    int hbuf_size;
    if( LIBI_recvUsrData( (void**)&hbuf, &hbuf_size ) != LIBI_OK ){
        mrn_dbg(1, mrn_printf(FLF, stderr, "Failure: unable to receive mapping\n") );
        return;
    }
    if( LIBI_broadcast( &hbuf_size, sizeof(int) ) ){
        mrn_dbg(1, mrn_printf(FLF, stderr, "Failure: unable to broadcast\n") );
        return;
    }
    if( !master )
        hbuf = (char*)malloc( hbuf_size );
    if( LIBI_broadcast( hbuf, hbuf_size ) ){
        mrn_dbg(1, mrn_printf(FLF, stderr, "Failure: unable to broadcast\n") );
        return;
    }

    //For multiple procs per host, the LIBI rank order determines the MRNet rank order
    //find out how many procs on the same host are before this proc
    char** hosts = (char**)malloc( (libi_rank + 1) * sizeof(char*) );
    cur = (char*)hbuf;
    for( i = 0; i <= libi_rank; i++){
        hosts[i] = cur;
        cur += strlen(cur)+1;
    }
    myhostname = hosts[libi_rank];
    int n = 1;
    for( i = 0; i < libi_rank; i++){
        if( strcmp(myhostname, hosts[i])==0 )
            n++;
    }

    //find the nth proc on the same host in the mrnet topology
    mrn_dbg(2, mrn_printf(FLF, stderr, "%s libi_rank:%u hostname:%s n:%i includeLeaves:%i\n", type, libi_rank, myhostname, n, includeLeaves) );
    sg = new SerialGraph( topo );
    myrank = UnknownRank;
    get_Identity( sg, n, myhostname, myrank, mynumchild, phostname, prank, isForMW, includeLeaves, false, true );
    if( myrank == UnknownRank ){
        string net_hostname;
        XPlat::NetUtils::GetNetworkName( string(myhostname), net_hostname );
        get_Identity( sg, n, net_hostname.c_str(), myrank, mynumchild, phostname, prank, isForMW, includeLeaves, true, true );

        if( myrank == UnknownRank ){
            mrn_dbg(1, mrn_printf(FLF, stderr, "Failure: unable to find my rank. proc number %i on host %s.\n", 1, myhostname) );
            return;
        } else {
            myhostname = strdup( net_hostname.c_str() );
        }
    }
    mrn_dbg(2, mrn_printf(FLF, stderr, "%s myhostname:%i myrank:%i, mynumchild:%i, phostname:%s prank:%u\n", type, myhostname, myrank, mynumchild, phostname, prank) );

    myport = UnknownPort;
    mysocket = -1;
    if( isForMW ){
        void* root;
        int root_size;

        //receive the root's rank and port
        if( LIBI_recvUsrData( (void**)&root, &root_size ) != LIBI_OK ){
            mrn_dbg(1, mrn_printf(FLF, stderr, "Failure: unable to receive mapping\n") );
            return;
        }

        Port temp_port = UnknownPort;
        int bRet = bindPort( &mysocket, &temp_port, true ); //non-blocking
        if( bRet == -1 ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "Failure: unable to bind socket\n") );
            return;
        }
        myport = temp_port;
        //mrn_dbg(2, mrn_printf(FLF, stderr, "myrank:%u MW port:%i\n", myrank, myport) );

        if( master ){
            parents_size = mapping_size * (size+1);
            parents = malloc( parents_size );
        }
        void* mymapping = malloc( mapping_size );
        cur = (char*)mymapping;
        memcpy( (void*)cur, (void*)&myrank, sizeof(Rank) );
        cur += sizeof(Rank);
        memcpy( (void*)cur, (void*)&myport, sizeof(Port) );

        if( LIBI_gather( mymapping, mapping_size, parents ) != LIBI_OK ){
            mrn_dbg(1, mrn_printf(FLF, stderr, "Failure: unable to gather ports.\n") );
            return;
        }
        if( master ){
            cur = (char*)parents;
            for( i = 0; i <= size; i++ ){
                if( i == size )
                    memcpy( (void*)cur, root, root_size );
                Rank r = *(Rank*)cur;
                cur += sizeof(Rank);
                Port p = *(Port*)cur;
                cur += sizeof(Port);
                mrn_dbg(2, mrn_printf(FLF, stderr, "libi_rank:%i mrnet_rank:%u port:%u.\n", i, r, p) );
            }
        }
        if( LIBI_sendUsrData( parents, &parents_size ) != LIBI_OK ){
            mrn_dbg(1, mrn_printf(FLF, stderr, "Failure: unable to send port list\n") );
            return;
        }
    } else {
        if( LIBI_recvUsrData( (void**)&parents, &parents_size ) != LIBI_OK ){
            mrn_dbg(1, mrn_printf(FLF, stderr, "Failure: unable to receive mapping\n") );
            return;
        }
    }

    if( LIBI_broadcast( &parents_size, sizeof(int) ) ){
        mrn_dbg(1, mrn_printf(FLF, stderr, "Failure: unable to broadcast\n") );
        return;
    }
    if( !master )
        parents = malloc( parents_size );
    if( LIBI_broadcast( parents, parents_size ) ){
        mrn_dbg(1, mrn_printf(FLF, stderr, "Failure: unable to broadcast\n") );
        return;
    }

    cur = (char*)parents;
    pport = UnknownPort;
    for( i = 0; ( i < ( parents_size / mapping_size ) ) && ( pport == UnknownPort ); i++){
        if( prank == *(Rank*)cur ){
            cur += sizeof(Rank);
            pport = *(Port*)cur;
        } else
            cur += mapping_size;
    }
    if( pport == UnknownPort )
        mrn_dbg(1, mrn_printf(FLF, stderr, "Failure: unable to find parent's port. my rank:%u parent rank:%u\n", myrank, prank) );

    char* done = "a";
    int asdf =2;
    if( LIBI_sendUsrData( (void*)done, &asdf ) != LIBI_OK ){
        mrn_dbg(1, mrn_printf(FLF, stderr, "Failure: unable to send done message list\n") );
        return;
    }

    LIBI_finalize();

    mrn_dbg( 2, mrn_printf(FLF, stderr, "%s phostname:%s pport:%i prank:%i myrank:%i children:%u \n", type, phostname, pport, prank, myrank, mynumchild) );
}


// This relays the MRNet topology information and allows nodes to connect back to their MRNet topology parents.
bool
LIBINetwork::bootstrap_communication_distributed( libi_sess_handle_t sess,
                              NetworkTopology* nt,
                              bool isForMW,
                              bool launchBE,
                              void* & ports,
                              int & ports_size){
    char** hostlist;
    unsigned int sess_size;
    char* topo;
    char* cur;
    void* hbuf;
    int hbuf_size=0;
    unsigned int i;
    int n = 0;

    char* type = "BE";
    bool includeLeaves = true;
    if( isForMW ){
        type = "MW";
        includeLeaves = !launchBE;
    }

    //send topology first
    int topo_size = nt->get_TopologyString().size() + 2;
    topo = (char*)malloc( topo_size );
    if( includeLeaves )
        topo[0] = 'y';
    else
        topo[0] = 'n';
    strcpy( &topo[1], nt->get_TopologyString().c_str() );
    mrn_dbg(2, mrn_printf(FLF, stderr, "topology:%s\n" , &topo[1] ) );
    if( LIBI_fe_sendUsrData( sess, (void*)topo, topo_size ) != LIBI_OK ){
        mrn_dbg(1, mrn_printf(FLF, stderr, "Failed to send %s topology.\n" , type) );
        return false;
    }

    if( LIBI_fe_getHostlist( sess, &hostlist, &sess_size ) != LIBI_OK ){
         mrn_dbg(1, mrn_printf(FLF, stderr, "Failed to get the %s host list.\n", type) );
        return false;
    }
    //calculate host buf size
    for(i = 0; i<sess_size; i++){
        hbuf_size += strlen( hostlist[i] ) + 1;
        mrn_dbg(2, mrn_printf(FLF, stderr, "host[%i]=%s\n", i, hostlist[i]) );
    }
    //create hostbuf
    hbuf = malloc( hbuf_size );
    cur = (char*)hbuf;
    for(i = 0; i < sess_size; i++){
        strcpy( cur, hostlist[i] );
        cur += strlen( cur ) + 1;
    }
    if( LIBI_fe_sendUsrData( sess, hbuf, hbuf_size ) != LIBI_OK ){
        mrn_dbg(1, mrn_printf(FLF, stderr, "Failed to send %s host buffer.\n" , type) );
        return false;
    }

    //send the port & mrnet ranks (either root or root + MW)
    if( LIBI_fe_sendUsrData( sess, ports, ports_size) != LIBI_OK ){
        mrn_dbg(1, mrn_printf(FLF, stderr, "Failed to send %s port list.\n", type) );
        return false;
    }
    if( isForMW ){
        free(ports);
        ports=NULL;

        //receive a list of port & mrnet ranks
        if( LIBI_fe_recvUsrData( sess, &ports, &ports_size ) ){
            mrn_dbg(1, mrn_printf(FLF, stderr, "Failed to receive %s port list.\n" , type) );
            return false;
        }
        mrn_dbg(2,
            cur = (char*)ports;
            Rank r;
            Port p;
            for( i = 0; i < sess_size; i++ ){
                r = *(Rank*)cur;
                cur += sizeof(Rank);
                p = *(Port*)cur;
                cur += sizeof(Port);
                mrn_printf(FLF, stderr, "%s rank:%u port:%u\n", type, r, p);
            }
        );
    }

    char* done = "x";
    n=0;
    if( LIBI_fe_recvUsrData( sess, (void**)&done, &n ) ){
        mrn_dbg(1, mrn_printf(FLF, stderr, "Failed to receive %s port list.\n" , type) );
        return false;
    }

    return true;
}
// constructor

LIBINetwork::LIBINetwork()
    : Network()
{
	// Nothing to see here, just call the default Network constructor.
}

} // namespace MRN

