
#include <vector>
using std::vector;

#include <map>
using std::map;

#include <set>
using std::set;

#include <iostream>
using std::ios;
using std::cerr;

#include <fstream>
using std::ofstream;
using std::ifstream;

#include "types_visitor.h"

// unit test:
// [1] create a complex pig data structure that uses all the complex and simple types
// [2] serialize it into a blob
// [3] deserialize the blob back into a data structure
// [4] do the contents of the new structure match the original?
//
// after execution, before.txt and after.txt should match the following:
// {( 311  666 [  1 # 13    2 # 42  ])( 100  200 )}
int main()
{
    simple_item* key_1 = create_simple_item((int)1);
    simple_item* key_2 = create_simple_item((int)2);
    complex_item* value_1 = create_complex_item((long)13);
    complex_item* value_2 = create_complex_item((long)42);
    map< simple_item*, complex_item* > m;
    m.insert(make_pair(key_1, value_1));
    m.insert(make_pair(key_2, value_2));
    complex_item* map_1 = create_complex_item(&m);

    complex_item* value_3 = create_complex_item((float)311.5);
    complex_item* value_4 = create_complex_item((double)666.3);
    vector< complex_item* > t1;
    t1.push_back(value_3);
    t1.push_back(value_4);
    t1.push_back(map_1);
    complex_item* tuple_1 = create_complex_item(&t1);

    complex_item* value_5 = create_complex_item((char*)"foo");
    complex_item* value_6 = create_complex_item((char*)"bar");
    vector< complex_item* > t2;
    t2.push_back(value_5);
    t2.push_back(value_6);
    complex_item* tuple_2 = create_complex_item(&t2);

    set< complex_item* > b;
    b.insert(tuple_1);
    b.insert(tuple_2);
    complex_item* bag_1 = create_complex_item(&b);
    
    ofstream text_out("before.txt");
    types_visitor before_writer(&text_out);
    before_writer.print_complex_item(bag_1);
    text_out.close();

    ofstream bin_out("tuple.dat", ios::binary);
    types_visitor writer(&bin_out);
    writer.serialize_complex_item(bag_1);
    bin_out.close();

    ifstream bin_in("tuple.dat", ios::binary);
    types_visitor reader(&bin_in);
    complex_item* stored = reader.deserialize_complex_item();
    bin_in.close();

    text_out.open("after.txt");
    types_visitor after_writer(&text_out);
    after_writer.print_complex_item(stored);
    text_out.close();

    return 0;
}

