/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Error.h,v 1.3 2007/01/24 19:33:42 darnold Exp $
#ifndef XPLAT_ERROR_H
#define XPLAT_ERROR_H

#include <cstring>
#include <string>

namespace XPlat
{

namespace Error
{

std::string GetErrorString( int code );
bool ETimedOut( int code );
bool EAddrInUse( int code );
bool EConnRefused( int code ); 
bool EInProgress( int code );
bool EInterrupted( int code );
} // namespace Error

} // namespace XPlat

#endif // XPLAT_ERROR_H
