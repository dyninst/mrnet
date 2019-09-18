/******************************************************************************
 *  Copyright © 2003-2014 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                   Detailed MRNet usage rights in "LICENSE" file.           *
 ******************************************************************************/

#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>

#include <libgen.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "mrnet/MRNet.h"
#include "xplat/Process.h"
#include "xplat/SocketUtils.h"

#include "mrnet_config.h"
#include "utils.h"
#include "Message.h"
#include "SerialGraph.h"
#include "CTINetwork.h"
#include "XTFrontEndNode.h"
#include "XTBackEndNode.h"
#include "XTInternalNode.h"

extern "C"
{
#include "common_tools_be.h"
#ifdef HAVE_CRAY_TOOLS_FE_H
#include "common_tools_fe.h"
#define HAVE_CRAYTOOLS_FE
#endif
}

namespace MRN
{
const char* topofd_optstr = "--topofd";
const char* port_optstr = "--listen-port";
const char* timeout_optstr = "--listen-timeout";

//----------------------------------------------------------------------------
// base class factory methods
// creates objects of our specialization
Network*
Network::CreateNetworkFE( const char * itopology,
			  const char * ibackend_exe,
			  const char **ibackend_argv,
			  const std::map< std::string, std::string > * iattrs,
			  bool irank_backends,
			  bool iusing_mem_buf )
{
    mrn_dbg_func_begin();

    Network* n = new XTNetwork( iattrs );
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

/*
   CTI is going to launch a single CP on each node. Should the
   topology require more than that, it is up to that first
   CP to fork/exec the others and use a pipe to deliver the
   topology to them. The presence the topofd_optstr indicates
   that this invocation is one of the fork/exec variety.

   With this in mind, for first CP, argv has the content:

     [<port_optstr> <port>] [<timeout_optstr> <timeout>] [<be_path> [be_args...]]

   With this in mind, for the other CPs, argv has the content:

     <topofd_optstr> <fd> [<be_path> [be_args...]]

 */
Network*
Network::CreateNetworkIN( int argc, char** argv )
{
    mrn_dbg_func_begin();

    int port = -1;
    int timeout = -1;
    int topoPipeFd = -1;
    int beArgc = 0;
    char** beArgv = NULL;

    mrn_dbg(3, mrn_printf(FLF, stderr, "Inside Network::CreateNetworkIN\n"));
    if( argc > 0 ) {
        int offset = Network::GetParametersIN(argc, argv, port, timeout, topoPipeFd);
        mrn_dbg(3, mrn_printf(FLF, stderr, "Internal Network Parameters: Port=%d,Timeout=%d,TopoPipe=%d,argc=%d,offset=%d\n", port,
                 timeout, topoPipeFd, argc, offset));

        beArgc = argc - offset;
        beArgv = argv + offset;
        Port topoPort = XTNetwork::FindTopoPort(port);
        Network* net = new XTNetwork( true, topoPipeFd, topoPort,
                                      timeout, beArgc, beArgv );

        if (!net) {
            mrn_dbg(3, mrn_printf(FLF, stderr, "No network was created\n"));
        }
        return net;
    }

    mrn_dbg(3, mrn_printf(FLF, stderr, "No args were delivered\n"));
    return NULL;
}

//----------------------------------------------------------------------------
// XTNetwork methods

// FE constructor
XTNetwork::XTNetwork( const std::map< std::string, std::string > * iattrs )
    : ctiApid(0), callerMid(false), ctiMid(0)
{
    std::string myHost;
    XPlat::NetUtils::GetLocalHostName( myHost );
    set_LocalHostName( myHost );

    disable_FailureRecovery();

    // Get the file staging string, if any

    // Get the file staging string via env var, if any
    int separator = '\177';  // default to DEL chararacter
    bool stage_deps = true;  // default to staging dependencies
    const char* stage_files = NULL;
    if ((stage_files = getenv("CRAY_STAGE_FILES")) != NULL) {
      stage_deps = true;
    } else if ((stage_files = getenv("CRAY_STAGE_FILES_ONLY")) != NULL){
      stage_deps = false;
    } else if ((stage_files = getenv("CRAY_ALPS_STAGE_FILES")) != NULL) {
      separator = ':';
      stage_deps = false;
    }

    char *procs_per_node = getenv("MRNET_CRAY_PROCS_PER_NODE");
    if(procs_per_node != NULL) {
        aprun_depth = atoi(procs_per_node);
    }
    else {
        aprun_depth = -1;
    }

    // Process the attributes
    int jobID = -1;
    int stepID = -1;
    if( iattrs != NULL ) {
        mrn_dbg(3, mrn_printf(FLF, stderr, "Looping through attrs\n"));
        std::map< std::string, std::string >::const_iterator iter = iattrs->begin();
        for( ; iter != iattrs->end(); iter++ ) {
            /*
             * We need to get a CTI handle for the app by registering the app
    `        */

            // Could come in as a SLURM job ID and step ID
            if (strcmp(iter->first.c_str(), "CRAY_SLURM_JOBID") == 0) {
                jobID = atoi( iter->second.c_str() );
                mrn_dbg(3, mrn_printf(FLF, stderr, "CRAY_SLURM_JOBID=%d\n", jobID));
            }
            else if ((strcmp(iter->first.c_str(), "CRAY_SLURM_STEPID") == 0)) {
                stepID = atoi( iter->second.c_str() );
                mrn_dbg(3, mrn_printf(FLF, stderr, "CRAY_SLURM_STEPID=%d\n", stepID));
            }

            // Could come in already registered as the CTI application ID
            else if ((strcmp(iter->first.c_str(), "CRAY_CTI_APPID") == 0) ) {
                ctiApid = (cti_app_id_t) strtoul( iter->second.c_str(), NULL, 0 );
                mrn_dbg(3, mrn_printf(FLF, stderr, "CRAY_CTI_APPID=%uld\n", ctiApid));
            }

            // Could come in as the pid of the application launcher
            else if (( strcmp(iter->first.c_str(), "CRAY_LAUNCHER_PID") == 0 )
                   || ( strcmp(iter->first.c_str(), "CRAY_ALPS_APRUN_PID") == 0 ) ) {
                int launcher_pid = (int)strtol( iter->second.c_str(), NULL, 0 );
                switch (cti_current_wlm()) {
                  case CTI_WLM_SLURM:
                  {
                      cti_slurm_ops_t *ops;
                      if (cti_open_ops((void **)&ops) != CTI_WLM_SLURM) {
                         mrn_dbg(1, mrn_printf(FLF, stderr, "cti_open_ops returned invalid wlm.\n"));
                      }
                      cti_srunProc_t *jobInfo;
                      if ((jobInfo = ops->getJobInfo(launcher_pid)) == NULL) {
                          mrn_dbg(1, mrn_printf(FLF, stderr, "getJobInfo failed. CTI error: '%s'\n", cti_error_str()));
                      }
                      mrn_dbg(3, mrn_printf(FLF, stderr, "jobid=%d, stepid=%d\n", jobInfo->jobid, jobInfo->stepid));
                      if ((ctiApid = ops->registerJobStep(jobInfo->jobid, jobInfo->stepid)) == 0) {
                          mrn_dbg(1, mrn_printf(FLF, stderr, "registerJobStep failed. CTI error: '%s'\n", cti_error_str()));
                      }
                      free(jobInfo);
                  }
                      break;
                  case CTI_WLM_SSH:
                  {
                      cti_ssh_ops_t *ops;
                      if (cti_open_ops((void **)&ops) != CTI_WLM_SSH) {
                         mrn_dbg(1, mrn_printf(FLF, stderr, "cti_open_ops returned invalid wlm.\n"));
                      }
                      if ((ctiApid = ops->registerJob(launcher_pid)) == 0) {
                          mrn_dbg(1, mrn_printf(FLF, stderr, "registerJob failed. CTI error: '%s'\n", cti_error_str()));
                      }
                  }
                      break;
                  default:
                      mrn_dbg(1, mrn_printf(FLF, stderr, "Unsupported WLM for CRAY_LAUNCHER_PID\n"));
                      break;
                }
                mrn_dbg(3, mrn_printf(FLF, stderr, "Launcher pid=%d, ctiApid=%d\n",
                                      launcher_pid, ctiApid));
            }

            /*
             * Other attributes
             */
            else if ((strcmp(iter->first.c_str(), "CRAY_CTI_MID") == 0) ) {
                callerMid = true;
                ctiMid = (cti_manifest_id_t) atoi( iter->second.c_str() );
                mrn_dbg(3, mrn_printf(FLF, stderr, "CTI mid=%d\n", ctiMid));
            }
            else if ((stage_files == NULL) && (strcmp(iter->first.c_str(), "CRAY_STAGE_FILES") == 0)) {
                stage_files = iter->second.c_str();
                separator = '\177';     // DEL chararacter
                stage_deps = true;
            }
            else if ((stage_files == NULL) && (strcmp(iter->first.c_str(), "CRAY_STAGE_FILES_ONLY") == 0)) {
                stage_files = iter->second.c_str();
                separator = '\177';     // DEL chararacter
                stage_deps = false;
            }
            else if ((stage_files == NULL) && (strcmp(iter->first.c_str(), "CRAY_ALPS_STAGE_FILES") == 0)) {
                stage_files = iter->second.c_str();
                separator = ':';      // historical
                stage_deps = false;   // historical
            } else if( strcmp(iter->first.c_str(), "MRNET_PORT_BASE") == 0 ) {
                int base_port = (int)strtol( iter->second.c_str(), NULL, 0 );
                FindTopoPort(base_port); // despite name, actually sets the base
                mrn_dbg(3, mrn_printf(FLF, stderr, "MRNET_PORT_BASE=%d\n", base_port));
            }
        }
    }

    // Register with CTI if we have not done so already and we were
    // given CRAY_SLURM_JOBID and CRAY_SLURM_STEPID
    if ((jobID != -1) && (stepID != -1) && !ctiApid) {
        cti_slurm_ops_t *ops;
        if (cti_open_ops((void **)&ops) != CTI_WLM_SLURM) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "cti_open_ops returned invalid wlm.\n"));
        }
        if ((ctiApid = ops->registerJobStep(jobID, stepID)) == 0) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "registerJobStep failed. CTI error: '%s'\n", cti_error_str()));
        }
        else
            mrn_dbg(3, mrn_printf(FLF, stderr, "SLURM CTI apid=%d\n", ctiApid));
    }

    // Create a CTI manifest if one was not provided to us.
    if (ctiMid == 0) {
        cti_session_id_t ctiSid = -1;
        if ((ctiSid = cti_createSession(ctiApid)) == 0) {
            mrn_dbg(1, mrn_printf(FLF, stderr,
            "cti_createSession failed to create a session with ctiApid=%d. CTI error: '%s'\n",
            ctiApid, cti_error_str()) );
        }
        if ((ctiMid = cti_createManifest(ctiSid)) == 0) {
            mrn_dbg(1, mrn_printf(FLF, stderr,
                    "cti_createManifest failed to create a manifest. Error='%s'\n",
                    cti_error_str()) );
        }
    }

    mrn_dbg(3, mrn_printf(FLF, stderr, "CTI ctiApid==%d, ctiMid=%d\n", ctiApid, ctiMid));

    // Convert colon/DEL  separated files string to c++ strings
    // This code addresses any necessary strduping
    if( stage_files != NULL ) {
        char* files = strdup( stage_files );
        char* nextf = files;
        while( true ) {
            char* split = strchr( nextf, separator );
            if( split != NULL )
                *split = '\0';

            mrn_dbg(5, mrn_printf(FLF, stderr, "CTI stage file is %s\n", nextf) );
            xt_stage_files.insert( std::string(nextf) );

            if( split == NULL )
                break;
            else
                nextf = split + 1;
        }
        free( files );
    }
    // Check if this version of CTI supports disabling dependencies.
