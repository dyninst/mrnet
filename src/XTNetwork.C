/******************************************************************************
 *  Copyright © 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                   Detailed MRNet usage rights in "LICENSE" file.           *
 ******************************************************************************/

#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#ifndef os_windows
#include <unistd.h>
#include <sys/wait.h>
#endif

#include "config.h"
#include "utils.h"
#include "SerialGraph.h"
#include "XTNetwork.h"
#include "XTFrontEndNode.h"
#include "XTBackEndNode.h"
#include "XTInternalNode.h"

#include "mrnet/MRNet.h"
#include "xplat/Process.h"
#include "xplat/SocketUtils.h"

extern "C"
{
#ifdef HAVE_LIBALPS_H
#include "libalps.h"
#endif
}

namespace MRN
{

Port XTNetwork::defaultTopoPort = 26500;


//----------------------------------------------------------------------------
// base class factory methods
// creates objects of our specialization
Network*
Network::CreateNetworkFE( const char * itopology,
			  const char * ibackend_exe,
			  const char **ibackend_argv,
			  const std::map<std::string,std::string>* iattrs,
			  bool irank_backends,
			  bool iusing_mem_buf )
{
    mrn_dbg_func_begin();

    Network* n = new XTNetwork;
    n->init_FrontEnd( itopology,
                      ibackend_exe,
                      ibackend_argv,
                      iattrs,
                      irank_backends,
                      iusing_mem_buf );
    return n;
}


Network*
Network::CreateNetworkBE( int argc, char** argv )
{
    mrn_dbg_func_begin();

    // Get MRNet args at end of BE args
    if( argc >= 6 ) {
        const char * phostname = argv[argc-5];
        Port pport = (Port) strtoul( argv[argc-4], NULL, 10 );
        Rank prank = (Rank) strtoul( argv[argc-3], NULL, 10 );
        const char * myhostname = argv[argc-2];
        Rank myrank = (Rank) strtoul( argv[argc-1], NULL, 10 );

        Network * net = new XTNetwork();
        net->init_BackEnd( phostname, pport, prank, myhostname, myrank );
	return net;
    }
    else {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Unexpected number of BE args = %d\n", argc));
        return NULL;
    }
}


Network*
Network::CreateNetworkIN( int argc, char** argv )
{
    mrn_dbg_func_begin();

    int topoPipeFd = -1;
    int beArgc = 0;
    char** beArgv = NULL;
    if( argc > 0 ) {
        if( strcmp(argv[0], "-T") == 0 ) {
            // we are not the first process on this node
            // we are being passed the FD of a pipe from which we 
            // can read the topology
            topoPipeFd = (int)strtol( argv[1], NULL, 10 );
            beArgc = argc - 2;
            beArgv = argv + 2;
        }
        else {
            // just pass along BE information
            beArgc = argc;
            beArgv = argv;
        }
    }

    return new XTNetwork( true, topoPipeFd, beArgc, beArgv );
}


//----------------------------------------------------------------------------
// XTNetwork methods

// FE and BE constructor
XTNetwork::XTNetwork( void )
{
    set_LocalHostName( GetNodename() );
}

SerialGraph*
XTNetwork::GetTopology( int topoSocket, Rank& myRank )
                        
{
    mrn_dbg_func_begin();

    char* sTopology = NULL;
    size_t sTopologyLen = 0;

    // obtain topology from our parent
    read( topoSocket, &sTopologyLen, sizeof(sTopologyLen) );
    mrn_dbg(5, mrn_printf(FLF, stderr, "read topo len=%d\n", (int)sTopologyLen ));

    sTopology = new char[sTopologyLen + 1];
    char* currBufPtr = sTopology;
    size_t nRemaining = sTopologyLen;
    while( nRemaining > 0 ) {
        ssize_t nread = read( topoSocket, currBufPtr, nRemaining );
        nRemaining -= nread;
        currBufPtr += nread;
    }
    *currBufPtr = 0;

    // get my rank
    read( topoSocket, &myRank, sizeof(myRank) );

    mrn_dbg(5, mrn_printf(FLF, stderr, "read topo=%s, rank=%d\n", 
                          sTopology, (int)myRank ));

    SerialGraph* sg = new SerialGraph( sTopology );
    delete[] sTopology;

    return sg;
}

int
XTNetwork::PropagateTopologyOffNode( Port topoPort, 
                                     SerialGraph* mySubtree,
				     SerialGraph* topology )
{
    mrn_dbg_func_begin();

    std::string childHost, thisHost;
    std::set< std::string > hostsSeen;
    thisHost = mySubtree->get_RootHostName();
    hostsSeen.insert( thisHost );
    mrn_dbg(5, mrn_printf(FLF, stderr, "sending topology from %s:%d\n",
                          thisHost.c_str(), mySubtree->get_RootRank() ) );

    // serialize the topology
    std::string sTopology = topology->get_ByteArray();

    // propagate topology to our off-node child processes
    mySubtree->set_ToFirstChild();
    SerialGraph* currChildSubtree = mySubtree->get_NextChild();
    for( ; currChildSubtree != NULL; currChildSubtree = mySubtree->get_NextChild() ) {

        childHost = currChildSubtree->get_RootHostName();
        Rank childRank = currChildSubtree->get_RootRank();

        if( hostsSeen.find(childHost) == hostsSeen.end() ) {
            
            if( ClosestToRoot( topology, childHost, childRank ) ) {

                // connect to child's topology propagation socket
                mrn_dbg(5, mrn_printf(FLF, stderr, "sending topology to %s:%d\n", 
                                      childHost.c_str(), childRank ));

                int childTopoSocket = -1;
                int cret = connectHost( &childTopoSocket, childHost.c_str(), topoPort, 10 );
                if( cret != 0 ) {
                    // TODO how to handle this error?
                    mrn_dbg(1, mrn_printf(FLF, stderr, 
                                          "Failure: unable to connect to %s:%d to send topology\n", 
                                          childHost.c_str(), childRank ) );
                    continue;
                }
                PropagateTopology( childTopoSocket, topology, childRank );
                XPlat::SocketUtils::Close( childTopoSocket );
            }
            hostsSeen.insert( childHost );
        }
    }

    return 0;
}

void
XTNetwork::PropagateTopology( int topoFd, 
			      SerialGraph* topology,
                              Rank childRank )
{
    mrn_dbg_func_begin();

    std::string sTopology = topology->get_ByteArray();
    size_t sTopologyLen = sTopology.length();

    mrn_dbg(5, mrn_printf(FLF, stderr, "sending topology=%s, rank=%d\n", 
                          sTopology.c_str(), (int)childRank ));

    // send serialized topology size
    ssize_t nwritten = write( topoFd, &sTopologyLen, sizeof(sTopologyLen) );

    // send the topology itself
    // NOTE this code assumes the byte array underneath the std::string
    // remains valid throughout this function
    size_t nRemaining = sTopologyLen;
    const char* currBufPtr = sTopology.c_str();
    while( nRemaining > 0 ) {
        nwritten = write( topoFd, currBufPtr, nRemaining );
        nRemaining -= nwritten;
        currBufPtr += nwritten;
    }

    // deliver the child rank
    write( topoFd, &childRank, sizeof(childRank) );
}

// IN constructor
XTNetwork::XTNetwork( bool, int topoPipeFd, 
		      int beArgc,
		      char** beArgv )
{
    mrn_dbg_func_begin();

    // ensure we know our node's hostname
    set_LocalHostName( GetNodename() );

    int topoFd = -1;
    int listeningTopoSocket = -1;
    Port topoPort = FindTopoPort();
    if( topoPipeFd == -1 ) {
        // we are the first process on this node

        // set up a listening socket that our parent will connect 
        // to to provide us the topology
        if( -1 == CreateListeningSocket( listeningTopoSocket, topoPort, false ) ) {
            mrn_dbg(1, mrn_printf(FLF, stderr,
                                  "failed to create topology listening socket\n" ));
            exit( 1 );
        }
	int inout_errno;
        topoFd = getSocketConnection( listeningTopoSocket, inout_errno );
    }
    else {
        // we are not the first process on this node
        // we were forked from another process that set up a pipe
        // on which to deliver our part of the topology
        topoFd = topoPipeFd;
    }
    assert( topoFd != -1 );

    // Get the topology
    std::string myHost = get_LocalHostName();
    Rank myRank;
    SerialGraph* topology = GetTopology( topoFd, myRank );
    assert( topology != NULL );
    XPlat::SocketUtils::Close( topoFd );
    if( listeningTopoSocket != -1 )
        XPlat::SocketUtils::Close( listeningTopoSocket );

    TopologyPosition* my_tpos = NULL;

    FindPositionInTopology( topology, myHost, myRank, my_tpos );
    assert( my_tpos != NULL );
  
    const char* mrn_commnode_path = Network::FindCommnodePath();
  
    if( topoPipeFd == -1 ) { // first process on this node

        // spawn other processes on this node, if any
        std::vector< TopologyPosition* > other_procs;
        FindColocatedProcesses( topology, myHost, my_tpos, other_procs );
        if( other_procs.size() ) {

            std::vector< TopologyPosition* >::iterator pos_iter = other_procs.begin();
            for( ; pos_iter != other_procs.end() ; pos_iter++ ) {
                
                TopologyPosition* tpos = *pos_iter;
                assert( tpos != NULL );

                // check if current pos is a BE or another IN
                pid_t currPid = 0;
                if( tpos->subtree->is_RootBackEnd() && (beArgc > 0) ) {
                    
                    currPid = SpawnBE( beArgc, beArgv, tpos->parentHostname.c_str(),
                                       tpos->parentRank, tpos->parentPort,
                                       myHost.c_str(), tpos->myRank );
                    
                    if( currPid == 0 ) {
                        // we failed to spawn the process
                        // TODO how to handle the error?
                        mrn_dbg(1, mrn_printf(FLF, stderr, 
                                              "Failure: unable to spawn child %s:%d\n", 
                                              myHost.c_str(), (int)tpos->myRank ));
                        continue;
                    }
                }
                else {
                    int currTopoFd = -1;
                    currPid = SpawnIN( mrn_commnode_path, beArgc, beArgv, &currTopoFd );
                    
                    if( (currPid == 0) || (currTopoFd == -1) ) {
                        // we failed to spawn the process
                        // TODO how to handle the error?
                        mrn_dbg(1, mrn_printf(FLF, stderr, 
                                              "Failure: unable to spawn child %s:%d\n", 
                                              myHost.c_str(), (int)tpos->myRank ));
                        continue;
                    }
       
                    // deliver the full topology
                    PropagateTopology( currTopoFd, topology, tpos->myRank );
                    XPlat::SocketUtils::Close( currTopoFd );
                }

                delete tpos;
            }
            mrn_dbg(3, mrn_printf(FLF, stderr, 
                                 "done spawning co-located processes\n" ));
        }
	else
            mrn_dbg(3, mrn_printf(FLF, stderr,
                                  "no other processes on this node to spawn\n" ));    
    }
    
    // Am I really a BE?
    if( my_tpos->subtree->is_RootBackEnd() && (beArgc > 0) ) {

        // I am supposed to be a BE but currently am an IN.
        // I need to spawn a BE and deliver the topology as I received it
        // I should be the only process on this node
        assert( topoPipeFd == -1 );

        // spawn the BE process
        pid_t bePid = SpawnBE( beArgc, beArgv, my_tpos->parentHostname.c_str(),
                               my_tpos->parentRank, my_tpos->parentPort,
                               myHost.c_str(), myRank );
        if( bePid == 0 ) {
            // we failed to spawn the BE process
            // TODO is this best way to handle error?
            mrn_dbg(1, mrn_printf(FLF, stderr, 
                                  "Failure: unable to spawn child on %s\n", 
                                  (my_tpos->myHostname).c_str() ) );
            exit( 1 );
        }
        
        // we can't just go away - 
        // in case we were started by aprun, we need to stick
        // around so that it doesn't think the "application" is done.
        int beProcessStatus = 0;
        waitpid( bePid, &beProcessStatus, 0 );
        exit( 0 );
    }

    // Initialize as IN
    int listeningDataSocket = -1;
    Port listeningDataPort = my_tpos->subtree->get_RootPort();
    if( -1 == CreateListeningSocket( listeningDataSocket, listeningDataPort, true ) ) {
        mrn_dbg(1, mrn_printf(FLF, stderr,
                              "failed to create listening data socket\n" ));
        exit( 1 );
    }
    
    Network::init_InternalNode( my_tpos->parentHostname.c_str(),
                                my_tpos->parentPort,
                                my_tpos->parentRank,
                                myHost.c_str(),
                                myRank,
                                listeningDataSocket,
                                listeningDataPort );

    // Tell local ParentNode how many children to expect
    XTInternalNode* in = dynamic_cast<XTInternalNode*>( get_LocalInternalNode() );
    assert( in != NULL );
    in->init_numChildrenExpected( *(my_tpos->subtree) );
    mrn_dbg(5, mrn_printf(FLF, stderr, "expecting %u children\n", 
                          in->get_numChildrenExpected() ));

    if( ! my_tpos->subtree->is_RootBackEnd() ) {
        // propagate full topology to any off-node children
        PropagateTopologyOffNode( topoPort, my_tpos->subtree, topology );
    }
    delete my_tpos;

    in->PropagateSubtreeReports();
}


pid_t
XTNetwork::SpawnIN( const char* mrn_commnode_path,
                    int beArgc, 
                    char** beArgv,
                    int* topoFd )
{
    mrn_dbg_func_begin();

    pid_t ret = 0;
    *topoFd = -1;

    assert( mrn_commnode_path != NULL );

    // build the pipe for communicating topology
    int topoFds[2];
    pipe( topoFds );

    // we can't use the XPlat::Process::Create function here
    // because we want to close the unused ends of the pipe
    // after the fork().
    pid_t pid = fork();
    if( pid > 0 ) {
        // we are the parent
        // close our read end of the pipe
        mrn_dbg(3, mrn_printf( FLF, stderr, "parent closing %d, returning %d\n", topoFds[0], topoFds[1] ));
        XPlat::SocketUtils::Close( topoFds[0] );

        // return the write end of the pipe
        *topoFd = topoFds[1];
        ret = pid;
    }
    else if( pid == 0 ) {
        // we are the child

        mrn_dbg(3, mrn_printf( FLF, stderr, "child closing %d\n", topoFds[1] ));
        XPlat::SocketUtils::Close( topoFds[1] );

        // we need to become another IN process
        // we also need to pass the fd of the topology 
        // pipe on the command line
        int argc = beArgc + 3;
        char** argv = new char*[argc + 3];

        int currIdx = 0;
        argv[currIdx++] = strdup( mrn_commnode_path );
        argv[currIdx++] = strdup( "-T" );

        std::ostringstream fdStr;
        fdStr << topoFds[0];
        argv[currIdx++] = strdup( fdStr.str().c_str() );

        for( int i = 0; i < beArgc; i++ ) {
            argv[currIdx++] = beArgv[i];
        }
        argv[currIdx] = NULL;        

        mrn_dbg(5, mrn_printf(FLF, stderr, "spawning another IN with arguments:\n" ));
        for( int i = 0; i < beArgc + 3; i++ ) {
            mrn_dbg(5, mrn_printf(FLF, stderr, "IN argv[%d]=%s\n", i, argv[i] ));
        }

        execvp( argv[0], argv );
        mrn_dbg(1, mrn_printf(FLF, stderr, "exec of colocated IN failed\n" ));
        exit( 1 );
    }
    else {
        mrn_dbg(1, mrn_printf(FLF, stderr, "fork of colocated IN failed\n" ));
        *topoFd = -1;
        ret = 0;
    }
    return ret;
}

pid_t
XTNetwork::SpawnBE( int beArgc, char** beArgv, const char* parentHost, 
                    Rank parentRank, Port parentPort, 
                    const char* childHost, Rank childRank )
{
    mrn_dbg_func_begin();

    char pport[16], prank[16], crank[16]; 
    pid_t ret = 0;

    assert( beArgc > 0 );
    assert( beArgv != NULL );

    sprintf(pport, "%d", (int)parentPort);
    sprintf(prank, "%d", (int)parentRank);
    sprintf(crank, "%d", (int)childRank);

    // we can't use the XPlat::Process::Create function here
    // because we need the pid to wait on.
    pid_t pid = fork();
    if( pid > 0 ) {
        // we are the parent
        ret = pid;
    }
    else if( pid == 0 ) {
        // we are the child
        // we also need to pass the connection info
        int argc = beArgc + 5;
        char** argv = new char*[argc + 1];

        int currIdx = 0;
        for( int i = 0; i < beArgc; i++ ) {
            argv[currIdx++] = beArgv[i];
        }
        argv[currIdx++] = strdup(parentHost);
        argv[currIdx++] = pport;
	argv[currIdx++] = prank;
        argv[currIdx++] = strdup(childHost);
	argv[currIdx++] = crank;
        argv[currIdx] = NULL;

        mrn_dbg(5, mrn_printf(FLF, stderr, "spawning BE with arguments:\n" ));
        for( int i = 0; i < argc; i++ ) {
	    mrn_dbg(5, mrn_printf(FLF, stderr, "BE argv[%d]=%s\n", i, argv[i] ));
        }

        execvp( argv[0], argv );
        mrn_dbg(1, mrn_printf(FLF, stderr, "exec of BE failed\n" ));
        exit( 1 );
    }
    else {
        mrn_dbg(1, mrn_printf(FLF, stderr, "fork of BE failed\n" ));
        ret = 0;
    }

    return ret;
}

Port
XTNetwork::FindTopoPort( void )
{
    Port topoPort = UnknownPort;

    char* topoPortStr = getenv( "MRNET_PORT" );
    if( topoPortStr != NULL )
         topoPort = (Port)strtoul( topoPortStr, NULL, 10 );
    else
         topoPort = defaultTopoPort;

    return topoPort;
}

Port
XTNetwork::FindParentPort( void )
{
    Port ret = FindTopoPort();
    assert( ret != UnknownPort );
    ret++;
    return ret;
}


SerialGraph* FindClosestHostToRoot( const std::string& host, SerialGraph* topology )
{
    // check my children
    topology->set_ToFirstChild();
    SerialGraph* currChildSubtree = topology->get_NextChild();
    for( ; currChildSubtree != NULL ; currChildSubtree = topology->get_NextChild() ) {
        if( strcmp(currChildSubtree->get_RootHostName().c_str(), host.c_str()) == 0 ) {
            return currChildSubtree;
        }
    }

    // recursively check my children's descendants
    topology->set_ToFirstChild();
    currChildSubtree = topology->get_NextChild();
    for( ; currChildSubtree != NULL ; currChildSubtree = topology->get_NextChild() ) {
        if( ! currChildSubtree->is_RootBackEnd() ) {
            SerialGraph* found = FindClosestHostToRoot( host, currChildSubtree );
            if( found != NULL )
                return found;
        }
    }
    
    return NULL;
}

bool
XTNetwork::ClosestToRoot( SerialGraph* topology,
                          const std::string& childhost, Rank childrank )
{
    mrn_dbg_func_begin();

    // identify process on same host that is closest to topology root
    SerialGraph* firsthost = FindClosestHostToRoot( childhost, topology );
    if( firsthost == NULL )
        return false;

    else if( firsthost->get_RootRank() == childrank ) {
        mrn_dbg( 5, mrn_printf(FLF, stderr, "found my child %s:%d as closest\n", 
                               childhost.c_str(), childrank) );
        return true;
    }
    else {
        mrn_dbg( 5, mrn_printf(FLF, stderr, "closest %s:%d is not my child\n", 
                               childhost.c_str(), firsthost->get_RootRank() ) );
        return false;
    }
}

void
XTNetwork::FindPositionInTopology( SerialGraph* topology,
                                   const std::string& myHost, Rank myRank,
                                   XTNetwork::TopologyPosition*& myPos )
{
    mrn_dbg_func_begin();

    topology->set_ToFirstChild();
    SerialGraph* currChildSubtree = topology->get_NextChild();
    for( ; currChildSubtree != NULL ; currChildSubtree = topology->get_NextChild() ) {
        // check the hostname associated with the root of the current subtree
        if( (strcmp(currChildSubtree->get_RootHostName().c_str(), myHost.c_str()) == 0) &&
            (currChildSubtree->get_RootRank() == myRank) ) {
            TopologyPosition* tpos = new TopologyPosition;
            tpos->myHostname = myHost;
            tpos->myRank = myRank;
            tpos->subtree = currChildSubtree;
            tpos->parentHostname = topology->get_RootHostName();
            tpos->parentRank = topology->get_RootRank();
            tpos->parentPort = topology->get_RootPort();
            tpos->parSubtree = topology;
            myPos = tpos;
	    return;
        }
        else if( ! currChildSubtree->is_RootBackEnd() )
            FindPositionInTopology( currChildSubtree, myHost, myRank, myPos );
    }
}

void
XTNetwork::FindColocatedProcesses( SerialGraph* topology, 
                                   const std::string& myHost,
                                   const XTNetwork::TopologyPosition* myPos, 
                                   std::vector< XTNetwork::TopologyPosition* >& procs )
{
    mrn_dbg_func_begin();

    topology->set_ToFirstChild();
    SerialGraph* currChildSubtree = topology->get_NextChild();
    for( ; currChildSubtree != NULL ; currChildSubtree = topology->get_NextChild() ) {
        // check the hostname/rank of the current subtree root
        if( ( strcmp(currChildSubtree->get_RootHostName().c_str(), myHost.c_str()) == 0 ) &&
            ( currChildSubtree->get_RootRank() != myPos->myRank ) ) {
            // we found another process on this host
            TopologyPosition* tpos = new TopologyPosition;
            tpos->myHostname = myHost;
            tpos->myRank = currChildSubtree->get_RootRank();
            tpos->subtree = currChildSubtree;
            tpos->parentHostname = topology->get_RootHostName();
            tpos->parentRank = topology->get_RootRank();
            tpos->parentPort = topology->get_RootPort();
            tpos->parSubtree = topology;
            procs.push_back( tpos );
        }
        if( ! currChildSubtree->is_RootBackEnd() )
            FindColocatedProcesses( currChildSubtree, myHost, myPos, procs );
    }
}

void
XTNetwork::FindHostsInTopology( SerialGraph* topology,
       	       	                std::map< std::string, int >& host_counts )
{
    mrn_dbg_func_begin();

    // check the hostname associated with the root
    std::string host = topology->get_RootHostName();
    std::map< std::string, int >::iterator host_info = host_counts.find( host );
    if( host_info == host_counts.end() ) {
        host_counts[host] = 1;
    }
    else {
        host_info->second++;
    }

    topology->set_ToFirstChild();
    SerialGraph* currChildSubtree = topology->get_NextChild();
    for( ; currChildSubtree != NULL ; currChildSubtree = topology->get_NextChild() )
	FindHostsInTopology( currChildSubtree, host_counts );
}


void
XTNetwork::Instantiate( ParsedGraph* topology,
                        const char* mrn_commnode_path,
                        const char* be_path,
                        const char** be_argv,
                        unsigned int be_argc,
                        const std::map<std::string,std::string>* iattrs )
{
    mrn_dbg_func_begin();

    bool have_backends = (strlen(be_path) != 0);
 
   // determine whether we will be using the ALPS tool helper at all
    int apid = -1;
    if( iattrs != NULL ) {
        for( std::map<std::string, std::string>::const_iterator iter = iattrs->begin();
            iter != iattrs->end(); iter++ ) {
            if( strcmp( iter->first.c_str(), "apid" ) == 0 ) {
                apid = (int)strtol( iter->second.c_str(), NULL, 0 );
                break;
            }
        }
    }

    // analyze the topology and attributes to determine 
    // which processes we need to start with aprun (if any)
    // and which we need to start with ALPS tool helper (if any)
    std::set<std::string> aprunHosts;
    std::set<std::string> athHosts;
    int athFirstNodeNid = -1;

    DetermineProcessTypes( topology->get_Root(),
                           apid,
                           aprunHosts,
                           athHosts,
                           athFirstNodeNid );

    if( aprunHosts.empty() && athHosts.empty() ) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "FE is only node in topology, not spawning anybody\n" ));
        return;
    }

    // start any needed internal processes and back end processes
    int spawnRet = SpawnProcesses( aprunHosts,
                                   athHosts,
                                   apid,
                                   athFirstNodeNid,
                                   mrn_commnode_path,
                                   be_path,
                                   be_argc, be_argv );
    if( spawnRet != 0 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Failure: unable to instantiate network\n" ));
        error( ERR_INTERNAL, UnknownRank, "");  // we don't know our process' rank yet
    }
    mrn_dbg(5, mrn_printf(FLF, stderr, "done spawning processes\n" ));

    // connect the processes we started into a tree
    int connectRet = ConnectProcesses( topology, have_backends );
    if( connectRet != 0 ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Failure: unable to instantiate network\n" ));
        error( ERR_INTERNAL, UnknownRank, "");  // we don't know our process' rank yet
    }
    mrn_dbg(5, mrn_printf(FLF, stderr, "done connecting processes\n" ));

    mrn_dbg_func_end();
}

