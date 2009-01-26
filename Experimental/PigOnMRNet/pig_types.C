
#include "pig_types.h"

bool is_complex(uint type)
{
    return (0 == type % COMPLEX_SIMPLE);
}

simple_item* create_simple_item(uint type)
{
    simple_item* new_si = new simple_item;
    new_si->type = type;
    return new_si;
}

complex_item* create_complex_item(uint type)
{
    complex_item* new_ci = new complex_item;
    new_ci->type = type;
    switch(type)
    {
    case COMPLEX_SIMPLE: break;
    case COMPLEX_TUPLE:  vector< complex_item* >* new_tuple;
                         new_tuple = new vector< complex_item* >();
                         break;
    case COMPLEX_BAG:    set< complex_item* >* new_set;
                         new_set = new set< complex_item* >();
                         break;
    case COMPLEX_MAP:    map< simple_item*, complex_item* >* new_map;
                         new_map = new map< simple_item*, complex_item* >();
                         break;
    }
    return new_ci;
}

