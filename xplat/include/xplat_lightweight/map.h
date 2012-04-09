/****************************************************************************
 *  Copyright 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined __map_h
#define __map_h 1

struct map_node_t {
    struct map_node_t* left;
    struct map_node_t* right;
    int key;
    int hashed_key;
    void* val;
};
typedef struct map_node_t map_node_t;

struct mrn_map_t {
    map_node_t* root;
    int* keys;
    size_t size;
    size_t alloc_size;
};
typedef struct mrn_map_t mrn_map_t;

mrn_map_t* new_map_t();

void insert( mrn_map_t* map, int key, void* val);

void* get_val( mrn_map_t* map, int key);

void delete_map_t(mrn_map_t* map);

void clear_map_t(mrn_map_t** map);

mrn_map_t* erase(mrn_map_t* map, int ikey);

void print(mrn_map_t* map);

#endif 
