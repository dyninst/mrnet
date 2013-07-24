/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "mrnet/MRNet.h"
#include "test_common.h"
#include "test_NativeFilters.h"

#include <string>

using namespace MRN;
using namespace MRN_test;
Test * test;

int test_Sum( Network * net, DataType typ );
int test_Max( Network * net, DataType typ );
int test_Min( Network * net, DataType typ );
int test_Avg( Network * net, DataType typ );

int main(int argc, char **argv)
{
    if( argc != 3 ) {
        fprintf(stderr, "Usage: %s <topology file> <backend_exe>\n", argv[0]);
        return -1;
    }
    const char * topology_file = argv[1];
    const char * backend_exe = argv[2];

    fprintf(stderr,"MRNet C++ Interface *Native Filter* Test Suite\n"
            "--------------------------------------\n"
            "This test suite performs tests that exercise\n"
            "MRNet's built-in filters.\n\n");

    test = new Test("MRNet Native Filter Test");
    const char * dummy_argv=NULL;
    Network * net = Network::CreateNetworkFE( topology_file, backend_exe, &dummy_argv  );
    if( net->has_Error() )
        return -1;
 
    test_Sum( net, CHAR_T );
    test_Sum( net, UCHAR_T );
    test_Sum( net, INT16_T );
    test_Sum( net, UINT16_T );
    test_Sum( net, INT32_T );
    test_Sum( net, UINT32_T );
    test_Sum( net, INT64_T );
    test_Sum( net, UINT64_T );
    test_Sum( net, FLOAT_T );
    test_Sum( net, DOUBLE_T );
  
    Communicator * comm_BC = net->get_BroadcastCommunicator( );
    Stream * stream = net->new_Stream( comm_BC );
    if(stream->send(PROT_EXIT, "") == -1){
        test->print("stream::send(exit) failure\n");
        return -1;
    }

    if(stream->flush() == -1){
        test->print("stream::flush() failure\n");
        return -1;
    }

    test->end_Test();
    delete test;

    delete net;

    return 0;
}

