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
Process::SplitArgString(std::string argString, std::vector<std::string> & args) {
    const char * argChar = argString.c_str();
    int64_t startpos = -1;
    bool inQuotes = false;
    bool escapedChar = false;
    for (int64_t i = 0; i < int64_t(argString.size()); i++) {
        if (argChar[i] == '\000')
            break;
        // Check the three token types
        if (argChar[i] == '\\' && escapedChar == false) {
            // if we see an escape character, treat the next char as a regular character.
            escapedChar = true;
            continue;
        // If the next character is one that should be counted in a arguement token, count it
        // The critera for a charcter being apart of an arguement is...
        // 1. if it is not a ", space, or tab character OR
        // 2. if the character is escaped by a \ in the previous charcter (see if statement above) OR
        // 3. The charcter is inside a quote (inQuotes) and is not a slash. 
        // If any one of the above match, the charcter should be considered part of a stand alone arg. 
        } else if ((argChar[i] != '\"' && argChar[i] != ' ' &&  argChar[i] != '\t') || escapedChar == true || (inQuotes == true && argChar[i] != '\"')){
            // If we are not already in a token, start a new one at the current position
            if (startpos == -1)
                startpos = i;
            // Reset the escape character.
            escapedChar = false;
        // If we see an unescaped " charcater we know that this is either starting or ending a quoted string.
        } else if (argChar[i] == '\"' ) {
            // take care of the case where a string appears in the middle of a token
            // ex: abc="ZDASDA" 
            // ZDASDA will be split off from the rest
            if (startpos >= 0 && inQuotes == false) {
                args.push_back(std::string(&argChar[startpos], i - startpos));
                startpos = -1;               
            }

            // we are ending the quote if inQuotes is set.
            if (inQuotes == true && argChar[i] == '\"' ) {
                // If we do not have a start position, add a zero length argument to the return vector args.
                // This can happen in cases where a quotation with no charcters inside it appears (ex ""),
                if (startpos == -1) {
                    args.push_back(std::string(""));
                // Otherwise we have a standard quoted string, copy it into the vector ignoring the starting and ending quotation marks.
                } else {
                    args.push_back(std::string(&argChar[startpos], i - startpos));
                }
                // Reset the starting position and the flag to tell if we are in quotes.
                startpos = -1;
                inQuotes = false;
            // If we are starting a new quoted token
            } else if (inQuotes == false && argChar[i] == '\"') {
                // set inquotes flags,
                inQuotes = true;
            } 
            // we are either starting or ending a quoted token
        // If we see a space or a tab
        } else if (argChar[i] == ' ' || argChar[i] == '\t') {
            // we are either ending a token or ignoring if inside ""
            // 
            // This case triggers when there are multiple spaces without any characters (no tokens)
            if (startpos == -1) 
                continue;
            // Otherwise just end the token
            args.push_back(std::string(&argChar[startpos], i - startpos));
            startpos = -1;
        }
    }
    if (startpos != -1 && startpos != argString.size()) {
        args.push_back(std::string(&argChar[startpos], argString.size() - startpos));
        startpos = -1;        
    }
    return 0;
}

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
        SplitArgString(XPLAT_RSH_ARGS, rshArgs);
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

