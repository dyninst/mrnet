/****************************************************************************
 * Copyright © 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: SocketUtils-win.c,v 3.1 2012/01/20 17:41:41 samanas Exp $
#include <winsock2.h>
#include "xplat_lightweight/SocketUtils.h"


int XPlat_SocketUtils_Close( int sock )
{
    return closesocket( (SOCKET)sock );
}

