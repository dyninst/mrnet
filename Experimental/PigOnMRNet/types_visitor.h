// Copyright Paradyn, 2008: contact jolly@cs.wisc.edu for details

#if !defined( types_visitor_h )
#define types_visitor_h 1

#include <iostream>
using std::ostream;

#include <cstring>

#include "boar.h"

#include "pig_types.h"

class types_visitor
{
public:

    ostream* os;

    types_visitor(ostream* stream);

    ~types_visitor();

    void print_simple_item(simple_item* item);

    void print_complex_item(complex_item* item);

    void print_tuple(vector< complex_item* >* tuple);
    
    void print_bag(set< complex_item* >* bag);
    
    void print_map(map< simple_item*, complex_item* >* map);
    
    void serialize_simple_item(simple_item* item);
    
    void serialize_complex_item(complex_item* item);
    
    void serialize_tuple(vector< complex_item* >* tuple);
    
    void serialize_bag(set< complex_item* >* bag);
    
    void serialize_map(map< simple_item*, complex_item* >* map);

};

#endif // types_visitor_h
