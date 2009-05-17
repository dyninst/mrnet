/**************************************************************************
 * Copyright 2003-2009   James Jolly, Michael J. Brim, Barton P. Miller   *
 *                Detailed MRNet usage rights in "LICENSE" file.          *
 **************************************************************************/

#if !defined(hetero_test_h)
#define hetero_test_h 1

#include <vector>
#include <string>
#include <sstream>
using std::vector;
using std::string;
using std::ostringstream;

#include "mrnet/Types.h"
#include "mrnet/Packet.h"
#include "mrnet/MRNet.h"

typedef enum {
    PROT_EXIT = FirstApplicationTag, 
    PROT_TEST
} Protocol;

#endif /* hetero_test_h */
