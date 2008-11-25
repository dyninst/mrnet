/****************************************************************************
 * Copyright � 2003-2008 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <iostream>
#include <assert.h>
#include <unistd.h>

#include "mrnet/MRNet.h"
#include "microbench.h"

using namespace MRN;

void BeDaemon( void );


int
main( int argc, char* argv[] )
{
    int ret = 0;
    Stream* stream;
    int tag;
    PacketPtr buf;


    if( getenv( "MRN_DEBUG_BE" ) != NULL )
    {
        fprintf( stderr, "BE: spinning for debugger, pid=%d\n", getpid() );
        bool bCont=false;
        while( !bCont )
        {
            // spin
        }
    }

#if READY
    // become a daemon process
    BeDaemon();
#endif // READY


    // join the MRNet network
    Network * network = new Network( argc, argv );

    // participate in the broadcast/reduction roundtrip latency experiment
    bool done = false;
    while( !done )
    {
        // receive the broadcast message
        tag = 0;
        int rret = network->recv( &tag, buf, &stream );
        if( rret == -1 )
        {
            std::cerr << "BE: Stream::recv() failed" << std::endl;
            return -1;
        }

        if( tag == MB_ROUNDTRIP_LATENCY )
        {
            // extract the value and send it back
            int ival = 0;
            buf->unpack( "%d", &ival );

#if READY
            std::cout << "BE: roundtrip lat, received val " << ival << std::endl;
#else
#endif // READY
            
            // send our value for the reduction
            if( (stream->send( tag, "%d", ival ) == -1) ||
                (stream->flush() == -1) )
            {
                std::cerr << "BE: roundtrip exp reduction failed" << std::endl;
                return -1;
            }
        }
        else
        {
            // we're done with the roundtrip latency experiment
            done = true;
            if( tag != MB_RED_THROUGHPUT )
            {
                std::cerr << "BE: unexpected tag " << tag 
                        << " seen to end roundtrip experiments"
                        << std::endl;
            }
        }
    }


    //
    // participate in the reduction throughput experiment
    //

    // determine the number of reductions required
    assert( tag == MB_RED_THROUGHPUT );
    assert( stream != NULL );
    int nReductions = 0;
    int ival;
    buf->unpack( "%d %d", &nReductions, &ival );

#if READY
    std::cout << "BE: received throughput exp start message"
        << ", nReductions = " << nReductions
        << ", ival = " << ival
        << std::endl;
#else
#endif // READY

    // do the reductions
    for( int i = 0; i < nReductions; i++ )
    {
        // send a value for the reduction
        if( (stream->send( MB_RED_THROUGHPUT, "%d", ival ) == -1 ) ||
            (stream->flush() == -1) )
        {
            std::cerr << "BE: reduction throughput exp reduction failed" 
                    << std::endl;
            return -1;
        }
    }
    // cleanup
    // receive a go-away message
    tag = 0;
    int rret = network->recv( &tag, buf, &stream );
    if( (rret != -1) && (tag != MB_EXIT) )
    {
        std::cerr << "BE: received unexpected go-away tag " << tag << std::endl;
    }
    if( tag != MB_EXIT )
    {
        std::cerr << "BE: received unexpected go-away tag " << tag << std::endl;
    }

    // FE delete network will shut us down, so just go to sleep!!
    sleep(10);
    return 0;
}

void
BeDaemon( void )
{
    // become a background process
    pid_t pid = fork();
    if( pid > 0 )
    {
        // we're the parent - we want our child to be the real job,
        // so we just exit
        exit(0);
    }
    else if( pid < 0 )
    {
        std::cerr << "BE: fork failed to put process in background" << std::endl;
        exit(-1);
    }
    
    // we're the child of the original fork
    pid = fork();
    if( pid > 0 )
    {
        // we're the parent in the second fork - exit
        exit(0);
    }
    else if( pid < 0 )
    {
        std::cerr << "BE: second fork failed to put process in background" << std::endl;
        exit(-1);
    }

    // we were the child of both forks - we're the one who survives
}