int
XTNetwork::SpawnProcesses( const std::set<std::string>& aprunHosts,
                           const std::set<std::string>& athHosts,
                           int apid,
                           int athFirstNodeNid,
                           const char* mrn_commnode_path,
                           std::string be_path,
                           int argc,
                           const char** argv )
{
    int ret = 0;

    mrn_dbg_func_begin();

    // we should never be asked not to spawn any processes
    assert( !(aprunHosts.empty() && athHosts.empty()) );

    // start processes (if any) that need to be started with aprun
    if( !aprunHosts.empty() ) {
        // TODO remove hardcoded aprun spec, especially to ORNL site-specific
        // location
        std::string cmd = "/usr/bin/aprun";
        std::vector<std::string> args;
        args.push_back( cmd );

        // specify number of internal processes to create
        args.push_back( "-n" );
        std::ostringstream sizestr;
        sizestr << aprunHosts.size();
        args.push_back( sizestr.str() );

        // specify number of processes per node - we want one (SMP mode)
        // TODO is this assumption correct?
        args.push_back( "-N" );
        args.push_back( "1" );

        // specify the nodes on which to run the processes
        std::string nodeSpec;
        BuildCompactNodeSpec( aprunHosts, nodeSpec );
        args.push_back( "-L" );
        args.push_back( nodeSpec );

        // the executable
        assert( mrn_commnode_path != NULL );
        args.push_back( mrn_commnode_path );

        // since the created processes may become a BE, we need to pass this too
        args.push_back( be_path );
        for( int i = 0; i < argc; i++ ) {
            args.push_back( argv[i] );
        }

        // spawn the processes
        //
        // note that although we're creating remote processes on the 
        // compute nodes, aprun runs on a login node
        std::ostringstream cmdStr;
        for( std::vector<std::string>::const_iterator iter = args.begin();
             iter != args.end(); iter++ ) {
            cmdStr << *iter << std::endl;
        }
        mrn_dbg(3, mrn_printf(FLF, stderr, "Starting processes with: %s\n", 
                              cmdStr.str().c_str() ));

        pid_t pid = XPlat::Process::Create( "localhost",
                                            cmd,
                                            args );
        if( pid != 0 ) {
            // we failed to create the aprun process
            return -1;
        }
    }

#ifdef HAVE_LIBALPS_H
    // start processes (if any) that need to be started with alps tool helper
    if( !athHosts.empty() ) {
        assert( apid != -1 );
        assert( athFirstNodeNid != -1 );

        // TODO not sure whether the commands buf needs to be NULL terminated
        std::ostringstream cmdStr;
        cmdStr << mrn_commnode_path
                << ' ';
        for( int i = 0; i < argc; i++ ) {
            cmdStr << argv[i] << ' ';
        }
        mrn_dbg(3, mrn_printf(FLF, stderr, "Using ALPS tool helper to start: %s\n", 
                              cmdStr.str().c_str() ));

        char** cmds = new char*[2];
        cmds[0] = new char[cmdStr.str().length() + 1];
        strcpy( cmds[0], cmdStr.str().c_str() );
        cmds[1] = NULL;

        // launch the processes using ALPS tool helper
        const char* launchRet = alps_launch_tool_helper( 
                                   apid,            // app id 
                                   athFirstNodeNid, // nid of first node in placement list
                                   1,               // transfer helper program
                                   1,               // execute helper program
                                   1,               // number of helper program commands
                                   cmds             // helper program and args
                                                        ); 
        delete[] cmds[0];
        delete[] cmds;
        if( launchRet != NULL ) {
            return -1;
        }
    }
#endif // HAVE_LIBALPS_H

    return ret;
}

