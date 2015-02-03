/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Process.h,v 1.7 2007/03/15 20:11:04 darnold Exp $
#ifndef XPLAT_PROCESS_H
#define XPLAT_PROCESS_H

#include <cstdlib>
#include <string>
#include <vector>
#include "xplat/NetUtils.h"
#include <map>
#include <inttypes.h>
 
namespace XPlat
{

class Process
{
 private:
    static int CreateLocal( const std::string& cmd,
                            const std::vector<std::string>& args );
    static int CreateRemote( const std::string& host,
                             const std::string& cmd,
                             const std::vector<std::string>& args );

    static int SplitArgString(std::string argString, std::vector<std::string> & args);    
    static std::string XPLAT_RSH;
    static std::string XPLAT_RSH_ARGS;
    static std::string XPLAT_REMCMD;

 public:
    static int GetProcessId( void );

    // spawn a new process (cmd should be args[0])
    static int Create( const std::string& host,
                       const std::string& cmd,
                       const std::vector<std::string>& args )
    {
        int ret;
        if( NetUtils::IsLocalHost(host) )
            ret = CreateLocal( cmd, args );
        else
            ret = CreateRemote( host, cmd, args );
        return ret;
    }
    
    static void set_RemoteShell( std::string rsh )
    {
        XPLAT_RSH = rsh;
    }

    static void set_RemoteShellArgs( std::string rshargs )
    {
        XPLAT_RSH_ARGS =rshargs;
    }

    static void set_RemoteCommand( std::string remcmd ) 
    {
        XPLAT_REMCMD = remcmd;
    }

    static int GetLastError( void );
};

} // namespace XPlat

#endif // XPLAT_PROCESS_H
