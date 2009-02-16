
#if !defined( pig_types_h )
#define pig_types_h 1

#include <iostream>
using std::istream;

#include <vector>
using std::vector;

#include <set>
using std::set;

#include <map>
using std::map;
using std::pair;
using std::make_pair;

#include "boar.h"

const uint SIMPLE_INTEGER( 89 );
const uint SIMPLE_LONG( 178 );
const uint SIMPLE_FLOAT( 267 );
const uint SIMPLE_DOUBLE( 356 );
const uint SIMPLE_STRING( 445 );

const uint COMPLEX_SIMPLE( 97 );
const uint COMPLEX_TUPLE( 194 );
const uint COMPLEX_BAG( 291 );
const uint COMPLEX_MAP( 388 );

union simple
{
    int _int;
    long _long;
    float _float;
    double _double;
    char* _string;
};

struct simple_item
{
    uint type;
    simple data;
};

struct complex_item;

union complex
{
    simple_item* _simple;
    vector< complex_item* >* _tuple;    
    set< complex_item* >* _bag;
    map< simple_item*, complex_item* >* _map;
};

struct complex_item
{
    uint type;
    complex data;
};

bool is_simple(uint type);

bool is_complex(uint type);

simple_item* create_simple_item(uint type);

simple_item* create_simple_item(int data);

simple_item* create_simple_item(long data);

simple_item* create_simple_item(float data);

simple_item* create_simple_item(double data);

simple_item* create_simple_item(char* data);

complex_item* create_complex_item(uint type);

complex_item* create_complex_item(int data);

complex_item* create_complex_item(long data);

complex_item* create_complex_item(float data);

complex_item* create_complex_item(double data);

complex_item* create_complex_item(char* data);

complex_item* create_complex_item(vector< complex_item* >* data);

complex_item* create_complex_item(set< complex_item* >* data);

complex_item* create_complex_item(map< simple_item*, complex_item* >* data);

#endif // pig_types_h