// Build compact specifications of compute node ids, for use
// with the '-L' placement flag to the aprun command.
//
// Since placement specifications allow ranges, we need to identify
// contiguous ranges of node ids.  We have no guarantee about ordering
// of the input hosts array, but we leave the array sorted on output.
int
XTNetwork::BuildCompactNodeSpec( const std::set<std::string>& hostset,
                                 std::string& nodespec )
{
    nodespec.clear();
    if( hostset.empty() ) {
        return 0;
    }

    // sort hosts by nid
    std::vector<std::string> hosts;
    for( std::set<std::string>::const_iterator iter = hostset.begin();
         iter != hostset.end(); iter++ ) {
        hosts.push_back( *iter );
    }
    std::sort( hosts.begin(), hosts.end() );

    // walk through sorted array, recognizing contiguous nid ranges
    std::ostringstream nodeSpecStream;
    std::pair<uint32_t,uint32_t> currRange;
    currRange.first = 0;
    currRange.second = 0;
    for( std::vector<std::string>::const_iterator iter = hosts.begin();
         iter != hosts.end(); iter++ ) {
        std::string currHost = *iter;
        uint32_t currNid = GetNidFromNodename( *iter );

        if( iter == hosts.begin() ) {
            // this is the first nid we've seen - start the first range
            currRange.first = currNid;
            currRange.second = currNid;
        }
        else if( currNid == (currRange.second + 1) ) {
            // this nid is a continuation of the current nid range
            currRange.second = currNid;
        }
        else {
            // we finished the current range and started a new one
            AddNodeRangeSpec( nodeSpecStream, currRange );
            currRange.first = currNid;
            currRange.second = currNid;
        }
    }

    // finish the last node range
    AddNodeRangeSpec( nodeSpecStream, currRange );

    // return the spec we generated
    nodespec = nodeSpecStream.str();

    return 0;
}

