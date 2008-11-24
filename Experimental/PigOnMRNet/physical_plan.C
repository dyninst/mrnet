// Copyright Paradyn, 2008: contact jolly@cs.wisc.edu for details

#include "physical_plan.h"

physical_plan::physical_plan()
{
}

physical_plan::~physical_plan()
{
}

void physical_plan::add_operator(const physical_operator& op)
{
    operators.push_back(op);
}

string physical_plan::get_manifest()
{
    string plan;
    uint i, j;
    for(i = 0; i < operators.size(); ++i)
    {
        plan += to_string(operators[i].host_node_id);
        plan += " ";
        plan += to_string(operators[i].operator_id);
        plan += " ";
        plan += to_string((int)operators[i].op_type);
        plan += " ";
        for(j = 0; j < operators[i].inputs.size(); ++j)
        {
            plan += to_string(operators[i].inputs[i]);
            plan += " ";
        }
        plan += " , ";
        for(j = 0; j < operators[i].outputs.size(); ++j)
        {
            plan += to_string(operators[i].outputs[i]);
            plan += " ";
        }
        plan += "\n";
    }

    return plan;
}

