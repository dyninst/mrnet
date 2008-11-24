// Copyright Paradyn, 2008: contact jolly@cs.wisc.edu for details

#include "input_reader.h"

vector<tuple> read_input(
    string filename,
    int num_fields
    )
{
    ifstream infile;
    string field;
    tuple new_tuple;
    vector<tuple> results;
    
    infile.open(filename.c_str());
    if(!infile.is_open())
    {
      return results;
    }
    while(1)
    {
        for(int i = 0; i < num_fields; ++i)
        {
            infile >> field;
            new_tuple.push_back(field);
        }
        if(infile.eof())
        {
            break;
        }
        results.push_back(new_tuple);
        new_tuple.clear();
    }

    infile.close();
    return results;
}



