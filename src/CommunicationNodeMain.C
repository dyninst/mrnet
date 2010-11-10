/****************************************************************************
 *  Copyright 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <list>

#include "InternalNode.h"
#include "Message.h"
#include "utils.h"
#include "mrnet/MRNet.h"
#include "xplat/NetUtils.h"


using namespace MRN;
using namespace XPlat;

void BeDaemon( void );

int main(int argc, char **argv)
{
    int ret = 0;
    Network *net = NULL;

    try
    {
        // parse the command line for platform-independent arguments
        //
        // skip past the executable name
        argc--;
        argv++;

        // check whether to detach from the terminal
        if( argc >= 1 )
        {
            if( strcmp( argv[0], "-n" ) == 0 )
            {
                BeDaemon();

                // skip past this flag
                argc--;
                argv++;
            }
        }

        // build our object allowing access to the MRNet network

        net = Network::CreateNetworkIN( argc, argv );
        if( (net == NULL) || net->has_Error() )
            return 1;
        
        // handle events
        net->waitfor_ShutDown();
    }
    catch( std::exception& e )
    {
        mrn_dbg( 1, mrn_printf( FLF, stderr, e.what() ) );
        ret = 1;
    }

    delete net;
    net = NULL;

    return ret;
}

void BeDaemon( void )
{
#ifndef os_windows
# ifndef arch_crayxt

    // become a background process
    pid_t pid = fork();
    if( pid > 0 ) {
        // we're the parent - we want our child to be the real job,
        // so we just exit
        exit(0);
    }
    else if( pid < 0 ) {
        fprintf( stderr, "BE: fork failed to put process in background\n" );
        exit(-1);
    }
    
    // we're the child of the original fork
    pid = fork();
    if( pid > 0 ) {
        // we're the parent in the second fork - exit
        exit(0);
    }
    else if( pid < 0 ) {
        fprintf( stderr, "BE: 2nd fork failed to put process in background\n" );
        exit(-1);
    }
    // we were the child of both forks - we're the one who survives

# endif /* !defined(arch_crayxt) */
#else
    //TODO: figure out how to run in background on windows
#endif /* !defined(os_windows) */
}
