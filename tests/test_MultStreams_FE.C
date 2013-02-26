/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "mrnet/MRNet.h"
#include "test_basic.h"
#include "test_common.h"

#include <assert.h>
#include <vector>
#include <sstream>
using namespace MRN;
using namespace MRN_test;


int test_alltypes( std::vector<Stream*>, bool block );

Test * test;

int main(int argc, char **argv)
{
    if( argc != 4 ) {
        fprintf(stderr, "Usage: %s <topology file> <num streams> <backend_exe>\n", argv[0]);
        exit(-1);
    }
    if (atoi(argv[2]) < 1) {
        fprintf(stderr, "Argument 2, num streams, must be positive. Given %d\n", atoi(argv[2]));
        exit(-1);
    }

    fprintf(stdout, "MRNet C++ Interface *Multiple Streams* Test Suite\n"
            "--------------------------------------------\n"
            "This test suite performs test that exercise\n"
            "MRNet's multiple streams functionality.\n\n");
    fflush( stdout );

    test = new Test( "MRNet Multiple Streams Test", stdout );


    std::vector<Stream *> streams;
    const char * dummy_argv=NULL;
    Network * net = Network::CreateNetworkFE( argv[1], argv[3], &dummy_argv );
    Communicator * comm_BC = net->get_BroadcastCommunicator( );
    assert(comm_BC);

    // create std::vector of streams 
    for (int j = 0; j < atoi(argv[2]); j++) {
        Stream * stream_BC;
        stream_BC = net->new_Stream( comm_BC, TFILTER_NULL, SFILTER_DONTWAIT );
        streams.push_back(stream_BC);
    }

    /* For all the following tests, the bool param indicates *
     * whether the recv should block or not */

    if (test_alltypes(streams, false) == -1) {}
    if (test_alltypes(streams, true) == -1) {}

    std::vector<Stream *>::iterator stream_iter;
    stream_iter = streams.begin();    
    if ((*stream_iter)->send(PROT_EXIT, "") == -1) {
        test->print("stream::send(exit) failure\n");
        return -1;
    }
    if ((*stream_iter)->flush() == -1) {
        test->print("stream::flush() failure\n");
        return -1;
    }
    
    delete net;

    test->end_Test();
    delete test;

    return 0;
}

/* 
 *  test_alltypes():
 *    bcast a packet containing data of all types to all endpoints in stream.
 *    recv  a packet containing data of all types from every endpoint
 */
