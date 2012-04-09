/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Process.C,v 1.8 2008/10/09 19:54:05 mjbrim Exp $
#include <cassert>
#include "xplat/Process.h"

std::string XPlat::Process::XPLAT_RSH;
std::string XPlat::Process::XPLAT_RSH_ARGS;
std::string XPlat::Process::XPLAT_REMCMD;

namespace XPlat
{

int
Process::CreateRemote( const std::string& host,
                       const std::string& cmd,
                       const std::vector<std::string>& args )
{
    assert( host.length() > 0 );
    assert( !NetUtils::IsLocalHost( host ) );
    
    // determine the remote shell program to use
    
    std::string rshCmd = "ssh";
    std::string varval;

    if( !(XPLAT_RSH.empty() ))
    {
       rshCmd = XPLAT_RSH;
    }   
    
    std::vector<std::string> rshArgs;
    rshArgs.push_back( rshCmd );

    if( !(XPLAT_RSH_ARGS.empty() ))
    {
        rshArgs.push_back( XPLAT_RSH_ARGS );
    }

#ifndef os_windows
    rshArgs.push_back( std::string("-n") ); // redirect stdin to /dev/null //
#endif
    rshArgs.push_back( host );
    
    // determine whether the remote command must be run by some other
    // remote utility command (e.g., so that it has AFS tokens)

    if( !(XPLAT_REMCMD.empty() ))
    {
        rshArgs.push_back( XPLAT_REMCMD );
    }

    // add the user-supplied args
    for( std::vector<std::string>::const_iterator aiter = args.begin();
            aiter != args.end();
            aiter++ )
    {
        rshArgs.push_back( *aiter );
    }

    // execute the local command that will create the remote process
    return CreateLocal( rshCmd, rshArgs );
}

} // namespace XPlat

