// Copyright Paradyn, 2008: contact jolly@cs.wisc.edu for details

#if !defined( input_reader_h )
#define input_reader_h 1

#include "boar.h"

#include <iostream>
using std::cerr;

#include <fstream>
using std::ifstream;
#include <string>
using std::string;
#include <vector>
using std::vector;

vector< vector<string> > read_input(
    string filename,
    int num_fields
    );

#endif

