/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <sstream>
#include "SerialGraph.h"

namespace MRN
{

void SerialGraph::add_Leaf( std::string ihostname, Port iport, Rank irank )
{
    std::ostringstream hoststr;

    hoststr << "[" << ihostname << ":" << std::setw(5) << std::setfill( '0' ) << iport << ":" << irank << ":0" << "]";
    _byte_array += hoststr.str();

    _num_nodes++; _num_backends++;
}

void SerialGraph::add_SubTreeRoot( std::string ihostname, Port iport, Rank irank )
{
    std::ostringstream hoststr;

    hoststr << "[" << ihostname << ":" << std::setw(5) << std::setfill( '0' ) << iport << ":" << irank << ":1";
    _byte_array += hoststr.str();

    _num_nodes++;
}

void SerialGraph::end_SubTree( void )
{
    _byte_array += "]";
}

bool SerialGraph::is_RootBackEnd( void ) const
{
    //format is "host:port:rank:[0|1]: where 0 means back-end and 1 means internal node
    unsigned int idx,num_colons;
    for( idx=0,num_colons=0; num_colons<3; idx++ ){
        if( _byte_array[idx] == ':' )
            num_colons++;
    }
    if( _byte_array[idx] == '0' ) {
        return true;
    }
    else {
        return false;
    }
}

std::string SerialGraph::get_RootHostName()
{
    std::string retval;
    size_t begin=1, end=1; //Byte array begins [ xxx ...

    //find first ':'
    end = _byte_array.find(':', begin);
    assert ( end != std::string::npos );

    retval = _byte_array.substr(begin, end-begin);

    return retval;
}

Port SerialGraph::get_RootPort()
{
    size_t begin, end;
    Port retval = (Port)-1;
    int ret_atoi;

    begin = _byte_array.find(':', 2);
    assert( begin != std::string::npos );
    begin++;
    end = _byte_array.find(':', begin);
    std::string port_string = _byte_array.substr(begin, end-begin);
    ret_atoi = atoi(port_string.c_str());

    if( (ret_atoi <= UINT16_MAX) && (ret_atoi >= 0) ) {
        retval = (Port)ret_atoi;
    }

    return retval;
}

Rank SerialGraph::get_RootRank()
{
    Rank retval = UnknownRank;
    size_t begin=0, end=1; //Byte array begins [ xxx ...

    //find 2nd ':'
    begin = _byte_array.find(':', begin);
    assert(begin != std::string::npos );
    begin++;
    begin = _byte_array.find(':', begin);
    assert(begin != std::string::npos );
    begin++;
    end = _byte_array.find(':', begin);

    std::string rankstring = _byte_array.substr(begin, end-begin);

    retval = atoi(rankstring.c_str());

    return retval;
}


SerialGraph* SerialGraph::get_MySubTree( std::string &ihostname, 
                                         Port iport, Rank irank )
{
    std::ostringstream hoststr;
    std::ostringstream myrank;
    size_t begin, cur, end, rankEnd;
    bool found = false;

    // Temporary patch to disregard port when searching....
    hoststr << "[" << ihostname;
    myrank << irank;

    begin = _byte_array.find( hoststr.str() );
    while (begin != std::string::npos && found != true) {
        cur = begin;
        // Locate the rank number
        begin = _byte_array.find(':', begin);
        assert(begin != std::string::npos );
        begin++;
        begin = _byte_array.find(':', begin);
        assert(begin != std::string::npos );
        begin++;
        rankEnd = _byte_array.find(':', begin);
        std::string rankstring = _byte_array.substr(begin, rankEnd-begin);
        if(myrank.str() == rankstring)
            found = true;
        begin = _byte_array.find( hoststr.str(), begin);
    }



    // hoststr << "[" << ihostname << ":" << std::setw(5) << std::setfill( '0' ) << iport << ":" << irank << ":" ; 
    // mrn_dbg( 5, mrn_printf(FLF, stderr, "SubTreeRoot:'%s' byte_array:'%s'\n",
    //                        hoststr.str().c_str(), _byte_array.c_str() ));

    // begin = _byte_array.find( hoststr.str() );
    if( found == false ) {
        mrn_dbg( 5, mrn_printf(FLF, stderr,
                               "SubTreeRoot:'%s' not found\n",
                               hoststr.str().c_str()) );
        return NULL;
    }

    //now find matching ']'
    //cur=begin;
    begin = cur;
    end=1;
    int num_leftbrackets=1, num_rightbrackets=0;
    while( num_leftbrackets != num_rightbrackets ) {
        cur++;
        end++;
        if( _byte_array[cur] == '[')
            num_leftbrackets++;
        else if( _byte_array[cur] == ']')
            num_rightbrackets++;
    }

    SerialGraph * retval = new SerialGraph( _byte_array.substr(begin, end) );
    
    mrn_dbg( 5, mrn_printf(FLF, stderr, "returned sg byte array :\"%s\"\n",
                           retval->_byte_array.c_str() )); 
    return retval;
}

void SerialGraph::set_ToFirstChild( void )
{
    _buf_idx = _byte_array.find('[',1);
}

bool SerialGraph::set_Port(std::string hostname, Port port, Rank irank)
{  
    std::ostringstream hoststr, port_str;
    size_t begin,port_pos; 
     
    std::string begin_str;

    hoststr << "[" << hostname << ":" << std::setw(5) << std::setfill( '0' ) << UnknownPort << ":" << irank << ":" ;
    port_str <<  port ;
    
    begin = _byte_array.find( hoststr.str() );
    if( begin == std::string::npos ) {
        mrn_dbg( 5, mrn_printf(FLF, stderr,
                 "Host :\"%s\" whose port is to changed is not found in byte_array:\"%s\"\n",
                 hoststr.str().c_str(), _byte_array.c_str() ));
       return false; //return value of false means not succesful set_Port
    }

    begin_str = _byte_array.substr(begin);

    port_pos = begin_str.find_first_of(':', 0);

    _byte_array.replace( begin + port_pos+1, 5, port_str.str() );

    return true;
}

SerialGraph * SerialGraph::get_NextChild()
{
    SerialGraph * retval;
    size_t begin, end, cur;
    const char * buf = _byte_array.c_str();

    if( (_buf_idx == std::string::npos) ||
        (_byte_array.find('[',_buf_idx) == std::string::npos) ) {
        return NULL;
    }

    cur = begin = _buf_idx;
    end = 1;
    int num_leftbrackets=1, num_rightbrackets=0;
    while( num_leftbrackets != num_rightbrackets ) {
        cur++;
        end++;
        if( buf[cur] == '[' )
            num_leftbrackets++;
        else if( buf[cur] == ']' )
            num_rightbrackets++;
    }

    _buf_idx = cur + 1;

    retval = new SerialGraph( _byte_array.substr(begin, end) );
    
    mrn_dbg( 5, mrn_printf(FLF, stderr, "returned sg byte array :\"%s\"\n",
                           retval->_byte_array.c_str()) ); 
    return retval;
}

} /* namespace MRN */
