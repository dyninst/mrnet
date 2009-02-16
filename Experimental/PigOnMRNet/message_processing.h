// Copyright Paradyn, 2008: contact jolly@cs.wisc.edu for details

#if !defined( message_processing_h )
#define message_processing_h 1

#include <string>
using std::string;

#include <sstream>
using std::stringstream;

#include <vector>
using std::vector;

#include <cstdlib>

#include "boar.h"

inline
string
serialize(
    const tuple& to_serialize
    )
{
    uint i;
    string to_return;
    for(i = 0; i < to_serialize.size(); ++i)
    {
        to_return += " ";
        to_return += to_serialize[i];
    }
    to_return += " ";
    return to_return;
}

inline
tuple
unserialize(
    const string& to_unserialize
    )
{
    string token;
    tuple to_return;
    stringstream token_stream(to_unserialize);
    while(token_stream >> token)
    {
        to_return.push_back(token);
    }
    return to_return;
}

inline
const char *
get_shard_filename(
    const string& startup_string
    )
{
    return startup_string.c_str(); 
}

inline
string
to_string(
    int i
    )
{
    stringstream ss;
    ss << i;
    string result = ss.str();
    return result;
}

inline
char*
copy_into_new_buffer(
    const string& to_convert
    )
{
    uint i;
    char* buf;
    buf = (char*)malloc(to_convert.size() + 1);  
    for(i = 0; i < to_convert.size(); ++i)
    {
        buf[i] = to_convert[i];
    }
    buf[i] = '\0';
    return buf;
}

#endif // message_processing_h

