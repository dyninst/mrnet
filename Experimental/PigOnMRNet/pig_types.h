
#include <algorithm>
using std::for_each;

#include <iostream>
using std::cerr;

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <set>
using std::set;

#include <map>
using std::map;
using std::pair;
using std::make_pair;

#define SIMPLE_INTEGER     89
#define SIMPLE_LONG       178
#define SIMPLE_FLOAT      267
#define SIMPLE_DOUBLE     356
#define SIMPLE_STRING     445

#define COMPLEX_SIMPLE     97
#define COMPLEX_TUPLE     194
#define COMPLEX_BAG       291
#define COMPLEX_MAP       388

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

void print_simple_item(simple_item* item)
{
    cerr << " ";
    switch(item->type)
    {
    case SIMPLE_INTEGER: cerr << item->data._int; break;
    case SIMPLE_LONG:    cerr << item->data._long; break; 
    case SIMPLE_FLOAT:   cerr << item->data._float; break;
    case SIMPLE_DOUBLE:  cerr << item->data._double; break;
    case SIMPLE_STRING:  cerr << item->data._string; break;
    }
    cerr << " ";
}

void print_tuple(vector< complex_item* >* tuple);

void print_bag(set< complex_item* >* bag);

void print_map(map< simple_item*, complex_item* >* map);

void print_complex_item(complex_item* item)
{
    switch(item->type)
    {
    case COMPLEX_SIMPLE:   print_simple_item(item->data._simple); break;
    case COMPLEX_TUPLE:    print_tuple(item->data._tuple); break;
    case COMPLEX_BAG:      print_bag(item->data._bag); break;
    case COMPLEX_MAP:      print_map(item->data._map); break;
    }
}

void print_tuple(vector< complex_item* >* tuple)
{
    cerr << "(";
    for_each(tuple->begin(), tuple->end(), print_complex_item);
    cerr << ")";
}

void print_bag(set< complex_item* >* bag)
{
    cerr << "{";
    for_each(bag->begin(), bag->end(), print_complex_item);
    cerr << "}";
}

void print_map(map< simple_item*, complex_item* >* map)
{
    std::map< simple_item*, complex_item* >::iterator at;
    cerr << "[";
    for(at = map->begin(); at != map->end(); ++at) 
    {
        cerr << " ";
        print_simple_item(at->first);
        cerr << "#";
        print_complex_item(at->second);
        cerr << " ";
    }
    cerr << "]";
}

