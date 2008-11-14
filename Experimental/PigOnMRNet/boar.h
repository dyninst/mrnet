// Copyright Paradyn, 2008: contact jolly@cs.wisc.edu for details

#if !defined( boar_h )
#define boar_h 1

#include <iostream>
using std::cerr;

#include <string>
using std::string;

#include <vector>
using std::vector;

#include "mrnet/Types.h"

// boar-specific defines, types, etc...

#define  INFO cerr << " INFO: " 
#define  WARN cerr << " WARN: " 
#define ERROR cerr << "ERROR: " 

typedef enum
{
    EXIT=FirstApplicationTag,
    DATA,
    START,
    DONE
}
Protocol;

typedef vector<string> tuple;

#endif // boar_h

