// Copyright Paradyn, 2008: contact jolly@cs.wisc.edu for details

#if !defined( physical_plan_h )
#define physical_plan_h 1

#include "physical_operator.h"
#include "message_processing.h"

#include <vector>
using std::vector;

#include <string>
using std::string;

class physical_plan
{
private:

    vector<physical_operator> operators;
    
public:

    physical_plan();
    ~physical_plan();

    void add_operator(const physical_operator& op);
    string get_manifest();

};

#endif // physical_plan_h

