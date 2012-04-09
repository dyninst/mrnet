/****************************************************************************
 * Copyright 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller   *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include <cstring>

#include "mrnet/MRNet.h"
#include "xt_exception.h"
#include "xt_protocol.h"
#include "xt_util.h"

int
main( int argc, char* argv[] )
{
    int ret = 0;
    MRN::Network* net = NULL;

    try
    {
        int myNid = FindMyNid();

        // connect to the MRNet
        net = MRN::Network::CreateNetworkBE( argc, argv );
        assert( net != NULL );
        int myRank = net->get_LocalRank();
        //fprintf( stderr, "BE[%d] done creating MRNet net BE\n", myRank );
        //fflush( stderr );

        // receive the ALPS apid for the application to be monitored
        MRN::Stream* strm = NULL;
        MRN::PacketPtr pkt;
        int tag = 0;
        int rret = net->recv( &tag, pkt, &strm );
        //fprintf( stderr, "BE[%d] received ALPS apid msg\n", myRank );
        //fflush( stderr );
        if( rret == -1 )
        {
            throw XTMException( "BE[%d] failed to receive control message from FE" );
        }
        if( tag != XTM_APID )
        {
            std::ostringstream mstr;
            mstr << "BE[" << myRank << "] expected XTM_APID message, got tag=" << tag << std::ends;
            throw XTMException( mstr.str().c_str() );
        }
        uint64_t apid = (uint64_t)-1;
        pkt->unpack( "%uld", &apid );
        //fprintf( stderr, "BE[%d] received apid=%lu\n", myRank, apid );
        //fflush( stderr );

#ifdef TOOL_CODE_GOES_HERE
        // ---- BEGIN TOOL-SPECIFIC CODE REGION ----
        // 1. attach to local application process(es)
        // 2. indicate to tool FE that we are attached
        // 3. handle application process events until it finishes
        // ---- END TOOL-SPECIFIC CODE REGION ----
#else
        // fake to the FE as if we have connected to the application and
        // waited for it to finish
        tag = XTM_APPOK;
        int dummyVal = 1;
        int sret = strm->send( tag, "%d", dummyVal );
        if( sret == -1 )
        {
            fprintf( stderr, "BE[%d] send of app OK msg failed\n", myRank );
            fflush( stderr );
        }
        int fret = strm->flush();
        if( fret == -1 )
        {
            fprintf( stderr, "BE[%d] flush of app OK msg failed\n", myRank );
            fflush( stderr );
        }
        if( (sret == -1) || (fret == -1) )
        {
            std::ostringstream mstr;
            mstr << "BE[" << myRank << "] failed to send app OK msg to FE" << std::ends;
            throw XTMException( mstr.str().c_str() ); 
        }
        //fprintf( stderr, "BE[%d] sent app OK message to FE\n", myRank );
        //fflush( stderr );

        sleep( 5 );
#endif

        //fprintf( stderr, "BE[%d] sending app done message to FE\n", myRank );
        //fflush( stderr );
        tag = XTM_APPDONE;
        int appExitCode = 1;
        sret = strm->send( tag, "%d", appExitCode );
        if( sret == -1 )
        {
            fprintf( stderr, "BE[%d] send of app done msg failed\n", myRank );
            fflush( stderr );
        }
        fret = strm->flush();
        if( fret == -1 )
        {
            fprintf( stderr, "BE[%d] flush of app done msg failed\n", myRank );
            fflush( stderr );
        }
        if( (sret == -1) || (fret == -1) )
        {
            std::ostringstream mstr;
            mstr << "BE[" << myRank << "] failed to send app exit code to FE" << std::ends;
            throw XTMException( mstr.str().c_str() ); 
        }
        //fprintf( stderr, "BE[%d] sent app done message to FE\n", myRank );
        //fflush( stderr );
        
        // wait for termination message from FE
        rret = net->recv( &tag, pkt, &strm );
        if( rret == -1 )
        {
            std::ostringstream mstr;
            mstr << "BE[" << myRank << "] failed to receive control message from FE" << std::ends;
            throw XTMException( mstr.str().c_str() ); 
        }
        if( tag != XTM_EXIT )
        {
            std::ostringstream mstr;
            mstr << "BE[" << myRank << "] expected XTM_EXIT message, got tag=" << tag << std::ends;
            throw XTMException( mstr.str().c_str() );
        }

        // clean up
        //fprintf( stderr, "BE[%d] exiting\n", myRank );
        //fflush( stderr );

        net->waitfor_ShutDown();
        delete net;
    }
    catch( std::exception& e )
    {
        fprintf( stderr, "!!BE EXCEPTION!! %s\n", e.what() );
        ret = 1;
    }

    return ret;
}

