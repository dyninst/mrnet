/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: TLSKey-win.C,v 1.3 2007/01/24 19:34:30 darnold Exp $
#include <cassert>
#include "TLSKey-win.h"


namespace XPlat
{

TLSKey *XPlat_TLSKey = NULL;

TLSKey::TLSKey( void )
  : data( new WinTLSKeyData )
{
    // nothing else to do
}


WinTLSKeyData::WinTLSKeyData( void )
  : initialized( false )
{
    index = TlsAlloc();
    if( index != TLS_OUT_OF_INDEXES )
    {
        initialized = true;
    }
}


WinTLSKeyData::~WinTLSKeyData( void )
{
    if( initialized )
    {
        TlsFree( index );
    }
}

} // namespace XPlat