#ifdef CTI_ATTR_STAGE_DEPENDENCIES
    // Tell CTI if dependencies of bins and libs are to be staged
    cti_setAttribute(CTI_ATTR_STAGE_DEPENDENCIES, (stage_deps ? "1" : "0"));
    mrn_dbg(3, mrn_printf(FLF, stderr, "stage_deps=%d, separator=%c, stage_files='%s'\n",
                                        stage_deps, separator, stage_files) );
#else
    mrn_dbg(3, mrn_printf(FLF, stderr, "CTI version does not support disabling dependencies") );
#endif

}

// BE constructor
XTNetwork::XTNetwork(void)
{
    std::string myHost;
    XPlat::NetUtils::GetLocalHostName( myHost );
    set_LocalHostName( myHost );

    disable_FailureRecovery();
}

SerialGraph*
XTNetwork::GetTopology( int topoFd, Rank& myRank )
{
    mrn_dbg_func_begin();

    char* sTopology = NULL;
    uint32_t sTopologyLen = 0;

    // obtain topology from our parent
    read( topoFd, (char*)&sTopologyLen, sizeof(sTopologyLen) );
    mrn_dbg(5, mrn_printf(FLF, stderr, "read topo len=%u\n", sTopologyLen) );

    sTopology = new char[sTopologyLen + 1];
    char* currBufPtr = sTopology;
    size_t nRemaining = sTopologyLen;
    while( nRemaining > 0 ) {
        ssize_t nread = read( topoFd, currBufPtr, nRemaining );
        nRemaining -= nread;
        currBufPtr += nread;
    }
    *currBufPtr = 0;

    // get my rank
    read( topoFd, (char*)&myRank, sizeof(myRank) );

    mrn_dbg(5, mrn_printf(FLF, stderr, "read topo=%s, rank=%u\n",
                          sTopology, myRank) );

    SerialGraph* sg = new SerialGraph( sTopology );
    delete[] sTopology;

    return sg;
}

