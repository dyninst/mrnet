/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#ifndef XPLAT_ATOMIC_H
#define XPLAT_ATOMIC_H

namespace XPlat
{


// a simple class providing portable, thread-safe memory updates
// note: there are likely to be more efficient implementations 
// for some T classes that take advantage of architecture-specific
// features.  For instance, x86/x86_64 have support for atomic memory
// updates for integers up to 64-bits wide, but there is not a portable
// interface for accessing those instructions.
template<class T>
class Atomic
{
private:
    mutable Mutex sync;
    T data;

public:
    Atomic( T _val )
      : data( _val )
    { }

    T Get( void ) const
    {
        T ret;
        sync.Lock();
        ret = data;
        sync.Unlock();
        return ret;
    }

    void Add( T addend )
    {
        sync.Lock();
        data += addend;
        sync.Unlock();
    }
};

} // namespace XPlat

#endif // XPLAT_ATOMIC_H
