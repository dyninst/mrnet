// Copyright Paradyn, 2008: contact jolly@cs.wisc.edu for details

#if !defined( physical_operator_h )
#define physical_operator_h 1

#include "boar.h"

class physical_operator
{
public:

    int operator_id, host_node_id;
    physical_op_type op_type;
    vector<int> inputs, outputs;

    physical_operator()
    {
    }
    
    ~physical_operator()
    {
    }

};

#endif // physical_operator_h