/*
   Send the topology to the CPs for children of mySubtree,
   but only those that were launched by a bulk launcher (CTI --
   aprun or srun). We only want to send to the "first CP"on each
   node. The others are created later and get the topo then.

   Returns -1 on failure.
 */
int
XTNetwork::PropagateTopologyForBulkLaunched( Port topoPort,
                                             SerialGraph* mySubtree,
				             SerialGraph* topology )
{
    mrn_dbg_func_begin();

    std::string childHost, thisHost;
    std::set< std::string > hostsSeen;
    thisHost = mySubtree->get_RootHostName();
    // hostsSeen.insert( thisHost ); // This breaks FEs sharing node with CPs
    mrn_dbg(5, mrn_printf(FLF, stderr, "sending topology from %s:%d\n",
                          thisHost.c_str(), mySubtree->get_RootRank() ) );

    // serialize the topology
    std::string sTopology = topology->get_ByteArray();

    // propagate topology to our bulk launched child processes
    mySubtree->set_ToFirstChild();
    SerialGraph* currChildSubtree = mySubtree->get_NextChild();
    for( ; currChildSubtree != NULL; currChildSubtree = mySubtree->get_NextChild() ) { // for each child

        childHost = currChildSubtree->get_RootHostName();       // hostname of child subtree root
        Rank childRank = currChildSubtree->get_RootRank();      // rank of child subtree root

        if( hostsSeen.find(childHost) == hostsSeen.end() ) {    // this child not seen yet?

            // The CP closest to the root was bulk launched and should
            // be sent the topology. The others will be fork/exec'd by
            // the closest and hand fed the topology by the forker.
            if( IsClosestToRoot( topology, childHost, childRank ) ) {

                // connect to child's topology propagation socket
                mrn_dbg(5, mrn_printf(FLF, stderr, "sending topology to %s:%d\n",
                                      childHost.c_str(), childRank ));

                int childTopoSocket = -1;
                int cret = connectHost( &childTopoSocket, childHost, topoPort, 15 );
                if( cret != 0 ) {
                    mrn_dbg(1, mrn_printf(FLF, stderr,
                                          "Failure: unable to connect to %s:%d to send topology\n",
                                          childHost.c_str(), childRank ) );
                    return -1;
                }

                // Send the topology to this child
                PropagateTopology( childTopoSocket, topology, childRank );
                XPlat::SocketUtils::Close( childTopoSocket );
            }
            hostsSeen.insert( childHost );      // It has been seen now.
        }
    }

    return 0;
}

/*
   Use the open topoFd to write the topology to the
   child. Follow up with writing the childRank
 */
