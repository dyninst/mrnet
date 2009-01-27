
#include "types_visitor.h"

types_visitor::types_visitor(istream* stream)
{
    is = stream;
}

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
    size_t str_len;
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
                         str_len = strlen(item->data._string) + 1;
                         os->write((char*)&str_len, sizeof(size_t));
                         os->write(item->data._string, str_len);
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

simple_item* types_visitor::deserialize_simple_item(uint type)
{
    simple_item* si = create_simple_item(type);
    switch(type)
    {
    case SIMPLE_INTEGER: int i_value;
                         is->read((char*)&i_value, sizeof(int));
                         si->data._int = i_value;
                         break;
    case SIMPLE_LONG:    long l_value;
                         is->read((char*)&l_value, sizeof(long));
                         si->data._long = l_value;
                         break; 
    case SIMPLE_FLOAT:   float f_value;
                         is->read((char*)&f_value, sizeof(float));
                         si->data._float = f_value;
                         break;
    case SIMPLE_DOUBLE:  double d_value;
                         is->read((char*)&d_value, sizeof(double));
                         si->data._double = d_value;
                         break;
    case SIMPLE_STRING:  size_t s_size;
                         is->read((char*)&s_size, sizeof(size_t));
                         char* s_value = new char[s_size];
                         is->read(s_value, s_size);
                         si->data._string = s_value;
                         break;
    }
    return si;
}

complex_item* types_visitor::deserialize_complex_item()
{    
    uint type;
    is->read((char*)&type, sizeof(uint));
    complex_item* ci = create_complex_item(type);
    simple_item* new_si;

    if(is_simple(type))
    {
        new_si = deserialize_simple_item(type);
        ci->type = COMPLEX_SIMPLE;
        ci->data._simple = new_si;
        return ci;
    }

    vector< complex_item* >* new_tuple;
    set< complex_item* >* new_bag;
    map< simple_item*, complex_item* >* new_map;
    
    (void)new_tuple;
    (void)new_bag;
    (void)new_map;

    switch(type)
    {
    case COMPLEX_TUPLE:  new_tuple = deserialize_tuple();
                         ci->type = COMPLEX_TUPLE;
                         ci->data._tuple = new_tuple;
                         break;
    case COMPLEX_BAG:    new_bag = deserialize_bag();
                         ci->type = COMPLEX_BAG;
                         ci->data._bag = new_bag;
                         break;
    case COMPLEX_MAP:    new_map = deserialize_map();
                         ci->type = COMPLEX_MAP;
                         ci->data._map = new_map;
                         break;
    default:             cerr << "encountered unknown type";
                         break; // TODO: throw something here
    }
    return ci;
}

vector< complex_item* >*  types_visitor::deserialize_tuple()
{
    vector< complex_item* >* new_tuple = new vector< complex_item* >();
    complex_item* new_item;
    size_t tuple_size;
    is->read((char*)&tuple_size, sizeof(size_t));
    for(uint i = 0; i < tuple_size; ++i)
    {
        new_item = deserialize_complex_item();
        new_tuple->push_back(new_item);
    }
    return new_tuple;
}

set< complex_item* >* types_visitor::deserialize_bag()
{
    set< complex_item* >* new_bag = new set< complex_item* >();
    complex_item* new_item;
    size_t bag_size;
    is->read((char*)&bag_size, sizeof(size_t));
    for(uint i = 0; i < bag_size; ++i)
    {
        new_item = deserialize_complex_item();
        new_bag->insert(new_item);
    }
    return new_bag;
}

map< simple_item*, complex_item* >* types_visitor::deserialize_map()
{
    map< simple_item*, complex_item* >* new_map = new map< simple_item*, complex_item* >();
    complex_item* new_ci;
    simple_item* new_si;
    size_t map_size;
    is->read((char*)&map_size, sizeof(size_t));
    for(uint i = 0; i < map_size; ++i)
    {
        uint type;
        is->read((char*)&type, sizeof(uint));
        new_si = deserialize_simple_item(type);
        new_ci = deserialize_complex_item();       
        new_map->insert(make_pair(new_si, new_ci));
    }
    return new_map;
}

