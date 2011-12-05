/****************************************************************************
 * Copyright © 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "mrnet/MRNet.h"
#include "MultThdStreams.h"
#include "xplat/Thread.h"

#include <iostream>
#include <string>
using namespace std;

using namespace MRN;

unsigned int min_val=0;
unsigned int max_val=0;
unsigned long bits_val;

void* WaveChkMain(void *arg) {
    int retval, tag, num_iters = 10;
    PacketPtr p;
    Stream *wave_chk_strm = (Stream *)arg;

    unsigned long l_bits, tmp_bits;
    unsigned int l_min, l_max, tmp_min, tmp_max;
    XPlat::Thread::Id me_thd = XPlat::Thread::GetId();

    if(wave_chk_strm->send(PROT_WAVE_CHECK, "%d", num_iters) == -1) {
        fprintf(stderr, "stream::send(CHECK_WAVE) failure\n");
        return NULL;
    }
    if(wave_chk_strm->flush() == -1){
        fprintf( stderr, "stream::flush() failure\n" );
        return NULL;
    }
    for(int i = 0; i < num_iters; i++) {
        retval = wave_chk_strm->recv(&tag, p);
        if( retval == -1) {
            //recv error
            fprintf( stderr, "stream::recv() failure\n" );
            return NULL;
        }
        if(tag != PROT_WAVE_CHECK) {
            fprintf(stderr, "FE: WaveChkMain received incorrect packet: %d\n", tag);
            fprintf(stderr, "thread %lu recv'd: %d, %s\n", me_thd,
                             tag, p->get_FormatString());
            return NULL;
        }
        p->unpack("%uld %ud %ud", &tmp_bits, &tmp_max, &tmp_min);
        // Trust the first message we get
        if(i == 0) {
            l_bits = tmp_bits;
            l_max = tmp_max;
            l_min = tmp_min;
            printf("FE: Received initial values. All subsequent checks should "
                   "match: min=%d max=%d bits=%lu.\n", l_min, l_max, l_bits);
        } else {
            if(tmp_bits != l_bits) {
                fprintf(stderr, "bit set %d: %lu != %lu\n",i, tmp_bits, l_bits);
            } else {
                printf("FE: bit set %d check successful: %lu\n", i, tmp_bits);
            }
            if(tmp_max != l_max) {
                fprintf(stderr, "max val %d: %u != %u\n", i, tmp_max, l_max);
            } else {
                printf("FE: max val %d check successful: %u\n", i, tmp_max);
            }
            if(tmp_min != l_min) {
                fprintf(stderr, "min val %d: %u != %u\n", i, tmp_min, l_min);
            } else {
                printf("FE: min val %d check successful: %u\n", i, tmp_min);
            }
        }
    }

    return NULL;

}

void* MaxThdMain(void * arg) {
    int retval, tag;
    PacketPtr p;
    Stream *max_stream = (Stream *) arg;
    // Check max
    for(int i = 0; i < 2; i++) {
        if(max_stream->send(PROT_CHECK_MAX, "") == -1) {
            fprintf( stderr, "stream::send(exit) failure\n" );
            return NULL;
        }
        if(max_stream->flush() == -1){
            fprintf( stderr, "stream::flush() failure\n" );
            return NULL;
        }
        retval = max_stream->recv(&tag, p);
        if( retval == -1){
            //recv error
            fprintf( stderr, "stream::recv() failure\n" );
            return NULL;
        }
        if(tag != PROT_CHECK_MAX) {
            fprintf(stderr, "FE: MaxThdMain received incorrect packet: %d\n", tag);
        }
        unsigned int good_max;
        p->unpack("%ud", &good_max);
        fprintf( stdout, "FE: check of MAX ");
        if( good_max != max_val ) 
            fprintf( stdout, "failed, filter %u != check %u (%d)\n", 
                    max_val, good_max, i );
        else fprintf( stdout, "successful %u (%d)\n", max_val, i );
        fflush(stdout);
    }

    delete max_stream;

    return NULL;
}

void* MinThdMain(void * arg) {
    int retval, tag;
    PacketPtr p;
    Stream *min_stream = (Stream *) arg;
    // Check min
    for(int i = 0; i < 2; i++) {
        if(min_stream->send(PROT_CHECK_MIN, "") == -1){
            fprintf( stderr, "stream::send(exit) failure\n" );
            return NULL;
        }
        if(min_stream->flush() == -1){
            fprintf( stderr, "stream::flush() failure\n" );
            return NULL;
        }
        retval = min_stream->recv(&tag, p);
        if( retval == -1){
            //recv error
            fprintf( stderr, "stream::recv() failure\n" );
            return NULL;
        }
        if(tag != PROT_CHECK_MIN) {
            fprintf(stderr, "FE: MinThdMain received incorrect packet: %d\n", tag);
        }
        unsigned int good_min;
        p->unpack("%ud", &good_min);
        fprintf( stdout, "FE: check of MIN ");
        if( good_min != min_val ) 
            fprintf( stdout, "failed, filter %u != check %u\n", 
                    min_val, good_min );
        else fprintf( stdout, "successful %u (%d)\n", min_val, i);
        fflush(stdout);
    }
    delete min_stream;

    return NULL;
}

void* PctThdMain(void * arg) {
    int retval, tag;
    PacketPtr p;
    Stream *pct_stream = (Stream *) arg;
    // Check percentiles
    for(int i = 0; i < 2; i++) {
        if(pct_stream->send(PROT_CHECK_PCT, "") == -1){
            fprintf( stderr, "stream::send(exit) failure\n" );
            return NULL;
        }
        if(pct_stream->flush() == -1){
            fprintf( stderr, "stream::flush() failure\n" );
            return NULL;
        }
        retval = pct_stream->recv(&tag, p);
        if( retval == -1){
            //recv error
            fprintf( stderr, "stream::recv() failure\n" );
            return NULL;
        }
        if(tag != PROT_CHECK_PCT) {
            fprintf(stderr, "FE: PctThdMain received incorrect packet: %d\n", tag);
        }
        unsigned long good_pct;
        p->unpack("%uld", &good_pct);
#ifdef compiler_sun
        string bits = fr_bin_set(bits_val).to_string();
        string good = fr_bin_set(good_pct).to_string();
#else
        string bits = fr_bin_set(bits_val).to_string<char,char_traits<char>,allocator<char> >();
        string good = fr_bin_set(good_pct).to_string<char,char_traits<char>,allocator<char> >();
#endif
        fprintf( stdout, "FE: check of PERCENTILES ");
        if( good_pct != bits_val ) 
            fprintf( stdout, "failed, filter %s != check %s (%d)\n", 
                     bits.c_str(), good.c_str(), i );
        else fprintf( stdout, "successful %s (%d)\n", bits.c_str(), i );
        fflush(stdout);
    }
    delete pct_stream;

    return NULL;
}

int num_failure_callbacks = 0;
void Failure_Callback( Event* evt, void* )
{
    if( (evt->get_Class() == Event::TOPOLOGY_EVENT) &&
        (evt->get_Type() == TopologyEvent::TOPOL_REMOVE_NODE) )
        num_failure_callbacks++;
}

int num_change_callbacks = 0;
void ParentFailure_Callback( Event* evt, void* )
{
    if( (evt->get_Class() == Event::TOPOLOGY_EVENT) &&
        (evt->get_Type() == TopologyEvent::TOPOL_CHANGE_PARENT) )
        num_change_callbacks++;
}

int main(int argc, char **argv)
{
    int tag, retval;
    PacketPtr p;

    if( argc != 4 ){
        fprintf( stderr, "Usage: %s <topology file> <backend_exe> <so_file>\n", argv[0] );
        exit(-1);
    }
    const char * topology_file = argv[1];
    const char * backend_exe = argv[2];
    const char * so_file = argv[3];
    const char * dummy_argv=NULL;
    void *min_ret;
    void *max_ret;
    void *pct_ret;

    // This Network() cnstr instantiates the MRNet internal nodes, according to the
    // organization in "topology_file," and the application back-end with any
    // specified cmd line args
    Network * net = Network::CreateNetworkFE( topology_file, backend_exe, &dummy_argv );
    if( net->has_Error() ) {
        net->perror( "Network creation failed" );
        return -1;
    }

    bool cbrett = net->register_EventCallback( Event::TOPOLOGY_EVENT,
                                               TopologyEvent::TOPOL_REMOVE_NODE,
                                               Failure_Callback, NULL );
    if(cbrett == false) {
        fprintf( stdout, "Failed to register callback for node failure topology event\n" );
        delete net;
        return -1;
    }

    cbrett = net->register_EventCallback( Event::TOPOLOGY_EVENT,
                                          TopologyEvent::TOPOL_CHANGE_PARENT,
                                          ParentFailure_Callback, NULL );
    if(cbrett == false) {
        fprintf( stdout, "Failed to register callback for parent change topology event\n" );
        delete net;
        return -1;
    }

    NetworkTopology* nettop = net->get_NetworkTopology();
    if( nettop->get_Root()->find_SubTreeHeight() < 2 ) {
        fprintf( stderr, "Please use a topology with depth >= 2.\n"
                 "There needs to be at least one mrnet_commnode process to kill.\n" );
        delete net;
        return -1;
    }

    // Make sure path to "so_file" is in LD_LIBRARY_PATH
    int filter_id = net->load_FilterFunc( so_file, "IntegerPercentiles" );
    if( filter_id == -1 ){
        fprintf( stderr, "Network::load_FilterFunc() failure\n" );
        delete net;
        return -1;
    }
    int chk_filter_id = net->load_FilterFunc( so_file, "BitsetOr" );
    if( chk_filter_id == -1 ){
        fprintf( stderr, "Network::load_FilterFunc() failure\n" );
        delete net;
        return -1;
    }

    // A Broadcast communicator contains all the back-ends
    Communicator * comm_BC = net->get_BroadcastCommunicator( );

    // Create a stream that will use the IntegerPercentiles filter for aggregation
    
    Stream * add_stream = net->new_Stream( comm_BC, filter_id,
                                           SFILTER_WAITFORALL );

    // Broadcast a control message to back-ends to send us "num_iters"
    // waves of integers
    tag = PROT_START;
    unsigned int num_iters = 10;
    printf("FE: sending PROT_START\n");
    if( add_stream->send( tag, "%ud", num_iters ) == -1 ){
        fprintf( stderr, "stream::send() failure\n" );
        return -1;
    }
    if( add_stream->flush( ) == -1 ){
        fprintf( stderr, "stream::flush() failure\n" );
        return -1;
    }

    // We expect "num_iters" aggregated responses from all back-ends
    for( unsigned int i=0; i < num_iters; i++ ) {

        retval = add_stream->recv( &tag, p );
        assert( retval != 0 ); //shouldn't be 0, either error or block till data
        if( retval == -1){
            //recv error
            fprintf( stderr, "stream::recv() failure\n" );
            return -1;
        }
        assert( tag == PROT_WAVE );
        if( p->unpack( "%uld %ud %ud", &bits_val, &max_val, &min_val ) == -1 ) {
            fprintf( stderr, "stream::unpack() failure\n" );
            return -1;
        }
        fprintf( stdout, "FE: min %u max %u bits %lu\n", 
                 min_val, max_val, bits_val );
        fflush(stdout);
    }

    // 3 new streams for checking of values (expect user to kill CP)
    Stream *wave_chk_strm0 = net->new_Stream(comm_BC, filter_id,
                                             SFILTER_WAITFORALL);
    
    Stream *wave_chk_strm1 = net->new_Stream(comm_BC, filter_id,
                                             SFILTER_WAITFORALL);

    Stream *wave_chk_strm2 = net->new_Stream(comm_BC, filter_id,
                                             SFILTER_WAITFORALL);

    // Start up threads to test threaded fault recovery/Network_recv
    printf("****** Double-checking PROT_WAVE values with 3 threads ******\n");
    XPlat::Thread::Id wcs0;
    XPlat::Thread::Id wcs1;
    XPlat::Thread::Id wcs2;
    void *wcs0_ret;
    void *wcs1_ret;
    void *wcs2_ret;

    XPlat::Thread::Create(WaveChkMain, (void *)wave_chk_strm0, &wcs0);
    XPlat::Thread::Create(WaveChkMain, (void *)wave_chk_strm1, &wcs1);
    XPlat::Thread::Create(WaveChkMain, (void *)wave_chk_strm2, &wcs2);

    XPlat::Thread::Join(wcs0, &wcs0_ret);
    XPlat::Thread::Join(wcs1, &wcs1_ret);
    XPlat::Thread::Join(wcs2, &wcs2_ret);

    delete add_stream;

    
    /* Create and register streams for each thread */
    /* Max stream */
    Stream * max_stream = net->new_Stream( comm_BC, TFILTER_MAX,
                                           SFILTER_WAITFORALL );
    max_stream->send(PROT_REG_MAX, "");

    /* Min stream */
    Stream * min_stream = net->new_Stream( comm_BC, TFILTER_MIN,
                                           SFILTER_WAITFORALL );
    min_stream->send(PROT_REG_MIN, "");

    /* Pct stream */
    Stream * pct_stream = net->new_Stream( comm_BC, chk_filter_id,
                                           SFILTER_WAITFORALL );
    pct_stream->send(PROT_REG_PCT, "");

    /* Receive acknowledgment that BE register streams correctly */
    //printf("done sending PROT_REG_STRM. Waiting for ack on max_stream..\n");
    if(max_stream->recv(&tag, p) != 1) {
        fprintf(stderr, "FE: Stream::recv failed on stream register ack\n");
        return 1;
    }

    if(max_stream->flush() == -1) {
        fprintf(stderr, "FE: Stream::flush failed. max_stream closed.\n");
        return 1;
    }

    /* Check for correct ack tag before starting threads */
    if(tag != PROT_REG_STRM) {
        fprintf(stderr, "Received wrong tag as ack for stream register.\n");
        return 1;
    }

    //printf("Received register stream ack\n");
    
    printf("****** Ping-pong check of PROT_WAVE values ******\n");
    XPlat::Thread::Id min_thd;
    XPlat::Thread::Id max_thd;
    XPlat::Thread::Id pct_thd;

    XPlat::Thread::Create(MaxThdMain, (void *)max_stream, &max_thd);
    XPlat::Thread::Create(MinThdMain, (void *)min_stream, &min_thd);
    XPlat::Thread::Create(PctThdMain, (void *)pct_stream, &pct_thd);

    XPlat::Thread::Join(min_thd, &min_ret);
    XPlat::Thread::Join(max_thd, &max_ret);
    XPlat::Thread::Join(pct_thd, &pct_ret);

    // Test case of race when 1 thread is receiving on network and a
    // PROT_NEW_STREAM is received.
    
    // Tell back-ends to exit
    Stream * ctl_stream = net->new_Stream( comm_BC, TFILTER_MAX,
                                           SFILTER_WAITFORALL );
    if(ctl_stream->send(PROT_EXIT, "") == -1){
        fprintf( stderr, "stream::send(exit) failure\n");
        return -1;
    }

    if(ctl_stream->flush() == -1){
        fprintf( stderr, "stream::flush() failure\n");
        return -1;
    }

    retval = ctl_stream->recv(&tag, p);
    if( retval == -1){
         //recv error
         fprintf( stderr, "stream::recv() failure\n");
         return -1;
    }
    delete ctl_stream;
    delete net; //destructor will cause all internal and leaf tree nodes to exit

    return 0;
}
