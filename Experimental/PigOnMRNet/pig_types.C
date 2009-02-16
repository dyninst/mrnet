
#include "pig_types.h"

bool is_complex(uint type)
{
    return (0 == type % COMPLEX_SIMPLE);
}

bool is_simple(uint type)
{
    return (0 == type % SIMPLE_INTEGER);
}

simple_item* create_simple_item(uint type)
{
    simple_item* new_si = new simple_item;
    new_si->type = type;
    return new_si;
}

simple_item* create_simple_item(int data)
{
    simple_item* new_si = new simple_item;
    new_si->type = SIMPLE_INTEGER;
    new_si->data._int = data;
    return new_si;
}

simple_item* create_simple_item(long data)
{
    simple_item* new_si = new simple_item;
    new_si->type = SIMPLE_LONG;
    new_si->data._long = data;
    return new_si;
}

simple_item* create_simple_item(float data)
{
    simple_item* new_si = new simple_item;
    new_si->type = SIMPLE_FLOAT;
    new_si->data._float = data;
    return new_si;
}

simple_item* create_simple_item(double data)
{
    simple_item* new_si = new simple_item;
    new_si->type = SIMPLE_DOUBLE;
    new_si->data._double = data;
    return new_si;
}

simple_item* create_simple_item(char* data)
{
    simple_item* new_si = new simple_item;
    new_si->type = SIMPLE_STRING;
    new_si->data._string = data;
    return new_si;
}

complex_item* create_complex_item(uint type)
{
    complex_item* new_ci = new complex_item;
    new_ci->type = type;
    return new_ci;
}

complex_item* create_complex_item(int data)
{
    complex_item* new_ci = new complex_item;
    new_ci->type = COMPLEX_SIMPLE;
    new_ci->data._simple = create_simple_item(data);
    return new_ci;
}

complex_item* create_complex_item(long data)
{
    complex_item* new_ci = new complex_item;
    new_ci->type = COMPLEX_SIMPLE;
    new_ci->data._simple = create_simple_item(data);
    return new_ci;
}

complex_item* create_complex_item(float data)
{
    complex_item* new_ci = new complex_item;
    new_ci->type = COMPLEX_SIMPLE;
    new_ci->data._simple = create_simple_item(data);
    return new_ci;
}

complex_item* create_complex_item(double data)
{
    complex_item* new_ci = new complex_item;
    new_ci->type = COMPLEX_SIMPLE;
    new_ci->data._simple = create_simple_item(data);
    return new_ci;
}

complex_item* create_complex_item(char* data)
{
    complex_item* new_ci = new complex_item;
    new_ci->type = COMPLEX_SIMPLE;
    new_ci->data._simple = create_simple_item(data);
    return new_ci;
}

complex_item* create_complex_item(vector< complex_item* >* data)
{
    complex_item* new_ci = new complex_item;
    new_ci->type = COMPLEX_TUPLE;
    new_ci->data._tuple = data;
    return new_ci;
}

complex_item* create_complex_item(set< complex_item* >* data)
{
    complex_item* new_ci = new complex_item;
    new_ci->type = COMPLEX_BAG;
    new_ci->data._bag = data;
    return new_ci;
}

complex_item* create_complex_item(map< simple_item*, complex_item* >* data)
{
    complex_item* new_ci = new complex_item;
    new_ci->type = COMPLEX_MAP;
    new_ci->data._map = data;
    return new_ci;
}