void
XTNetwork::PropagateTopology( int topoFd,
			      SerialGraph* topology,
                              Rank childRank )
{
    mrn_dbg_func_begin();

    std::string sTopology = topology->get_ByteArray();
    uint32_t sTopologyLen = (uint32_t) sTopology.length();

    mrn_dbg(5, mrn_printf(FLF, stderr, "sending topology=%s, rank=%u\n",
                          sTopology.c_str(), childRank ));

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

// CP constructor
XTNetwork::XTNetwork( bool, /* dummy for distinguising from other constructors */
                      int topoPipeFd /*= -1*/,
                      Port topoPort /*= -1*/,
                      int timeOut /*= -1*/,
		      int beArgc /*= 0*/,
		      char** beArgv /*= NULL*/ )
{
    mrn_dbg_func_begin();

    if( timeOut != -1 )
        set_StartupTimeout( timeOut );

    // ensure we know our node's hostname
    std::string myHost;
    XPlat::NetUtils::GetLocalHostName( myHost );
    set_LocalHostName( myHost );

    disable_FailureRecovery();

    int topoFd = -1;
    int listeningDataSocket = -1;
    int listeningTopoSocket = -1;
    bool firstproc = false;
    if( topoPipeFd == -1 ) {
        // we are the first process on this node
        firstproc = true;
        mrn_dbg(5, mrn_printf(FLF, stderr,
                          "We are the first process on this node\n"));

        // set up a listening socket so our parent can send us the topology
        Port p = (Port) topoPort;
        if( -1 == CreateListeningSocket(listeningTopoSocket, p, false) ) {
            mrn_dbg(1, mrn_printf(FLF, stderr,
                                  "failed to create topology listening socket\n"));
            exit(1);
        }
        mrn_dbg(5, mrn_printf(FLF, stderr, "Created listening socket\n"));
	int timeout = get_StartupTimeout();
        topoFd = getSocketConnection( listeningTopoSocket,
                                      timeout, false );
        if( topoFd == -1 ) {
            mrn_dbg(1, mrn_printf(FLF, stderr,
                                  "failed to get topology listening socket connection\n"));
            exit(1);
        }
        mrn_dbg(5, mrn_printf(FLF, stderr, "Got topo connection\n"));
    }
    else {
        // we are not the first process on this node
        // we were forked from another process that set up a pipe
        // on which to deliver our part of the topology
        topoFd = topoPipeFd;
        mrn_dbg(5, mrn_printf(FLF, stderr,
                          "We are NOT the first process on this node\n"));
    }
    assert( topoFd != -1 );

    // Get the topology
    Rank myRank;
    SerialGraph* topology = GetTopology( topoFd, myRank );
    assert( topology != NULL );
    XPlat::SocketUtils::Close( topoFd );
    if( listeningTopoSocket != -1 )
        XPlat::SocketUtils::Close( listeningTopoSocket );

    TopologyPosition* my_tpos = NULL;
    FindPositionInTopology( topology, myHost, myRank, my_tpos );
    assert( my_tpos != NULL );
    bool am_BE = ( my_tpos->subtree->is_RootBackEnd() && (beArgc > 0) );

#if 0 // set to 1 for early startup debugging
    set_LocalRank( myRank );
    init_ThreadState( CP_NODE );
    set_OutputLevel(5);
    extern char* MRN_DEBUG_LOG_DIRECTORY;
    MRN_DEBUG_LOG_DIRECTORY = strdup("/some/path/to/mrnet-log");
    mrn_dbg(5, mrn_printf(FLF, stderr, "Hello\n"));
#endif

    if( firstproc ) { // first process on this node

        // spawn other processes on this node, if any
        std::vector< TopologyPosition* > other_procs;
        FindColocatedProcesses( topology, myHost, my_tpos, other_procs );
        if( other_procs.size() ) {

            std::vector< TopologyPosition* >::iterator pos_iter = other_procs.begin();
            for( ; pos_iter != other_procs.end() ; pos_iter++ ) {

                TopologyPosition* tpos = *pos_iter;
                assert( tpos != NULL );

                // check if current pos is a BE or another CP
                pid_t currPid = 0;
                if( tpos->subtree->is_RootBackEnd() && (beArgc > 0) ) {

                    currPid = SpawnBE( beArgc, beArgv, tpos->parentHostname.c_str(),
                                       tpos->parentRank, tpos->parentPort,
                                       myHost.c_str(), tpos->myRank );

                    if( currPid == 0 ) {
                        // we failed to spawn the process
                        mrn_dbg(1, mrn_printf(FLF, stderr,
                                              "Failure: unable to spawn child %s:%d\n",
                                              myHost.c_str(), (int)tpos->myRank));
                        exit(1);
                    }
                }
                else {
                    int currTopoFd = -1;
                    currPid = SpawnCP( &currTopoFd, listeningDataSocket );
                    if( (currPid == 0) || (currTopoFd == -1) ) {
                        // we failed to spawn the process
                        mrn_dbg(1, mrn_printf(FLF, stderr,
                                              "Failure: unable to spawn child %s:%d\n",
                                              myHost.c_str(), (int)tpos->myRank));
                        exit(1);
                    }

                    // deliver the full topology
                    PropagateTopology( currTopoFd, topology, tpos->myRank );
                    XPlat::SocketUtils::Close( currTopoFd );
                }

                delete tpos;
            }
            mrn_dbg(3, mrn_printf(FLF, stderr,
                                 "done spawning co-located processes\n"));
        }
	else
            mrn_dbg(3, mrn_printf(FLF, stderr,
                                  "no other processes on this node to spawn\n"));
    }

    if( am_BE ) {
        // I am supposed to be a BE but currently am CP.
        // I need to spawn a BE and deliver the topology as I received it
        mrn_dbg(3, mrn_printf(FLF, stderr, "%s is a backend, calling SpawnBE\n", myHost.c_str()));

        // spawn the BE process
        pid_t bePid = SpawnBE( beArgc, beArgv, my_tpos->parentHostname.c_str(),
                               my_tpos->parentRank, my_tpos->parentPort,
                               myHost.c_str(), myRank );
        if( bePid == 0 ) {
            // we failed to spawn the BE process
            // TODO is this best way to handle error?
            mrn_dbg(1, mrn_printf(FLF, stderr,
                                  "Failure: unable to spawn child on %s\n",
                                  (my_tpos->myHostname).c_str()));
            exit(1);
        }
        delete my_tpos;

        // we can't just go away -
        // in case we were started by aprun, we need to stick
        // around so that it doesn't think the "application" is done.
        int beProcessStatus = 0;
        waitpid( bePid, &beProcessStatus, 0 );
        exit(0);
    } else {
        mrn_dbg(3, mrn_printf(FLF, stderr, "%s is not a backend: is_RootBackEnd() = %s, beArgc = %d\n", myHost.c_str(),
         my_tpos->subtree->is_RootBackEnd() ? "true" : "false", beArgc ));
    }

    /**************************************************************
     *
     * BEGIN CP INITIALIZATION REGION
     *
     * Note: the code in this region should not be moved before we
     *       spawn co-located processes, as there seems to be an
     *       issue with becoming multi-threaded before forking other
     *       CPs (the symptom is the EDT poll/select not seeing
     *       connection attempts on its listening socket).
     *
     **************************************************************/

    // Start listening only AFTER spawning co-located processes
    Port listeningDataPort = my_tpos->subtree->get_RootPort();
    if( -1 == CreateListeningSocket(listeningDataSocket, listeningDataPort, true) ) {
        mrn_dbg(1, mrn_printf(FLF, stderr,
                              "failed to create listening data socket\n"));
        exit(1);
    }

    // Initialize as CP
    Network::init_InternalNode( my_tpos->parentHostname.c_str(),
                                my_tpos->parentPort,
                                my_tpos->parentRank,
                                myHost.c_str(),
                                myRank,
                                listeningDataSocket,
                                listeningDataPort );

    // Tell local ParentNode how many children to expect
    XTInternalNode* in = NULL;
    in = dynamic_cast< XTInternalNode* >( get_LocalInternalNode() );
    if( in == NULL ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "internal node dynamic_cast failed\n"));
        exit(1);
    }
    in->init_numChildrenExpected( *(my_tpos->subtree) );
    mrn_dbg(5, mrn_printf(FLF, stderr, "expecting %u children\n",
                          in->get_numChildrenExpected()));

    /**************************************************************
     *
     * END CP INITIALIZATION REGION
     *
     **************************************************************/

    // propagate topology to any bulk launched children
    if( ! my_tpos->subtree->is_RootBackEnd() ) {
        int rc = PropagateTopologyForBulkLaunched( topoPort, my_tpos->subtree, topology );
        if( rc != 0 ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "bulk launched topology propagation failed\n"));
            exit(1);
        }
    }

    delete my_tpos;

    in->PropagateSubtreeReports();
}


