/****************************************************************************
 * Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#ifndef MRN_XTNetwork_h
#define MRN_XTNetwork_h

#include <set>
#include <string>
#include <vector>
#include <utility>

extern "C"
{
#include <alps/apInfo.h>
}

#include "mrnet/Network.h"
#include "ParsedGraph.h"

namespace MRN
{

class XTNetwork : public Network
{
private:

    static int GetLocalNid(void);
    static std::string GetNodename(int nid);

    // FE support
    uint64_t alps_apid;
    int aprun_depth;
    std::set< std::string > alps_stage_files;

    void DetermineProcessTypes( ParsedGraph::Node* node,
                                std::set<std::string>& aprunHosts,
                                std::set<std::string>& athHosts,
                                int& athFirstNodeNid ) const;
    void DetermineProcessTypesAux( ParsedGraph::Node* node,
                                   const std::set<std::string>& appHosts,
                                   std::set<std::string>& aprunHosts,
                                   std::set<std::string>& athHosts ) const;

    static void FindAppNodes( int nPlaces,
                              placeList_t* places,
                              std::set<std::string>& hosts );

    static uint32_t GetNidFromNodename( std::string nodename );
    static void AddNodeRangeSpec( std::ostringstream& nodeSpecStream,
                                  std::pair<uint32_t,uint32_t> range );
    static int BuildCompactNodeSpec( const std::set<std::string>& hosts,
                                     std::string& nodespec );

    int SpawnProcesses( const std::set<std::string>& aprunHosts,
                        const std::set<std::string>& athHosts,
                        int athFirstNodeNid,
                        const char* mrn_commnode_path,
                        std::string be_path,
                        int argc, 
                        const char** argv );

    int ConnectProcesses( ParsedGraph* graph, bool have_backends );


    // IN support
    struct TopologyPosition
    {
        std::string parentHostname;
        Port parentPort;
        Rank parentRank;
        std::string myHostname;
        Rank myRank;
        SerialGraph* subtree;
        SerialGraph* parSubtree;

        TopologyPosition( void )
          : parentPort( UnknownPort ),
            parentRank( UnknownRank ),
            myRank( UnknownRank ),
            subtree( NULL ),
            parSubtree( NULL )
        { }
    };

    SerialGraph* GetTopology( int topoFd, Rank& myRank );
                              
    int PropagateTopologyOffNode( Port topoPort, 
                                  SerialGraph* mySubtree,
                                  SerialGraph* topology );
                                  
    void PropagateTopology( int topoFd,
                            SerialGraph* topology,
                            Rank childRank );

    void FindPositionInTopology( SerialGraph* topology,
				 const std::string& myHost, Rank myRank,
				 TopologyPosition*& myPos );

    void FindColocatedProcesses( SerialGraph* topology, 
                                 const std::string& myHost, 
                                 const TopologyPosition* myPos,
                                 std::vector< TopologyPosition* >& procs );

    void FindHostsInTopology( SerialGraph* topology,
			      std::map< std::string, int >& host_counts );

    bool ClosestToRoot( SerialGraph* topology,
			const std::string& childhost, Rank childrank );

    int CreateListeningSocket( int& ps, Port& pp, bool nonblock ) const;


    pid_t SpawnBE( int beArgc, char** beArgv, 
                   const char* parentHost, Rank parentRank, Port parentPort,
                   const char* myHost, Rank myRank );
                   
    pid_t SpawnCP( int* topoFd, int listeningSocket );
                   

    bool GetToolHelperDir( std::string& path );

protected:
    void init_NetSettings(void);

    virtual bool Instantiate( ParsedGraph* parsed_graph,
                              const char* mrn_commnode_path,
                              const char* ibackend_exe,
                              const char** ibackend_args,
                              unsigned int backend_argc,
                              const std::map<std::string,std::string>* iattrs);

    virtual FrontEndNode* CreateFrontEndNode( Network* n,
                                              std::string ihost,
                                              Rank irank );
    virtual BackEndNode* CreateBackEndNode( Network * inetwork, 
                                            std::string imy_hostname, 
                                            Rank imy_rank,
                                            std::string iphostname, 
                                            Port ipport, 
                                            Rank iprank );
    virtual InternalNode* CreateInternalNode( Network* inetwork, 
                                              std::string imy_hostname,
                                              Rank imy_rank,
                                              std::string iphostname,
                                              Port ipport, 
                                              Rank iprank,
                                              int idataSocket = -1,
                                              Port idataPort = UnknownPort );

public:
    // FE role
    XTNetwork( const std::map<std::string,std::string> * iattrs );

    // BE role
    XTNetwork(void);

    // CP role
    XTNetwork( bool,
               int topoPipeFd = -1,
               Port topoPort = UnknownPort,
               int timeOut = -1,
               int beArgc = 0,
               char** beArgv = NULL );

    virtual ~XTNetwork(void) {}

    static Port FindTopoPort(Port iport=UnknownPort);
    static Port FindParentPort(void);
};

} // namespace MRN

#endif // MRN_XTNetwork_h
