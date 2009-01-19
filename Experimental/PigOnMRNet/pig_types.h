
#if !defined( pig_types_h )
#define pig_types_h 1

#include <vector>
using std::vector;

#include <set>
using std::set;

#include <map>
using std::map;

const uint SIMPLE_INTEGER( 89 );
const uint SIMPLE_LONG( 178 );
const uint SIMPLE_FLOAT( 267 );
const uint SIMPLE_DOUBLE( 356 );
const uint SIMPLE_STRING( 445 );

const uint COMPLEX_SIMPLE(  97 );
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
    short type;
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
    short type;
    complex data;
};

#endif // pig_types_h

