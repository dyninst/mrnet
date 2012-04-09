/****************************************************************************
 * Copyright 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller   *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cerrno>
#include <cstring>
#include <string>
#include <map>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <limits.h>

#include "mrnet/MRNet.h"
#include "xt_exception.h"
#include "xt_protocol.h"
#include "xt_util.h"

extern "C"
{
#include <alps/alps.h>
#include <alps/apInfo.h>
#include <alps/libalps.h>
}

using namespace MRN;
using namespace std;

// Information about a parallel application we have started on XT compute nodes
struct XTAppInfo
{
    uint64_t apid;   // ALPS application id
    int pid;         // ALPS aprun pid
    int fdFromAprun; // fd for getting messages from aprun
    int fdToAprun;   // fd for sending messages to aprun

    XTAppInfo( uint64_t _apid, 
               int _pid,
               int _rfd,
               int _wfd )
      : apid( _apid ),
        pid( _pid ),
        fdFromAprun( _rfd ),
        fdToAprun( _wfd )
    { }
};

struct Options
{
    int nProcs;
    int appArgc;
    char** appArgv;
    int beArgc;
    char** beArgv;
    char* libpath;
    string topoSpec;

    Options( void )
      : nProcs( 0 ),
        appArgc( 0 ),
        appArgv( NULL ),
        beArgc( 0 ),
        beArgv( NULL ),
	libpath( NULL )
    { }

    void ParseCommandLine( int argc, char* argv[] );
};

struct TopologyNode
{
    string hostname;
    unsigned int index;
    vector<TopologyNode*> children;

    TopologyNode( string h )
      : hostname( h ),
        index( 0 )
    { }

    ~TopologyNode( void )
    {
        for( vector<TopologyNode*>::iterator iter = children.begin();
            iter != children.end();
            iter++ )
        {
            delete *iter;
        }
        children.clear();
    }
};

// prototypes of functions used in this file
static XTAppInfo* LaunchApplication( int myNid, 
                                     int nProcs, 
                                     int appArgc, 
                                     char* appArgv[] );

static Network* BuildNetwork( int myNid,
                              XTAppInfo* ainfo, 
                              const Options& opts );

static set<string> FindAppNodes( int nPlaces, placeList_t* places );

// for building a "flattened" multi-level topology, so that CPs are 
// on same nodes as app processes
static string BuildFlattenedTopology( unsigned int fanout,
                                      int myNid,
                                      set<string>& appNodes,
                                      const Options& options );

static void AssignTopologyIndices( TopologyNode* n, 
                                   map<string, unsigned int>& indexMap );
static void PrintTopology( TopologyNode* n, ostringstream& ostr );

int
main( int argc, char* argv[] )
{
    int ret = 0;
    Network* net = NULL;
    Stream* stream = NULL;
    Options opts;

    try
    {
        PacketPtr buf;
        int rret, tag = 0;
        size_t nBackends;

        // parse the command line
        opts.ParseCommandLine( argc, argv );

        // determine the nid of the node we're running on
        int myNid = FindMyNid();

        // launch application processes on compute nodes
        // LaunchApplication returns when FE has verified app processes
        // are created and ready to run (but not yet running)
        XTAppInfo* ainfo = LaunchApplication( myNid,
                                              opts.nProcs,
                                              opts.appArgc,
                                              opts.appArgv );
        assert( ainfo != NULL );

        // create and connect MRNet tree
        cerr << "FE: building network" << endl;
        net = BuildNetwork( myNid, ainfo, opts );

        if( (net != NULL) && (! net->has_Error()) ) {
        int filter_id = net->load_FilterFunc("xt_toolFilters.so", 
                                             "ToolFilterSum");
        if( filter_id == 0 )
            throw XTMException( "Failed to load ToolFilterSum filter function" );

        // get a broadcast stream for communicating with our back-ends
        Communicator* comm = net->get_BroadcastCommunicator();
        stream = net->new_Stream( comm, 
                                  filter_id, 
                                  SFILTER_WAITFORALL,
                                  TFILTER_NULL );
        assert( stream != NULL );
        nBackends = comm->get_EndPoints().size();
        cerr << "FE: connected to " << nBackends << " backends" << endl;

        // deliver the apid to the tool daemons
        if( (stream->send( XTM_APID, "%uld", ainfo->apid ) == -1) ||
            (stream->flush() == -1) )
        {
            throw XTMException( "FE: failed to broadcast apid to tool daemons" );
        }
        cerr << "FE: sent apid to daemons" << endl;

        // wait for tool daemons to indicate they have connected
        tag = 0;
        rret = stream->recv( &tag, buf );
        if( rret == -1 )
        {
            cerr << "FE: recv of app connected msg failed" << endl;
        }
        else if( tag != XTM_APPOK )
        {
            cerr << "FE: received app connected msg with unexpected tag " << tag << endl;
        }
        else
        {
            cerr << "FE: received app connected msg\n";
            int reducedAppOKVal = 0;
            buf->unpack( "%d", &reducedAppOKVal );
            if( reducedAppOKVal != nBackends )
            {
                cerr << "FE: received unexpected value " 
                    << reducedAppOKVal
                    << " in app connected msg; expected "
                    << nBackends
                    << endl;
            }
        }
        }
        // allow application processes to run
        cerr << "FE: releasing app to run\n";
        int aprunReleaseMsg = 1;
        int wret = write( ainfo->fdToAprun, &aprunReleaseMsg, sizeof(aprunReleaseMsg) );
        if( wret != sizeof(aprunReleaseMsg) )
        {
            int ecode = errno;
            ostringstream mstr;
            mstr << "FE tried to release by writing 1 to " << ainfo->fdToAprun
                << ", got wret=" << wret
                << ", errno = " << ecode ;
            cerr << mstr.str() << endl;
            throw XTMException( "Failed to release app processes to run" );
        }

        // handle tool back-end events
        // for proof-of-concept, we just wait for app termination
        int status;
        waitpid( ainfo->pid, &status, 0 );

        if( (net != NULL) && (! net->has_Error()) ) {
        tag = 0;
        rret = stream->recv( &tag, buf );
        if( rret == -1 )
        {
            cerr << "FE: app termination recv() failed" << endl;
        }
        else if( tag != XTM_APPDONE )
        {
            cerr << "FE: received app termination msg with unexpected tag " << tag << endl;
        }
        else
        {
            int reducedAppExitCode = 0;
            buf->unpack( "%d", &reducedAppExitCode );
            if( reducedAppExitCode != nBackends )
            {
                cerr << "FE: received unexpected value " 
                    << reducedAppExitCode
                    << " in app connected msg; expected "
                    << nBackends
                    << endl;
            }
            else
            {
                cerr << "FE: application terminated normally" << endl;
            }
        }
        // clean up
        if( stream != NULL )
        {
            if( stream->send( XTM_EXIT, "%d", 0 ) == -1 )
            {
                cerr << "FE: failed to broadcast tool daemon termination message" << endl;
            }
        }

        sleep(2);
        delete net;
        net = NULL;
        }

    }
    catch( exception& e )
    {
        cerr << "XT TOOL FE: caught excpetion: " << e.what() << endl;
        ret = 1;
    }

    return ret;
}


static
Network*
BuildNetwork( int myNid, 
              XTAppInfo* ainfo, 
              const Options& opts )
{
    char** cmds = NULL;
    appInfo_t alpsAppInfo;
    cmdDetail_t* alpsCmdDetail = NULL;
    placeList_t* alpsPlaces = NULL;
    Network* ret = NULL;

    try
    {
        // find out about app process placement...
        int aiRet = alps_get_appinfo( ainfo->apid,
                                    &alpsAppInfo,
                                    &alpsCmdDetail,
                                    &alpsPlaces );
        if( aiRet < 0 )
        {
            throw XTMException( "Failed to get ALPS app information" );
        }

        // convert the ALPS place information into a set of node names
        // (i.e., remove duplicates)
        set<string> appNodes = FindAppNodes( alpsAppInfo.numPlaces, alpsPlaces );

        // now that we know where the application processes were placed,
        // build an MRNet topology that uses those nodes 
        const unsigned int fanout = 2;  // for testing, use binary tree
        string topo = BuildFlattenedTopology( fanout, myNid, appNodes, opts );

        assert( topo.length() > 0 );
        cout << "FE: using topology\n" 
            << topo 
            << "---- End of topology ----\n";

        // build the attribute pairs needed to pass to the MRNet 
        // library for network instantiation
        //
        // For the tool use case on XT, we need to pass the apid 
        // if we are expecting to be able to launch BE and CP 
        // processes on the same nodes as the app procesess
        map<string,string> mrnet_attrs;
        ostringstream apidStr;
        apidStr << ainfo->apid;
        mrnet_attrs.insert( make_pair("CRAY_ALPS_APID", apidStr.str()) );
        mrnet_attrs.insert( make_pair("CRAY_ALPS_STAGE_FILES", string(opts.libpath)) );

        // create and connect MRNet network processes
        ret = Network::CreateNetworkFE( topo.c_str(),
                                        opts.beArgv[0], // BE exe
                                        (const char**)(&(opts.beArgv[1])),    // library assumes this is NULL terminated
                                        &mrnet_attrs,   // network attributes
                                        true,   // rank BEs
                                        true ); // topo is in memory buffer, not file 
        if( ret == NULL )
        {
            // we failed to create the network
            throw XTMException( "Failed to create MRNet net" );
        }
        cerr << "FE: done creating network" << endl;
    }
    catch( ... )
    {
        delete ret;
        ret = NULL;
    }

    // clean up
    if( cmds != NULL )
    {
        delete[] cmds[0];
        delete[] cmds;
        cmds = NULL;
    }

    if( alpsCmdDetail != NULL )
    {
        free( alpsCmdDetail );
        alpsCmdDetail = NULL;
    }
    if( alpsPlaces != NULL )
    {
        free( alpsPlaces );
        alpsPlaces = NULL;
    }

    return ret;
}


void
Options::ParseCommandLine( int argc, char* argv[] )
{
    string usageStr = "USAGE: xt_toolFE nProcs /path/to/xt_toolFilters.so --be beExe beArg0 beArg1 ... --app appExe appArg0 appArg1 ...\n";

    // parse the command line
    // the command line parsing is pretty rigid for this example program

    if( argc < 7 )
    {
        ostringstream estr;
        estr << "too few arguments given on command line\n" << usageStr;
        throw XTMException( estr.str().c_str() );
    }

    // number of application processes
    int currArgIdx = 1;
    char* endp = NULL;
    nProcs = (int)strtol( argv[currArgIdx], &endp, 10 );
    if( (endp != argv[currArgIdx] + strlen(argv[currArgIdx])) || (nProcs <=0) )
    {
        ostringstream estr;
        estr << "unable to convert nProcs argument to integer greater than zero\n" << usageStr;
        throw XTMException( estr.str().c_str() );
    }
    cerr << "FE: application size is " << nProcs << " processes\n";

    // path to filter library
    currArgIdx++;
    libpath = argv[currArgIdx];
    struct stat s;
    if( 0 != stat(libpath, &s) )
    {
        ostringstream estr;
        estr << "invalid path (" << libpath << ") given for filter library\n" << usageStr;
        throw XTMException( estr.str().c_str() );
    } 
    
    // tool be arguments
    currArgIdx++;
    if( strcmp( argv[currArgIdx], "--be" ) != 0 )
    {
        ostringstream estr;
        estr << "no tool back-end specified on command line\n" << usageStr;
        throw XTMException( estr.str().c_str() );
    }

    int firstBeArgIdx = ++currArgIdx;
    int lastBeArgIdx = firstBeArgIdx;
    while( (lastBeArgIdx < argc) && (strcmp(argv[lastBeArgIdx], "--app") != 0) )
    {
        lastBeArgIdx++;
    }
    if( lastBeArgIdx == argc )
    {
        // user did not provide the app arguments
        ostringstream estr;
        estr << "no application specified on command line\n" << usageStr;
        throw XTMException( estr.str().c_str() );
    }
    beArgc = lastBeArgIdx - firstBeArgIdx;
    beArgv = new char*[beArgc + 1];
    for( int i = firstBeArgIdx; i < lastBeArgIdx; i++ )
    {
        beArgv[i-firstBeArgIdx] = argv[i];
    }
    beArgv[beArgc] = NULL;  // make sure BE args are NULL terminated

    cerr << "FE: back-end arguments:\n";
    for( int i = 0; i < beArgc; i++ )
    {
        cerr << "\tbeArgv[" << i << "]=" << beArgv[i] << endl;
    }
    if( beArgv[beArgc] != NULL )
    {
        cerr << "WARNING: beArgv not NULL terminated correctly" << endl;
    }

    // app arguments
    if( strcmp( argv[lastBeArgIdx], "--app" ) != 0 )
    {
        ostringstream estr;
        estr << "no application specified on command line\n" << usageStr;
        throw XTMException( estr.str().c_str() );
    }

    int firstAppArgIdx = lastBeArgIdx + 1;
    int lastAppArgIdx = argc;
    appArgc = lastAppArgIdx - firstAppArgIdx;
    appArgv = new char*[appArgc + 1];
    for( int i = firstAppArgIdx; i < lastAppArgIdx; i++ )
    {
        appArgv[i-firstAppArgIdx] = argv[i];
    }
    appArgv[appArgc] = NULL;

    cerr << "FE: app arguments:\n";
    for( int i = 0; i < appArgc; i++ )
    {
        cerr << "\tappArgv[" << i << "]=" << appArgv[i] << endl;
    }
}


XTAppInfo*
LaunchApplication( int nid,
                   int nProcs,
                   int appArgc,
                   char* appArgv[] )
{
    XTAppInfo* ret = NULL;

    // because we want our tool daemons to be able to monitor all
    // application process activity from the beginning, we tell
    // the launcher to create the processes but pause them as early 
    // as possible during initialization

    // aprun uses pipes for synchronizing operations with a controlling tool
    // one pipe (read pipe) is used to tell the tool that the app processes have
    // been created and paused at initialization,
    // the other pipe (write pipe) is used to tell aprun to release the 
    // application processes
    // in the file descriptor arrays, item [0] is our end and item [1] is
    // aprun's end.
    int toAprun[2];
    int fromAprun[2];
    int pret = 0;

    pret = pipe( toAprun );
    if( pret == -1 )
    {
        throw XTMException( "Failed to build pipe for communication to aprun" );
    }
    pret = pipe( fromAprun );
    if( pret == -1 )
    {
        throw XTMException( "Failed to build pipe for communication from aprun" );
    }

    // fork the child process that will become aprun
    pid_t pid = fork();
    if( pid < 0 )
    {
        throw XTMException( "Failed to fork app launcher process" );
    }

    // determine which type of process we are to be
    if( pid > 0 )
    {
        // we are the parent process
        // close the ends of the communication pipes we don't need
        close( toAprun[0] );
        close( fromAprun[1] );
        //ostringstream mstr;
        //mstr << "parent closed to=" << toAprun[1] << ", from=" << fromAprun[1] << endl;
        //cerr << mstr.str() << endl;
    }
    else
    {
        // we are the child process
        // close the ends of the communication pipes we don't need
        close( toAprun[1] );
        close( fromAprun[0] );
        //ostringstream mstr;
        //mstr << "child closed to=" << toAprun[0] << ", from=" << fromAprun[0] << endl;
        //cerr << mstr.str() << endl;
        
        // become the app launcher process
        char** aprunArgv = new char*[appArgc + 8];
        unsigned int currIdx = 0;
        aprunArgv[currIdx++] = strdup("/usr/bin/orig/aprun");
        aprunArgv[currIdx++] = strdup("-n");
        ostringstream nProcsStr;
        nProcsStr << nProcs ;
        aprunArgv[currIdx++] = strdup( nProcsStr.str().c_str() );
        aprunArgv[currIdx++] = strdup("-P");
        ostringstream fdStr;
        fdStr << fromAprun[1] << ',' << toAprun[0] ;
        aprunArgv[currIdx++] = strdup( fdStr.str().c_str() );
        for( unsigned int i = 0; i < appArgc; i++ )
        {
            aprunArgv[currIdx++] = appArgv[i];
        }
        aprunArgv[currIdx] = NULL;

        // verification
        //cerr << "appArgc=" << appArgc << endl;
        cerr << "starting aprun process with args:" << endl;
        for( unsigned int i=0; i < currIdx; i++ )
        {
            cerr << aprunArgv[i] << endl;
        }

        // become the aprun process
        //cerr << "execing aprun" << endl;
        execvp( aprunArgv[0], aprunArgv );
        cerr << "execv of aprun failed: " << errno << endl;
        exit( 1 );
    }

    // look up ALPS apid for the app
    uint64_t apid = (uint64_t)-1;
    unsigned int nTries = 0;
    while( nTries < 10 )
    {
        cerr << "FE: trying to get apid, nid=" << nid << ", pid=" << pid << endl;
        nTries++;
        apid = alps_get_apid( nid, pid );
        if( apid > 0 )
        {
            // running application was assigned an ALPS apid
            break;
        }
        sleep( nTries );
    }
    if( apid <= 0 )
    {
        throw XTMException( "Failed to get ALPS app id for parallel application" );
    }
    cerr << "\tapid = " << apid << endl;

    // ensure application processes have been created and are ready to run
    cerr << "FE: blocking until app ready to run\n";
    int aprunReadyMsg = 0;
    int rret = read( fromAprun[0], &aprunReadyMsg, sizeof(aprunReadyMsg) );
    if( rret != sizeof(aprunReadyMsg) )
    {
        throw XTMException( "Failed read when checking app readiness" );
    }
    if( aprunReadyMsg != 1 )
    {
        ostringstream estr;
        estr << "Expected aprun ready msg value 1, got " 
            << aprunReadyMsg ;
        throw XTMException( estr.str().c_str() );
    }
    cerr << "FE: received notification from aprun that app is ready to run" << endl;

    return new XTAppInfo( apid, pid, fromAprun[0], toAprun[1] );
}

set<string>
FindAppNodes( int nPlaces, placeList_t* places )
{
    set<string> ret;

    for( int i = 0; i < nPlaces; i++ )
    {
        int currNid = places[i].nid;
        ostringstream nodeNameStr;
        nodeNameStr << "nid" 
                    << setw( 5 ) 
                    << setfill('0') 
                    << currNid;

        // add node name into set, unless it is already there
        ret.insert( nodeNameStr.str() );
    }

    return ret;
}


template<class T>
struct topoPrint : public unary_function<T, void>
{
    ostream& ostr;

    topoPrint( ostream& os )
      : ostr( os )
    { }

    void operator()( T val )
    {
        ostr << '\t' << val << ":0" << '\n'; 
    }
};

string
BuildFlattenedTopology( unsigned int fanout,
                        int myNid, 
                        set<string>& appNodes, 
                        const Options& opts )
{
    // start with level for leaves of the tree network
    vector<TopologyNode*>* currLevel = new vector<TopologyNode*>;
    for( set<string>::const_iterator iter = appNodes.begin();
         iter != appNodes.end();
         iter++ )
    {
        currLevel->push_back( new TopologyNode( *iter ) );
    }

    bool done = false;
    while( !done )
    {
        // start a new level
        vector<TopologyNode*>* oldLevel = currLevel;
        currLevel = new vector<TopologyNode*>;

        unsigned int nNodesThisLevel = (oldLevel->size() / fanout);
        if( (oldLevel->size() % fanout) != 0 )
        {
            // we need one extra node for the remainder
            nNodesThisLevel++;
        }

        for( unsigned int i = 0; i < nNodesThisLevel; i++ )
        {
            TopologyNode* newNode = new TopologyNode( (*oldLevel)[i*fanout]->hostname );
            for( unsigned int j = 0; j < fanout; j++ )
            {
                if( (i*fanout + j) < oldLevel->size() )
                {
                    newNode->children.push_back( (*oldLevel)[i*fanout + j] );
                }
                else
                {
                    // we're done with this level
                    break;
                }
            }
            currLevel->push_back( newNode );
        }

        if( currLevel->size() == 1 )
        {
            // we have reached the root
            done = true;
        }
        delete oldLevel;    // don't delete the nodes in the old level vector!
    }
    assert( currLevel->size() == 1 );

    // replace root node (which above algorithm places on a backend node)
    // with FE node
    ostringstream ostr;
    ostr << "nid" 
            << setw( 5 ) 
            << setfill('0') 
            << myNid;
    (*currLevel)[0]->hostname = ostr.str();

    // now assign per-host indices to processes
    map<string, unsigned int> indexMap;
    for( set<string>::const_iterator iter = appNodes.begin();
         iter != appNodes.end();
         iter++ )
    {
        assert( indexMap.find(*iter) == indexMap.end() );
        indexMap[*iter] = 0;
    }
    AssignTopologyIndices( (*currLevel)[0], indexMap );

    // now dump the resulting topology
    ostringstream topoStr;
    PrintTopology( (*currLevel)[0], topoStr );

    // we're done with the topology structure
    delete (*currLevel)[0];
    delete currLevel;

    return topoStr.str();
}

void
AssignTopologyIndices( TopologyNode* n, 
                        map<string, unsigned int>& indexMap )
{
    // assign index to the current node
    n->index = indexMap[n->hostname];
    indexMap[n->hostname]++;
    
    // recursively assign indices to this node's descendants
    for( vector<TopologyNode*>::iterator iter = n->children.begin();
        iter != n->children.end();
        iter++ )
    {
        AssignTopologyIndices( *iter, indexMap );
    }
}

void
PrintTopology( TopologyNode* n, ostringstream& ostr )
{
    // we do not need to add anything if we are processing a leaf
    if( n->children.empty() )
    {
        return;
    }

    ostr << n->hostname << ":" << n->index << " =>\n";

    for( vector<TopologyNode*>::iterator iter = n->children.begin();
        iter != n->children.end();
        iter++ )
    {
        ostr << "    " << (*iter)->hostname << ':' << (*iter)->index << '\n';
    }
    ostr << "    ;\n";

    for( vector<TopologyNode*>::iterator iter = n->children.begin();
        iter != n->children.end();
        iter++ )
    {
        PrintTopology( *iter, ostr );
    }
}
