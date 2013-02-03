/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: Monitor.h,v 1.5 2008/10/09 19:54:06 mjbrim Exp $
#ifndef XPLAT_MONITOR_H
#define XPLAT_MONITOR_H

#if !defined(NULL)
#include <cstdlib>
#endif // !defined(NULL)

namespace XPlat
{

class Monitor
{
public:
    class Data
    {
    public:
        Data( void ) { }
        virtual ~Data( void ) { }

        // critical section-related methods
        virtual int Lock( void ) = 0;
        virtual int Unlock( void ) = 0;

        // condition variable-related methods
        virtual int RegisterCondition( unsigned int condid ) = 0;
        virtual int WaitOnCondition( unsigned int condid ) = 0;
        virtual int TimedWaitOnCondition( unsigned int condid, int milliseconds) = 0;
        virtual int SignalCondition( unsigned int condid ) = 0;
        virtual int BroadcastCondition( unsigned int condid ) = 0;
    };

private:
    Data* data;

public:
    Monitor( void );
    virtual ~Monitor( void );

    // critical section-related methods
    virtual int Lock( void );
    virtual int Unlock( void );

    // condition variable-related methods
    virtual int RegisterCondition( unsigned int condid );
    virtual int WaitOnCondition( unsigned int condid );
    virtual int TimedWaitOnCondition( unsigned int condid, int milliseconds);
    virtual int SignalCondition( unsigned int condid );
    virtual int BroadcastCondition( unsigned int condid );
};

} // namespace XPlat

#endif // XPLAT_MONITOR_H