void
XTNetwork::AddNodeRangeSpec( std::ostringstream& nodeSpecStream,
                             std::pair<uint32_t,uint32_t> range )
{
    if( !(nodeSpecStream.str().empty()) ) {
        nodeSpecStream << ",";
    }
    nodeSpecStream << range.first;
    if( range.second != range.first ) {
        nodeSpecStream << "-" << range.second;
    }
}

// extract the node ID from a node name
// we assume the node name has format "nidxxxxx" where xxxxx is
// a five-digit decimal integer.
uint32_t
XTNetwork::GetNidFromNodename( std::string nodename )
{
    uint32_t nid = (uint32_t)-1;


    // skip over "nid" prefix
    assert( nodename[0] == 'n' );
    assert( nodename[1] == 'i' );
    assert( nodename[2] == 'd' );
    std::string nidstring = nodename.substr( 3 );

    // extract the nid number
    std::istringstream istr( nidstring );
    istr >> std::dec >> nid;

    return nid;
}

int
XTNetwork::ConnectProcesses( ParsedGraph* topology, bool have_backends )
{
    mrn_dbg_func_begin();

    Port fe_port = get_LocalPort();
    std::string fe_host = get_LocalHostName();

    // get SerialGraph topology
    std::string sg = topology->get_SerializedGraphString( have_backends );
    SerialGraph sgtmp(sg);

    // statically assign data listening ports for all processes
    char currPort[10];
    Port base = FindParentPort();
    char* sgnew = strdup( sg.c_str() );
    std::map< std::string, int > hosts;
    FindHostsInTopology( &sgtmp, hosts );
    std::map< std::string, int >::iterator host_iter = hosts.begin();
    for( ; host_iter != hosts.end() ; host_iter++ ) {

        int count = host_iter->second;
        std::ostringstream findme;
        findme << host_iter->first << ":" << UnknownPort;

        char* search = strdup( findme.str().c_str() );
        char* start = sgnew;

        for( int i=0; i < count; i++ ) {

            // need to special case FE port, which isn't static 
            if( (i == 0) && (fe_host == host_iter->first) )
                sprintf( currPort, "%d", fe_port );
            else
                sprintf( currPort, "%d", base + i );

            char* found = strstr(start, search);
            if( found != NULL ) {
                mrn_dbg(5, mrn_printf(FLF, stderr, "changing %s port to %s\n", 
                                      search, currPort ));
                found += host_iter->first.length() + 1;
                strncpy( found, currPort, strlen(currPort) );
            }
	    else break;
            start = found + strlen(currPort);
        }
        free( search );
    }
    mrn_dbg(5, mrn_printf(FLF, stderr, "topology after port assignment %s\n",
                          sgnew ));
        
    // tell our FE object about the topology
    XTFrontEndNode* fe = dynamic_cast<XTFrontEndNode*>( get_LocalFrontEndNode() );
    assert( fe != NULL );
    fe->init_numChildrenExpected( sgtmp );
    mrn_dbg(5, mrn_printf(FLF, stderr, "expecting %u children\n", 
                          fe->get_numChildrenExpected() ));

    // tell our children about the topology and update front end topology
    std::string reset_topo=sgnew;
    reset_Topology(reset_topo);
    SerialGraph* sgo = new SerialGraph( sgnew );
    assert( sgo != NULL );
    free( sgnew );
    Port topoPort = FindTopoPort();
    PropagateTopologyOffNode( topoPort, sgo, sgo );

    return 0;
}

