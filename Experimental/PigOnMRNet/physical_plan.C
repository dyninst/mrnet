// Copyright Paradyn, 2008: contact jolly@cs.wisc.edu for details

#include "physical_plan.h"

physical_plan::physical_plan()
{
}

physical_plan::~physical_plan()
{
}

void physical_plan::add_operator(
    const physical_operator& op
    )
{
    operators.push_back(op);
}

// The manifest is a std::string that contains one line for each operator.
// Each line starts with an operator id X and ends with a delimiter value -1.
// The remaining integers on the line are the ids of operators that supply X with either input or output.
// The input and output operators are also seperated by the delimiter value -1, with the input operators listed first.
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
        plan += " -1 ";
        for(j = 0; j < operators[i].outputs.size(); ++j)
        {
            plan += to_string(operators[i].outputs[i]);
            plan += " ";
        }
        plan += " -1 ";
    }
    return plan;
}

void physical_plan::load_manifest(
    const string& manifest
    )
{
    string line;
    int value;
    istringstream value_reader(manifest);
    while(1)
    {
        physical_operator new_op;
        value_reader >> new_op.operator_id;
        if(value_reader.eof())
        {
            break;
        }
        value_reader >> new_op.host_node_id;
        value_reader >> value;
        new_op.op_type = (physical_op_type)value;
        value_reader >> value;
        while(-1 != value)
        {
            new_op.inputs.push_back(value);
            value_reader >> value;
        }
        value_reader >> value;
        while(-1 != value)
        {
            new_op.outputs.push_back(value);
            value_reader >> value;
        }
        operators.push_back(new_op);
    }
}

void physical_plan::get_operators_for_host(
    const int rank,
    vector<physical_operator>& matches
    )
{
    uint j;
    for(j = 0; j < operators.size(); ++j)
    {
        if(operators[j].host_node_id == rank)
        {
            matches.push_back(operators[j]);
        }
    }
}