pid_t
XTNetwork::SpawnCP( int* topoFd, int listeningSocket )
{
    mrn_dbg_func_begin();

    pid_t ret = 0;
    *topoFd = -1;

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

	if( listeningSocket != -1 )
	    XPlat::SocketUtils::Close( listeningSocket );

        // we need to become another CP process
        // we also need to pass the fd of the topology
        // pipe on the command line
        int currIdx = 0;
        int argc = 4;

        char** argv = new char*[argc+1];
        char fdstr[8];
        sprintf(fdstr, "%d", topoFds[0]);
        argv[currIdx++] = strdup( topofd_optstr );
        argv[currIdx++] = strdup( fdstr );
        argv[currIdx] = NULL;

        char topoport[8]; // ???????
        sprintf(topoport, "%hd", FindTopoPort());
        argv[currIdx++] = strdup( port_optstr );
        argv[currIdx++] = strdup( topoport );


        mrn_dbg(5, mrn_printf(FLF, stderr, "spawning CP with arguments:\n" ));
        for( int i = 0; i < currIdx; i++ ) {
            mrn_dbg(5, mrn_printf(FLF, stderr, "CP argv[%d]=%s\n", i, argv[i] ));
        }

        // now, act like a CP already
        // NOTE: following code should match actions of CP main() (see CommunicationNodeMain.C)
        //--- BEGIN CP main() ---
        Network* net = CreateNetworkIN( argc, argv );
        if( (net == NULL) || net->has_Error() ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "spawn of CP failed\n" ));
            return 0;
        }
        else {
            net->waitfor_ShutDown();
            delete net;
            exit(0);
        }
        //--- END CP main() ---
    }
    else {
        mrn_dbg(1, mrn_printf(FLF, stderr, "fork of colocated CP failed\n"));
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

    // we need to pass the connection info
    int argc = beArgc + 5;
    char** argv = new char*[argc + 1];

    int currIdx = 0;
    for( int i = 0; i < beArgc; i++ ) {
        argv[currIdx++] = strdup(beArgv[i]);
    }
    argv[currIdx++] = strdup(parentHost);
    argv[currIdx++] = strdup(pport);
    argv[currIdx++] = strdup(prank);
    argv[currIdx++] = strdup(childHost);
    argv[currIdx++] = strdup(crank);
    argv[currIdx] = NULL;

    mrn_dbg(5, mrn_printf(FLF, stderr, "spawning BE with arguments:\n"));
    for( int i = 0; i < argc; i++ ) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "BE argv[%d]=%s\n", i, argv[i] ));
    }

    // we can't use the XPlat::Process::Create function here
    // because we need the pid to wait on.
    pid_t pid = fork();
    if( pid > 0 ) {
        // we are the parent
        ret = pid;
	mrn_dbg(5, mrn_printf(FLF, stderr, "spawned BE has pid %d\n", (int)pid));
    }
    else if( pid == 0 ) {
        // we are the child
        execvp( argv[0], argv );
        mrn_dbg(1, mrn_printf(FLF, stderr, "exec of BE failed\n"));
        exit(1);
    }
    else {
        mrn_dbg(1, mrn_printf(FLF, stderr, "fork of BE failed\n" ));
        ret = 0;
    }

    return ret;
}

