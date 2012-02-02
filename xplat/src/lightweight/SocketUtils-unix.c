/****************************************************************************
 * Copyright © 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: SocketUtils-unix.c,v 1.2 2012/01/26 13:40:54 samanas Exp $
#include <unistd.h>
#include "xplat_lightweight/Types.h"
#include "xplat_lightweight/SocketUtils.h"

int XPlat_SocketUtils_Close( int sock )
{
    return close( sock );
}