FrontEndNode*
XTNetwork::CreateFrontEndNode( Network* n, std::string ihost, Rank irank )
{
    mrn_dbg_func_begin();

    int listeningSocket = -1;
    Port listeningPort = UnknownPort;
    if( -1 == CreateListeningSocket( listeningSocket, listeningPort, true ) ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "failed to create listening data socket\n" ));
        return NULL;
    }
    int flags;
    if((flags=fcntl(listeningSocket,F_GETFL,0))<0)
    {
           mrn_dbg(1, mrn_printf(FLF, stderr,
                   "failed to get flags on the listening data socket\n" ));
           exit( 1 );
    }

    if(fcntl(listeningSocket,F_SETFL,flags|O_NONBLOCK) < 0)
    {
           mrn_dbg(1, mrn_printf(FLF, stderr,
                   "failed to set non blocking flags on listening data socket \n" ));
           exit( 1 );
    }

    return new XTFrontEndNode( n, ihost, irank, 
                               listeningSocket, listeningPort );
}

BackEndNode*
XTNetwork::CreateBackEndNode( Network * inetwork, 
                              std::string imy_hostname,
                              Rank imy_rank,
                              std::string iphostname, 
                              Port ipport, 
                              Rank iprank )
{
    mrn_dbg_func_begin();

    if( ipport == UnknownPort ) {
        ipport = FindParentPort();
    }

    return new XTBackEndNode( inetwork, 
                              imy_hostname,
                              imy_rank,
                              iphostname,
                              ipport,
                              iprank );
}

