/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined __map_h
#define __map_h 1

struct map_node_t {
  int key;
  void* val;
  struct map_node_t* left;
  struct map_node_t* right;
} ;

typedef struct map_node_t map_node_t;

typedef struct {
   map_node_t* root;
   int size;
   int* keys;
} map_t;

 map_t* new_map_t();

void insert( map_t* map, int key, void* val);

void* get_val( map_t* map, int key);

void delete_map_t(map_t* map);

void clear_map_t(map_t* map);

map_t* erase(map_t* map, int ikey);

void print(map_t* map);

#endif 
