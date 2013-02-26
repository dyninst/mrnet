// Copyright Paradyn, 2008: contact jolly@cs.wisc.edu for details

#if !defined( boar_h )
#define boar_h 1

#include <string>
using std::string;

#include <vector>
using std::vector;

#include "mrnet/Types.h"

#define uint uint32_t

// boar-specific defines, types, etc...

typedef enum
{
    EXIT=FirstApplicationTag,
    DATA,
    START,
    DONE
}
Protocol;

typedef enum
{
    LOAD,
    STORE,
    FILTER,
    ORDER,
    CROSS,
    DISTINCT,
    COGROUP,
    UNION,
    SPLIT,
    GENERATE,
    FLATTEN,
    LIMIT
}
physical_op_type;

typedef vector<string> tuple;

#endif // boar_h