int test_alltypes( std::vector< Stream * > streams, bool block )
{
    int num_received=0, num_to_receive=0;
    int tag;
    PacketPtr pkt;
    bool success = true;

    char send_char='A', recv_char=0;
    unsigned char send_uchar='B', recv_uchar=0;
    int16_t send_short=-17, recv_short=0;
    uint16_t send_ushort=17, recv_ushort=0;
    int32_t send_int=-17, recv_int=0;
    int32_t send_uint=17, recv_uint=0;
    int64_t send_long=-17, recv_long=0;
    int64_t send_ulong=17, recv_ulong=0;
    float send_float=(float)123.23412, recv_float=0;
    double send_double=123.23412, recv_double=0;
    char *send_string=strdup("Test String"), *recv_string=0;

    std::string testname( "test_all(" );

    if( block ) {
        testname += "blocking_recv, ";
    }
    else {
        testname += "non-blocking_recv, ";
    }
    std::stringstream size;
    size << streams.size();
    testname += size.str();
    testname += " streams)";

    test->start_SubTest(testname);

    std::vector<Stream *>::iterator stream_iter;

    for (stream_iter = streams.begin();
         stream_iter != streams.end();
         ++stream_iter) {

        num_to_receive = (*stream_iter)->size();
        if( num_to_receive == 0 ) {
            test->print("No endpoints in stream\n", testname);
            test->end_SubTest(testname, MRNTEST_NOTRUN);
            return -1;
        }


        if( (*stream_iter)->send( PROT_ALL, "%c %uc %hd %uhd %d %ud %ld %uld %f %lf %s",
                          send_char, send_uchar,
                          send_short, send_ushort,
                          send_int, send_uint,
                          send_long, send_ulong,
                          send_float, send_double, send_string ) == 1 ) {
            test->print("stream::send() failure\n", testname);
            test->end_SubTest(testname, MRNTEST_FAILURE);
            return -1;
        }

        if( (*stream_iter)->flush() == -1 ) {
            test->print("stream::flush() failure\n", testname);
            test->end_SubTest(testname, MRNTEST_FAILURE);
            return -1;
        }
    }

    int stream_num = 1;
    for (stream_iter = streams.begin();
         stream_iter != streams.end();
         ++stream_iter) {
        // reset num_received
        num_received = 0;
        do {
            int retval;

            //In non-blocking mode we sleep b/n recv attempts
            if( ! block ) sleep(1);

            retval = (*stream_iter)->recv( &tag, pkt, block );
            if( retval == -1 ) {
                //recv error
                test->print("stream::recv() failure\n", testname);
                test->end_SubTest(testname, MRNTEST_FAILURE);
                return -1;
            }
            else if ( retval == 0 ) {
                //No data available
            }
            else {
                //Got data
                char tmp_buf[2048];

                num_received++;
#if defined(DEBUG)
                sprintf(tmp_buf, "Stream %d received %d packets; %d left.\n",
                        stream_num, num_received, num_to_receive-num_received);
                test->print(tmp_buf, testname);
#endif

                if( pkt->unpack( "%c %uc %hd %uhd %d %ud %ld %uld %f %lf %s",
                                 &recv_char, &recv_uchar,
                                 &recv_short, &recv_ushort,
                                 &recv_int, &recv_uint, &recv_long, &recv_ulong,
                                 &recv_float, &recv_double, &recv_string ) == 1 ) {
                    test->print("stream::unpack() failure\n", testname);
                    success = false;
                }

                if( ( send_char != recv_char ) ||
                    ( send_uchar != recv_uchar ) ||
                    ( send_short != recv_short ) ||
                    ( send_ushort != recv_ushort ) ||
                    ( send_int != recv_int ) ||
                    ( send_uint != recv_uint ) ||
                    ( send_long != recv_long ) ||
                    ( send_ulong != recv_ulong ) ||
                    ( !compare_Float( send_float, recv_float, 3 ) ) ||
                    ( !compare_Double( send_double, recv_double, 3 ) ) ||
                    ( strcmp(send_string, recv_string)) ) {
                    sprintf(tmp_buf, "Values sent != Values received failure.\n"
                            "  send_char=%c recv_char=%c\n"
                            "  send_uchar=%c recv_uchar=%c\n"
                            "  send_short=%hd recv_short=%hd\n"
                            "  send_ushort=%u recv_ushort=%u\n"
                            "  send_int=%d recv_int=%d\n"
                            "  send_uint=%u recv_uint=%u\n"
                            "  send_long=%lld recv_long=%lld\n"
                            "  send_ulong=%llu recv_ulong=%llu\n"
                            "  send_float=%f recv_float=%f\n"
                            "  send_double=%lf recv_double=%lf\n"
                            "  send_string=%s recv_string=%s\n",
                            send_char, recv_char, send_uchar, recv_uchar,
                            send_short, recv_short, send_ushort, recv_ushort,
                            send_int, recv_int, send_uint, recv_uint,
                            (long long int)send_long, (long long int)recv_long,
                            (long long unsigned int)send_ulong, 
                            (long long unsigned int)recv_ulong,
                            send_float, recv_float, send_double, recv_double,
                            send_string, recv_string);
                    test->print(tmp_buf, testname);
                    success = false;
                }
            }
        } while( num_received < num_to_receive );
        stream_num++;
    }// for each stream
    
    if( success ) {
        test->end_SubTest(testname, MRNTEST_SUCCESS);
        return 0;
    }
    else {
        test->end_SubTest(testname, MRNTEST_FAILURE);
        return -1;
    }

    return 0;
}