InternalNode*
XTNetwork::CreateInternalNode( Network* inetwork, 
                               std::string imy_hostname,
                               Rank imy_rank,
                               std::string iphostname,
                               Port ipport, 
                               Rank iprank,
                               int idataSocket,
                               Port idataPort )
{
    mrn_dbg_func_begin();

    int listeningSocket = idataSocket;
    Port listeningPort = idataPort;
    if( listeningSocket == -1 ) {
        if( idataPort == UnknownPort )
            listeningPort = FindParentPort();
        if( -1 == CreateListeningSocket( listeningSocket, listeningPort, true ) ) {
            mrn_dbg(1, mrn_printf(FLF, stderr,
       	                          "failed to create listening data socket\n" ));
            return NULL;
        }
    }

    if( ipport == UnknownPort ) {
        ipport = FindParentPort();
    }

    return new XTInternalNode( inetwork,
                               imy_hostname,
                               imy_rank,
                               iphostname,
                               ipport,
                               iprank,
                               listeningSocket,
                               listeningPort );
}

int
XTNetwork::CreateListeningSocket( int& ps, Port& pp, bool nonblock ) const
{
    ps = -1;
    int bRet = bindPort( &ps, &pp, nonblock );
    if( -1 == bRet ) {
        // TODO how to indicate the error to the caller?
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "Failure: unable to instantiate network\n" ));
        return bRet;  
    }
    return 0;
}