Port
XTNetwork::FindTopoPort( Port passed_port /*= UnknownPort*/ )
{
    static Port defaultTopoPort = 26500;

    if( UnknownPort != passed_port )
        defaultTopoPort = (Port) passed_port;

    Port topoPort = defaultTopoPort;

    // allow env to override default
    char* topoPortStr = getenv( "MRNET_PORT_BASE" );
    if( topoPortStr != NULL )
        topoPort = (Port) atoi( topoPortStr );

    return topoPort;
}

Port
XTNetwork::FindParentPort(void)
{
    // assumption: EDT port == topology port + 1
    Port ret = FindTopoPort();
    assert( ret != UnknownPort );
    return ret + (Port)1;
}

/*
   Search the topology for the "first" occurrence of the
   requested host, By traversing the tree from the root downward
   (children from first to last), the first occrrence will
   be to closest to the root. That is, the highest level, the
   "left most" child.
 */
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

/*
    Is the childhost/childrank combo the closest childhost to
    the root in this tree?
 */
bool
XTNetwork::IsClosestToRoot( SerialGraph* topology,
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


bool
XTNetwork::Instantiate( ParsedGraph* topology,
                        const char* mrn_commnode_path,
                        const char* be_path,
                        const char** be_argv,
                        unsigned int be_argc,
                        const std::map<std::string,std::string>* /* iattrs */ )
{
    mrn_dbg_func_begin();

    bool have_backends = (strlen(be_path) != 0);

    // analyze the topology and attributes to determine
    // which processes we need to start with aprun (if any)
    // and which we need to start with ALPS tool helper (if any)
    std::set<std::string> aprunHosts;
    std::set<std::string> athHosts;

    DetermineProcessTypes( topology->get_Root(),
                           aprunHosts,
                           athHosts);

    if( aprunHosts.empty() && athHosts.empty() ) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "FE is only node in topology, not spawning anybody\n"));
        return true;
    }

    // start any needed internal processes and back end processes
    std::string bePathStr(be_path ? be_path : "");
    int spawnRet = SpawnProcesses( aprunHosts,
                                   athHosts,
                                   mrn_commnode_path,
                                   bePathStr,
                                   be_argc, be_argv );
                                   //*(new std::string(be_path ? be_path : "")),
    if( spawnRet != 0 ) {
        error( ERR_INTERNAL, get_LocalRank(), "failed to spawn processes");
        return false;
    }
    mrn_dbg(5, mrn_printf(FLF, stderr, "done spawning processes\n" ));

    // connect the processes we started into a tree
    int connectRet = ConnectProcesses( topology, have_backends );
    if( connectRet != 0 ) {
        error( ERR_INTERNAL, get_LocalRank(), "failed to connect processes");
        return false;
    }
    mrn_dbg(5, mrn_printf(FLF, stderr, "done connecting processes\n" ));

    mrn_dbg_func_end();
    return true;
}

