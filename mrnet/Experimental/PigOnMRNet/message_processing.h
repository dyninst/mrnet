// Copyright Paradyn, 2008: contact jolly@cs.wisc.edu for details

#if !defined( message_processing_h )
#define message_processing_h 1

#include "boar.h"

#include <string>
using std::string;

#include <vector>
using std::vector;

inline string
serialize(
    tuple& to_serialize
    )
{
    string to_return("{");
    for(unsigned int i = 0; i < to_serialize.size(); ++i)
    {
        to_return += " ";
        to_return += to_serialize[i];
    }
    to_return += " }\n";
    return to_return;
}

inline const char *
get_shard_filename(
    string startup_string
    )
{
    return startup_string.c_str(); 
}

#endif

