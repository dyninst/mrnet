/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Error-unix.C,v 1.3 2007/01/24 19:33:58 darnold Exp $
#include <cstring>
#include <cerrno>
#include "xplat/Error.h"

namespace XPlat
{

std::string
Error::GetErrorString( int code )
{
    return strerror( code );
}

bool
Error::ETimedOut( int code )
{
    return (code == ETIMEDOUT);
}


bool
Error::EAddrInUse( int code )
{
    return (code == EADDRINUSE);
}

bool
Error::EConnRefused( int code )
{
    return (code == ECONNREFUSED);
}


bool
Error::EInProgress( int code )
{
    return (code == EINPROGRESS);
}

bool
Error::EInterrupted( int code )
{
    return (code == EINTR);
}

} // namespace XPlat