int
XTNetwork::SpawnProcesses( const std::set<std::string>& aprunHosts,
                           const std::set<std::string>& athHosts,
                           const char* mrn_commnode_path,
                           std::string be_path,
                           int be_argc,
                           const char** be_argv )
{
    mrn_dbg_func_begin();

    // we should never be asked not to spawn any processes
    assert( ! (aprunHosts.empty() && athHosts.empty()) );

    char timeout[8], topoport[8], depth[8];
    sprintf(timeout, "%d", get_StartupTimeout());
    sprintf(topoport, "%hd", FindTopoPort());
    if(aprun_depth > 0) {
        sprintf(depth, "%d", aprun_depth);
    }

    // start processes (if any) that need to be started with aprun
    if( ! aprunHosts.empty() ) {
        std::string cmd;
        std::vector<std::string> args;

        std::ostringstream sizestr;

        std::string nodeSpec;

        switch (cti_current_wlm()) {

            case CTI_WLM_SLURM:
                cmd = "srun";
                args.push_back( cmd );
                // specify number of internal processes to create
                args.push_back( "-n" );
                sizestr << aprunHosts.size();
                args.push_back( sizestr.str() );
                // specify number of processes per node - we want one
                args.push_back( "--ntasks-per-node" );
                args.push_back( "1" );
                // specify depth
                if(aprun_depth > 0) {
                    args.push_back( "--cpus-per-task" );
                    args.push_back( depth );
                }
                // specify the nodes on which to run the processes
                BuildCompactNodeSpec( aprunHosts, nodeSpec );
                args.push_back( "--nodelist" );
                args.push_back( nodeSpec );
                break;
        }


        // the executable
        assert( mrn_commnode_path != NULL );
        args.push_back( mrn_commnode_path );
        args.push_back( port_optstr );
        args.push_back( topoport );
        args.push_back( timeout_optstr );
        args.push_back( timeout );

        // since the created processes may become a BE, we need to pass this too
        if( be_path.length() ) {
            args.push_back( be_path );
            for( int i = 0; i < be_argc; i++ ) {
                args.push_back( be_argv[i] );
            }
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

    // start processes (if any) that need to be started with alps tool helper
    if( ! athHosts.empty() ) {
        assert( ctiMid != 0 );

        // stage back-end if there is one
        if( be_path.length() ) {
            mrn_dbg(3, mrn_printf(FLF, stderr, "Using CTI to stage: %s\n",
				  be_path.c_str()));

            if (cti_addManifestBinary(ctiMid,  be_path.c_str())) {
                mrn_dbg(1, mrn_printf(FLF, stderr,
                                      "CTI failed to stage %s - error is '%s'\n",
                                       be_path.c_str(), cti_error_str()) );
                return -1;
            }
        }

        // stage files if requested
        if( xt_stage_files.size() ) {
            std::set< std::string >::iterator fiter = xt_stage_files.begin();
            for( ; fiter != xt_stage_files.end() ; fiter++ ) {
                mrn_dbg(3, mrn_printf(FLF, stderr, "Using CTI tool helper to stage: %s\n",
                     fiter->c_str()));
                if (cti_addManifestLibrary(ctiMid, fiter->c_str())) {
                    mrn_dbg(1, mrn_printf(FLF, stderr,
                                          "CTI failed to stage %s - error is '%s'\n",
                                          fiter->c_str(), cti_error_str()) );
                    return -1;
                }
            }
        }

        // Build the args array for launching CP // ??? Are the strdups needed?
        int argvSize = be_argc + 6;     // "6" must match args below
        char const **args = new (char const *[argvSize])();

        int argc = 0;

        args[argc++] = strdup(port_optstr);                     // 1 argv
        args[argc++] = strdup(topoport);                        // 2 argv
        args[argc++] = strdup(timeout_optstr);                  // 3 argv
        args[argc++] = strdup(timeout);                         // 4 argv

        if (be_path.length()) {
            args[argc++] = // strip off path
              strdup(
                be_path.substr(
                  be_path.find_last_of('/')+1
                ).c_str()
              );                                                // 5 argv

            for( int i = 0; i < be_argc; i++ ) {                // be_argc argv
                  args[argc++] = strdup(be_argv[i]);
            }
        }
        args[argc++] = NULL;                                    // 6 argv
        assert(argc <= argvSize); // No writing off end of argv allowed!

        for( char const **pp = args; *pp ; pp++ ) {
          mrn_dbg(5, mrn_printf(FLF, stderr, "arg='%s'\n", *pp));
        }

        // launch the processes using CTI
        mrn_dbg(3, mrn_printf(FLF, stderr, "Using CTI to start: %s \n",
                               mrn_commnode_path ));

        if (cti_execToolDaemon(
                                    ctiMid,
                                    mrn_commnode_path,
                                    args,
                                    0                   // no environment - this could be used to propogate MRNet env variables
                                            )) {
            mrn_dbg(1, mrn_printf(FLF, stderr,
                                  "CTI failed to exec tool deamon %s - error is '%s'\n",
                                  mrn_commnode_path, cti_error_str()) );
            return -1;
        }

        mrn_dbg(3, mrn_printf(FLF, stderr, "Returned from using CTI to start: %s \n",
                              mrn_commnode_path ));
        delete[] args;
    } else {
    	// Force the manifest to ship if it was provided by the user. We need to
    	// do this if no CP or BE processes were launched by MRNet. This could
    	// be the case for the backend attach scenario when the fanout is high
    	// enough so that a tree is not required.
    	if (callerMid)
    	{
    		if (cti_sendManifest(ctiMid))
    		{
    			mrn_dbg(1, mrn_printf(FLF, stderr,
    						"CTI failed to ship manifest - error is '%s'\n",
    						cti_error_str()) );
    			return -1;
    		}
    	}
    }

    return 0;
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

    if( 0 == strncmp(nodename.c_str(), "nid", 3) ) {
        // skip over "nid" prefix
        std::string nidstring = nodename.substr( 3 );

        // extract the nid number
        std::istringstream istr( nidstring );
        istr >> std::dec >> nid;
    }
    // else, not in expected format

    return nid;
}

int
XTNetwork::ConnectProcesses( ParsedGraph* topology, bool have_backends )
{
    mrn_dbg_func_begin();

    Port base, fe_port;
    base = FindParentPort();
    fe_port = get_LocalPort();

    std::string fe_host = get_LocalHostName();

    // get SerialGraph topology
    std::string sg = topology->get_SerializedGraphString( have_backends );
    SerialGraph sgtmp(sg);

    // statically assign data listening ports for all processes
    int currPort;
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
            	currPort = fe_port;
            else
            	currPort = base + i;

            char* found = strstr(start, search);
            if( found != NULL ) {
            	// we always use a fixed port length of five digits.
            	// 0-pad when necessary.
            	std::ostringstream paddedPort;
            	paddedPort.width(5);
            	paddedPort.fill('0');
            	paddedPort << currPort;
                mrn_dbg(5, mrn_printf(FLF, stderr, "changing %s port to %d\n",
                                      search, currPort ));
                found += host_iter->first.length() + 1;
                strncpy( found, paddedPort.str().c_str(), paddedPort.str().length() );
            }
            else break;
            // port length is fixed to five digits
            start = found + 5;
        }
        free( search );
    }
    mrn_dbg(5, mrn_printf(FLF, stderr, "topology after port assignment %s\n",
                          sgnew ));

    // tell our FE object about the topology
    XTFrontEndNode* fe = dynamic_cast<XTFrontEndNode*>( get_LocalFrontEndNode() );
    if( fe == NULL ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "front-end dynamic_cast failed\n") );
        return -1;
    }
    fe->init_numChildrenExpected( sgtmp );
    mrn_dbg(5, mrn_printf(FLF, stderr, "expecting %u children\n",
                          fe->get_numChildrenExpected()) );

    // tell our children about the topology and update front end topology
    std::string reset_topo = sgnew;
    reset_Topology( reset_topo );
    SerialGraph* sgo = new SerialGraph( sgnew );
    if( sgo == NULL ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "new SerialGraph() failed\n") );
        return -1;
    }
    free( sgnew );

    Port topoPort = FindTopoPort();
    int rc = PropagateTopologyForBulkLaunched( topoPort, sgo, sgo );
    if( rc != 0 ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "bulk launched topology propagation failed\n") );
        return -1;
    }
    return 0;
}