int test_Sum( Network * net, DataType typ )
{
    PacketPtr buf;
    int64_t recv_buf; // we have alignment issues on some 64-bit platforms that require this
    void* recv_val = (void*)&recv_buf;
    int retval=0;
    std::string testname;
    bool success=true;

    int tag = PROT_SUM;

    testname = "test_Sum(" + Type2String[ typ ] + ")";
    test->start_SubTest(testname);

    Communicator * comm_BC = net->get_BroadcastCommunicator( );
    Stream * stream = net->new_Stream( comm_BC, TFILTER_SUM,
                                           SFILTER_WAITFORALL);

    int num_backends = stream->size();

    if( stream->send(tag, "%d", typ) == -1 ){
        test->print("stream::send() failure\n", testname);
        test->end_SubTest(testname, MRNTEST_FAILURE);
        return -1;
    }

    if( stream->flush( ) == -1 ){
        test->print("stream::flush() failure\n", testname);
        test->end_SubTest(testname, MRNTEST_FAILURE);
        return -1;
    }

    retval = stream->recv(&tag, buf);
    assert( retval != 0 ); //shouldn't be 0, either error or block till data
    if( retval == -1){
        //recv error
        test->print("stream::recv() failure\n", testname);
        test->end_SubTest(testname, MRNTEST_FAILURE);
        return -1;
    }
    else{
        //Got data
        char tmp_buf[1024];

	recv_buf = 0; // zero-fill buffer
        if( buf->unpack( Type2FormatString[typ], recv_val ) == -1 ){
            test->print("stream::unpack() failure\n", testname);
            return -1;
        }

        switch(typ){
        case CHAR_T: {
            char val = *(char*)recv_val;
            char comp = 0;
            for(int i=0; i < num_backends; i++)
                comp += CHARVAL;
            if( val != comp ){
                sprintf(tmp_buf,
                        "recv_val(%c) != CHARVAL(%c)*num_backends(%d):%c.\n",
                        val, CHARVAL, num_backends, comp);
                test->print(tmp_buf, testname);
                success = false;
            }
            break;
        }
        case UCHAR_T: {
            char val = *(char*)recv_val;
            unsigned char comp = 0;
            for(int i=0; i < num_backends; i++)
                comp += UCHARVAL;
            if( val != comp ){
                sprintf(tmp_buf,
                        "recv_val(%d) != UCHARVAL(%c)*num_backends(%d):%c.\n",
                        val, UCHARVAL, num_backends, comp);
                test->print(tmp_buf, testname);
                success = false;
            }
            break;
        }
        case INT16_T: {
            int16_t val = *(int16_t*)recv_val;
            int16_t comp = 0;
            for(int i=0; i < num_backends; i++)
                comp += INT16VAL;
            if( val != comp ){
                sprintf(tmp_buf,
                        "recv_val(%" PRIi16 ") != INT16VAL(%" PRIi16 ")*num_backends(%d):%" PRIi16 ".\n",
                        val, INT16VAL, num_backends, comp);
                test->print(tmp_buf, testname);
                success = false;
            }
            break;
        }
        case UINT16_T: {
            uint16_t val = *(uint16_t*)recv_val;
            uint16_t comp = 0;
            for(int i=0; i < num_backends; i++)
                comp += UINT16VAL;
            if( val != num_backends * UINT16VAL ){
                sprintf(tmp_buf,
                        "recv_val(%" PRIu16 ") != UINT16VAL(%" PRIu16 ")*num_backends(%d):%" PRIu16 ".\n",
                        val, UINT16VAL, num_backends, comp);
                test->print(tmp_buf, testname);
                success = false;
            }
            break;
        }
        case INT32_T: {
            int32_t val = *(int32_t*)recv_val;
            int32_t comp = 0;
            for(int i=0; i < num_backends; i++)
                comp += INT32VAL;
            if( val != comp ){
                sprintf(tmp_buf,
                        "recv_val(%" PRIi32 ") != INT32VAL(%" PRIi32 ")*num_backends(%d):%" PRIi32 ".\n",
                        val, INT32VAL, num_backends, comp);
                test->print(tmp_buf, testname);
                success = false;
            }
            break;
        }
        case UINT32_T: {
            uint32_t val = *(uint32_t*)recv_val;
            uint32_t comp = 0;
            for(int i=0; i < num_backends; i++)
                comp += UINT32VAL;
            if( val != comp ){
                sprintf(tmp_buf,
                        "recv_val(%" PRIu32 ") != UINT32VAL(%" PRIu32 ")*num_backends(%d):%" PRIu32 ".\n",
                        val, UINT32VAL, num_backends, comp);
                test->print(tmp_buf, testname);
                success = false;
            }
            break;
        }
        case INT64_T: {
            int64_t val = *(int64_t*)recv_val;
            int64_t comp = 0;
            for(int i=0; i < num_backends; i++)
                comp += INT64VAL;
            if( val != comp ){
                sprintf(tmp_buf,
                        "recv_val(%" PRIi64 " != INT64VAL(%" PRIi64 ")*num_backends(%d):%" PRIi64 ".\n",
                        val, INT64VAL, num_backends, comp);
                test->print(tmp_buf, testname);
                success = false;
            }
            break;
        }
        case UINT64_T: {
            uint64_t val = *(uint64_t*)recv_val;
            uint64_t comp = 0;
            for(int i=0; i < num_backends; i++)
                comp += UINT64VAL;
            if( val != comp ){
                sprintf(tmp_buf,
                        "recv_val(%" PRIu64 " != UINT64VAL(%" PRIu64 ")*num_backends(%d):%" PRIu64 ".\n",
                        val, UINT64VAL, num_backends, comp );
                test->print(tmp_buf, testname);
                success = false;
            }
            break;
        }
        case FLOAT_T: {
            float val = *(float*)recv_val;
            float comp = 0.0;
            for(int i=0; i < num_backends; i++)
                comp += FLOATVAL;
            if( ! compare_Float(val, comp, 2) ){
                sprintf(tmp_buf,
                        "recv_val(%f) != FLOATVAL(%f)*num_backends(%d):%f.\n",
                        val, FLOATVAL, num_backends, comp);
                test->print(tmp_buf, testname);
                success = false;
            }
            break;
        }
        case DOUBLE_T: {
            double val = *(double*)recv_val;
            double comp = 0.0;
            for(int i=0; i < num_backends; i++)
                comp += DOUBLEVAL;
            if( ! compare_Double(val, comp, 5) ){
                sprintf(tmp_buf,
                        "recv_val(%lf) != DOUBLEVAL(%lf)*num_backends(%d):%lf.\n",
                        val, DOUBLEVAL, num_backends, comp);
                test->print(tmp_buf, testname);
                success = false;
            }
            break;
        }
        case CHAR_ARRAY_T:
        case UCHAR_ARRAY_T:
        case INT16_ARRAY_T:
        case UINT16_ARRAY_T:
        case INT32_ARRAY_T:
        case UINT32_ARRAY_T:
        case INT64_ARRAY_T:
        case UINT64_ARRAY_T:
        case FLOAT_ARRAY_T:
        case DOUBLE_ARRAY_T:
        case STRING_T:
        case UNKNOWN_T:
        default:
            success=false;
            return -1;
        }
    }

    if(success){
        test->end_SubTest(testname, MRNTEST_SUCCESS);
    }
    else{
        test->end_SubTest(testname, MRNTEST_FAILURE);
    }
    return 0;
}

#if defined (UNCUT)
int test_Max( Network * net, DataType typ )
{
    PacketPtr buf;
    char recv_val[8];
    int tag=0, retval=0;
    std::string testname;
    bool success=true;

    tag = Type2MaxTag[ typ ];
    testname = "test_Max(" + Type2String[ typ ] + ")";

    test->start_SubTest(testname);

    Communicator * comm_BC = net->get_BroadcastCommunicator( );
    Stream * stream = net->new_Stream(comm_BC, TFILTER_MAX,
                                      SFILTER_WAITFORALL);

    if( stream->send(tag, "") == -1 ){
        test->print("stream::send() failure\n", testname);
        test->end_SubTest(testname, MRNTEST_FAILURE);
        return -1;
    }

    if( stream->flush( ) == -1 ){
        test->print("stream::flush() failure\n", testname);
        test->end_SubTest(testname, MRNTEST_FAILURE);
        return -1;
    }

    retval = stream->recv(&tag, buf);
    assert( retval != 0 ); //shouldn't be 0, either error or block till data
    if( retval == -1){
        test->print("stream::recv() failure\n", testname);
        test->end_SubTest(testname, MRNTEST_FAILURE);
        return -1;
    }
    else{
        char tmp_buf[1024];

        if( buf->unpack( Type2FormatString[typ], recv_val ) == -1 ){
            test->print("stream::unpack() failure\n", testname);
            return -1;
        }

        if( !compare_Vals( recv_val, send_val, typ ) ) {
            char recv_val_string[64];
            char send_val_string[64];
            val2string(recv_val_string, recv_val, typ);
            val2string(send_val_string, send_val, typ);
            sprintf(tmp_buf, "recv_val: %s != send_val: %s\n",
                    recv_val_string, send_val_string );
            test->print(tmp_buf, testname);
            success = false;
        }
    }

    return 0;
}
#endif
