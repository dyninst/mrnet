// Copyright Paradyn, 2008: contact jolly@cs.wisc.edu for details

#if !defined( physical_plan_h )
#define physical_plan_h 1

#include "physical_operator.h"
#include "message_processing.h"
#include "logger.h"

#include <iostream>
using std::cerr;

#include <vector>
using std::vector;

#include <string>
using std::string;

#include <sstream>
using std::istringstream;

class physical_plan
// a collection of physical operators that are loaded by the MRNet nodes,
// explaining each node's job within a Pig Latin query
{
private:

    vector<physical_operator> operators;

public:

    physical_plan();
    ~physical_plan();

    void add_operator(const physical_operator& op);
    void add_operator(const string& line);

    string get_manifest();
    void load_manifest(const string& manifest);

    void get_operators_for_host(const int rank, vector<physical_operator>& matches);

};

#endif // physical_plan_h