FrontEndNode*
XTNetwork::CreateFrontEndNode( Network* n, std::string ihost, Rank irank )
{
    mrn_dbg_func_begin();

    int listeningSocket = -1;
    Port listeningPort = UnknownPort;
    bool doCrayHack = false;

    // check for HLRS hack
    if (getenv("MRNET_CRAY_HLRS") != NULL)
    {
    	doCrayHack = true;
    	setenv("MRNET_CRAY_HOSTNAME", ihost.c_str(), 1);
    }

    if( -1 == CreateListeningSocket( listeningSocket, listeningPort, true ) ) {
        mrn_dbg(1, mrn_printf(FLF, stderr,
                              "failed to create listening data socket\n" ));
        return NULL;
    }

    if (doCrayHack)
    {
    	unsetenv("MRNET_CRAY_HOSTNAME");
    }

    int flags;
    if((flags=fcntl(listeningSocket,F_GETFL,0))<0)
    {
           mrn_dbg(1, mrn_printf(FLF, stderr,
                   "failed to get flags on the listening data socket\n" ));
           exit(1);
    }

    if(fcntl(listeningSocket,F_SETFL,flags|O_NONBLOCK) < 0)
    {
           mrn_dbg(1, mrn_printf(FLF, stderr,
                   "failed to set non blocking flags on listening data socket \n" ));
           exit(1);
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

/*
   Convert the array of ptr to string format to a set.
 */
void
XTNetwork::FindAppNodes(
  char **nodeList,              // IN: Null terminated array of ptr to node names
  std::set<std::string>& hosts  // OUT: Set of host names
  )
{
    for( char **p = nodeList; *p; p++ ) {
        hosts.insert( *p);
        //mrn_dbg(5, mrn_printf(FLF, stderr, // Comment out after testing
                              //"node %s has app procs\n",
                              //*p));
    }

    mrn_dbg(5, mrn_printf(FLF, stderr,
                         "Found %" PRIszt " app nodes\n", hosts.size()));
}

/*
    Analyze the topology to determine which processes we need
    to start with aprun (if any) and which we need to start
    with ALPS tool helper (if any). Any nodes that are not
    part of the app are returned in aprunHosts, the others
    are returned in athHosts.

    The "first node" is hitching a ride (returned) via this
    routine. Future ATH calls need it.

 */
void
XTNetwork::DetermineProcessTypes( ParsedGraph::Node* node,              // IN
                                  std::set<std::string>& aprunHosts,    // OUT
                                  std::set<std::string>& athHosts      // OUT
                                  ) const
{
    std::set<std::string> appHosts;

    if( ctiApid != 0 ) {
        // find out about app process placement...
        char **hostList = NULL;
        hostList =  cti_getAppHostsList( ctiApid );
        if ( !hostList ) {
          error( ERR_INTERNAL, UnknownRank,
                 "Failed to get CTI hostlist information for "
                 "ctiApid=%d CTI error: %s", ctiApid, cti_error_str());
          return;
        }

        // convert the ALPS place information into a set of node names
        // (i.e., remove duplicates)
        FindAppNodes( hostList, appHosts );

        // clean up
        free( hostList );
        hostList = NULL;
    }

    // now figure out hosts where processes will have to be
    // created with aprun (if any) and where they will have to be
    // created with ALPS tool helper
    DetermineProcessTypesAux( node,
                              appHosts,
                              aprunHosts,
                              athHosts );
}

/*
    Recursively walk the topology and determine, for each
    node, whether it should be launched on the app nodes
    (launched by ATH - ALPS Tool Helper) versus the non-app
    nodes (launched by aprun). Add each node either to the
    athHosts or aprunHosts set, as appropriate.

    The test is to see if the node is in the appHosts set. If
    so athHosts, else aprunHosts.
 */
void
XTNetwork::DetermineProcessTypesAux( ParsedGraph::Node* node,
                                     const std::set<std::string>& appHosts,     // IN
                                     std::set<std::string>& aprunHosts,         // OUT
                                     std::set<std::string>& athHosts ) const    // OUT
{
    // determine the host for this node
    std::string nodeName = node->get_HostName();

    // check whether this process is co-located with an app process
    if( node->get_Parent() != NULL ) { // not the FE process
        std::set< std::string >::const_iterator iend = appHosts.end();
        std::set< std::string >::const_iterator finder = appHosts.find( nodeName );
        if( finder != iend ) {
            // this process must be co-located with an app process
            // it will be created with ALPS tool helper
            athHosts.insert( nodeName );
            //mrn_dbg(5, mrn_printf(FLF, stderr,
            //                      "node %s IS co-located with app\n", nodeName.c_str()));
        }
        else {
            // this process is not co-located with an app process
            // it will be created with aprun
            aprunHosts.insert( nodeName );
            //mrn_dbg(5, mrn_printf(FLF, stderr,
            //                      "node %s NOT co-located with app\n",
            //                      nodeName.c_str()));
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

