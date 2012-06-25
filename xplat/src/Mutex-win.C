/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Mutex-win.C,v 1.5 2008/10/09 19:54:13 mjbrim Exp $
#include "Mutex-win.h"
#include <cassert>

namespace XPlat
{

Mutex::Mutex( void )
: data( new WinMutexData )
{
}

Mutex::~Mutex( void )
{
    delete data;
    data = NULL;
}

int Mutex::Lock( void )
{
    if( data != NULL )
        return data->Lock();
    else
        return 0;
}

int Mutex::Unlock( void )
{
    if( data != NULL )
        return data->Unlock();
    else
        return 0;
}

} // namespace XPlat