std::string
XTNetwork::GetNodename( void )
{
    // on the XT, the node number is available in /proc/cray_xt/nid
    std::ifstream ifs( "/proc/cray_xt/nid" );
    uint32_t nid;
    ifs >> nid;

    std::ostringstream nidStr;
    nidStr << "nid"
        << std::setw( 5 )
        << std::setfill( '0' )
        << nid
        << std::ends;
        
    return nidStr.str();
}

void
XTNetwork::FindAppNodes( int nPlaces,
                         placeList_t* places,
                         std::set<std::string>& hosts )
{
    for( int i = 0; i < nPlaces; i++ ) {
        int currNid = places[i].nid;
        std::ostringstream nodeNameStr;
        nodeNameStr << "nid" 
                    << std::setw( 5 ) 
                    << std::setfill('0') 
                    << currNid;

        // add node name into set, unless it is already there
        hosts.insert( nodeNameStr.str() );
    }
}

void
XTNetwork::DetermineProcessTypes( ParsedGraph::Node* node,
                                  int apid,
                                  std::set<std::string>& aprunHosts,
                                  std::set<std::string>& athHosts,
                                  int& athFirstNodeNid ) const
{
    std::set<std::string> appHosts;

#ifdef HAVE_LIBALPS_H
    // figure out whehter we will be co-locating processes with app processes,
    // and if so, where they are
    if( -1 != apid ) {
        // find out about app process placement...
        appInfo_t alpsAppInfo;
        cmdDetail_t* alpsCmdDetail = NULL;
        placeList_t* alpsPlaces = NULL;
        int aiRet = alps_get_appinfo( apid,
                                    &alpsAppInfo,
                                    &alpsCmdDetail,
                                    &alpsPlaces );
        if( aiRet < 0 ) {
            error( ERR_INTERNAL, UnknownRank,
                "Failed to get ALPS app information");
        }

        // convert the ALPS place information into a set of node names
        // (i.e., remove duplicates)
        FindAppNodes( alpsAppInfo.numPlaces, alpsPlaces, appHosts );
        athFirstNodeNid = alpsPlaces[0].nid;

        // clean up
        free( alpsCmdDetail );
        alpsCmdDetail = NULL;

        free( alpsPlaces );
        alpsPlaces = NULL;
    }
#endif // HAVE_LIBALPS_H

    // now figure out hosts where processes will have to be
    // created with aprun (if any) and where they will have to be
    // created with ALPS tool helper
    DetermineProcessTypesAux( node,
                              appHosts,
                              aprunHosts,
                              athHosts );
}

void
XTNetwork::DetermineProcessTypesAux( ParsedGraph::Node* node,
                                     const std::set<std::string>& appHosts,
                                     std::set<std::string>& aprunHosts,
                                     std::set<std::string>& athHosts ) const
{
    // determine the host for this node
    std::string nodeName = node->get_HostName();

    // check whether this process is co-located with an app process
    if( node->get_Parent() != NULL ) {
        // it is not the FE process
        if( appHosts.find( nodeName ) != appHosts.end() ) {
            // this process must be co-located with an app process
            // it will be created with ALPS tool helper
            athHosts.insert( nodeName );
        }
        else {
            // this process is not co-located with an app process
            // it will be created with aprun
            aprunHosts.insert( nodeName );
        }
    }

    // check if node is a leaf
    if( !(node->get_Children().empty()) ) {
        // the node has children - it is either the internal node or the FE
        // in either case, we need to check its children
        for( std::vector<ParsedGraph::Node*>::const_iterator iter = node->get_Children().begin();
             iter != node->get_Children().end(); iter++ ) {
            DetermineProcessTypesAux( *iter, 
                                      appHosts,
                                      aprunHosts,
                                      athHosts );
        }
    }
}


} // namespace MRN

