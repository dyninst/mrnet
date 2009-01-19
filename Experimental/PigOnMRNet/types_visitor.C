
#include "types_visitor.h"

types_visitor::types_visitor(ostream* stream)
{
    os = stream;
}

types_visitor::~types_visitor()
{
    
}

void types_visitor::print_simple_item(simple_item* item)
{
    *os << " ";
    switch(item->type)
    {
    case SIMPLE_INTEGER: *os << item->data._int; break;
    case SIMPLE_LONG:    *os << item->data._long; break; 
    case SIMPLE_FLOAT:   *os << item->data._float; break;
    case SIMPLE_DOUBLE:  *os << item->data._double; break;
    case SIMPLE_STRING:  *os << item->data._string; break;
    }
    *os << " ";
}

void types_visitor::print_complex_item(complex_item* item)
{
    switch(item->type)
    {
    case COMPLEX_SIMPLE: print_simple_item(item->data._simple); break;
    case COMPLEX_TUPLE:  print_tuple(item->data._tuple); break;
    case COMPLEX_BAG:    print_bag(item->data._bag); break;
    case COMPLEX_MAP:    print_map(item->data._map); break;
    }
}

void types_visitor::print_tuple(vector< complex_item* >* tuple)
{
    *os << "(";
    std::vector< complex_item* >::iterator at;
    for(at = tuple->begin(); at != tuple->end(); ++at) 
    {
        print_complex_item(*at);
    }
    *os << ")";
}

void types_visitor::print_bag(set< complex_item* >* bag)
{
    *os << "{";
    std::set< complex_item* >::iterator at;
    for(at = bag->begin(); at != bag->end(); ++at) 
    {
        print_complex_item(*at);
    }
    *os << "}";
}

void types_visitor::print_map(map< simple_item*, complex_item* >* map)
{
    *os << "[";
    std::map< simple_item*, complex_item* >::iterator at;
    for(at = map->begin(); at != map->end(); ++at) 
    {
        *os << " ";
        print_simple_item(at->first);
        *os << "#";
        print_complex_item(at->second);
        *os << " ";
    }
    *os << "]";
}

void types_visitor::serialize_simple_item(simple_item* item)
{
    switch(item->type)
    {
    case SIMPLE_INTEGER: os->write((char*)&SIMPLE_INTEGER, sizeof(uint));
                         os->write((char*)&item->data._int, sizeof(int));
                         break;
    case SIMPLE_LONG:    os->write((char*)&SIMPLE_LONG, sizeof(uint));
                         os->write((char*)&item->data._long, sizeof(long));
                         break;
    case SIMPLE_FLOAT:   os->write((char*)&SIMPLE_FLOAT, sizeof(uint));
                         os->write((char*)&item->data._long, sizeof(float));
                         break;
    case SIMPLE_DOUBLE:  os->write((char*)&SIMPLE_DOUBLE, sizeof(uint));
                         os->write((char*)&item->data._long, sizeof(double));
                         break;
    case SIMPLE_STRING:  os->write((char*)&SIMPLE_STRING, sizeof(uint));
                         os->write(item->data._string, strlen(item->data._string) + 1);
                         break;
    default:             break;
    }
}

void types_visitor::serialize_complex_item(complex_item* item)
{
    switch(item->type)
    {
    case COMPLEX_SIMPLE: serialize_simple_item(item->data._simple); break;
    case COMPLEX_TUPLE:  serialize_tuple(item->data._tuple); break;
    case COMPLEX_BAG:    serialize_bag(item->data._bag); break;
    case COMPLEX_MAP:    serialize_map(item->data._map); break;
    }
}

void types_visitor::serialize_tuple(vector< complex_item* >* tuple)
{
    os->write((char*)&COMPLEX_TUPLE, sizeof(uint));
    size_t tuple_size = tuple->size();
    os->write((char*)&tuple_size, sizeof(size_t));
    vector< complex_item* >::iterator at;
    for(at = tuple->begin(); at != tuple->end(); ++at)
    {
        serialize_complex_item(*at);
    }
}

void types_visitor::serialize_bag(set< complex_item* >* bag)
{
    os->write((char*)&COMPLEX_BAG, sizeof(uint));
    size_t bag_size = bag->size();
    os->write((char*)&bag_size, sizeof(size_t));
    set< complex_item* >::iterator at;
    for(at = bag->begin(); at != bag->end(); ++at)
    {
        serialize_complex_item(*at);      
    }
}

void types_visitor::serialize_map(map< simple_item*, complex_item* >* map)
{
    os->write((char*)&COMPLEX_MAP, sizeof(uint));
    size_t map_size = map->size();
    os->write((char*)&map_size, sizeof(size_t));
    std::map< simple_item*, complex_item* >::iterator at;
    for(at = map->begin(); at != map->end(); ++at) 
    {
        serialize_simple_item(at->first);
        serialize_complex_item(at->second);
    }
}

