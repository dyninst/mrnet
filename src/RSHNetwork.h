/******************************************************************************
 *  Copyright © 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                   Detailed MRNet usage rights in "LICENSE" file.           *
 ******************************************************************************/

#ifndef MRN_RSHNetwork_h
#define MRN_RSHNetwork_h

namespace MRN
{

class RSHNetwork : public Network
{
 protected:
    void init_NetSettings(void);

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
    // FE ctor
    RSHNetwork( const std::map< std::string, std::string>* iattrs );

    // BE/IN ctor
    RSHNetwork( const char* phostname, Port pport, Rank prank,
                const char* myhostname, Rank myrank, bool isInternal );

    virtual ~RSHNetwork(void) {}
};

} // namespace MRN

#endif // MRN_RSHNetwork_h
