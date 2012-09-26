/******************************************************************************
 *  Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                   Detailed MRNet usage rights in "LICENSE" file.           *
 ******************************************************************************/

#ifndef MRN_LIBINetwork_h
#define MRN_LIBINetwork_h

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <sys/types.h>

#include "libi/libi_api.h"

namespace MRN
{

class LIBINetwork : public Network
{
 protected:

    virtual bool Instantiate( ParsedGraph* parsed_graph,
                              const char* mrn_commnode_path,
                              const char* ibackend_exe,
                              const char** ibackend_args,
                              unsigned int backend_argc,
                              const std::map< std::string, std::string>* iattrs );

    virtual FrontEndNode* CreateFrontEndNode( Network* inetwork,
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
    //These maybe should be protected but I need the static functions
    //create network functions to be able to have access to them.
	void get_parameters( int argc, char* argv[], bool isForMW,
                             char* &myhostname, Port &myport, Rank &myrank,
                             int &mysocket,
                             char* &phostname, Port &pport, Rank &prank,
                             int &mynumchild );

    bool bootstrap_communication_distributed( libi_sess_handle_t sess,
                                  NetworkTopology* nt,
                                  bool isForMW,
                                  bool launchBE,
                                  void* & ports,
                                  int & ports_size);
    // declare constructor here.
    LIBINetwork();

    virtual ~LIBINetwork(void) {}

};

} // namespace MRN

#endif // MRN_LIBINetwork_h
