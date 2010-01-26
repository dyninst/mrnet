/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#ifndef os_windows
#include <unistd.h>
#endif

#include "map.h"
#include "utils.h"

map_t* new_map_t()
{
    int keys[1];
    map_t* new_map = (map_t*)malloc(sizeof(map_t));
    assert(new_map);

    new_map->root = NULL;
    new_map->size = 0;
    new_map->keys = (int*)malloc(sizeof(keys));
    assert(new_map->keys);

    return new_map;
}

map_node_t* delete_map_recursive(map_node_t* root) {
    if (root) {
        delete_map_recursive(root->left);
        delete_map_recursive(root->right);
        /* because elements are stored as pointers, might
         * be in use elsewhere */
        //free(root);
        root = NULL;
    }

    return root;
}

void delete_map_t(map_t* map) {
    map->root = delete_map_recursive(map->root);

    free(map);
}

void clear_map_t(map_t* map) {
    delete_map_recursive(map->root);
    map = new_map_t();
}

map_node_t* insert_recursive(map_node_t* root, int key, void* val)
{
    map_node_t* new_node;

    // map is currently empty
    if (root == NULL) {
        new_node = (map_node_t*)malloc(sizeof(map_node_t));
        assert(new_node);

        new_node->key = key;
        new_node->val = (void*)malloc(sizeof(val));
        assert(new_node->val);
        new_node->val = val;
        new_node->left = NULL;
        new_node->right = NULL;

        return new_node;
    }
  
    // check if we need to insert into left subtree or right subtree
    else {
        if (key < root->key) {
            root->left = insert_recursive(root->left, key, val);
        }
        else if (key == root->key) {
            perror("Cannot have multiple map entries with same key");
            return NULL;
        }
        else {
            root->right = insert_recursive(root->right, key, val);
        }
    }

    return root;
}

void insert(map_t* map, int key, void* val)
{
    int i;
    int new_val = 1;
    int keys[1];

    for (i = 0; i < map->size; i++) {
        if (map->keys[i] == key) 
            new_val = 0;
    }
    if (new_val) {
        // insert new (key, val) pair into map
        map->root = insert_recursive(map->root, key, val);
        // update map size
        map->size++;
        // insert key into key array
        map->keys = (int*)realloc(map->keys, sizeof(keys)*(map->size));
        assert(map->keys);
        map->keys[map->size-1] = key;
    }

    //print(map);

}

void* get_val_recursive(map_node_t* root, int key)
{
	if (root == NULL)
        return NULL;
	else if (root->key == key) {
        return root->val;
    }
    else if (key < root->key)
        return get_val_recursive(root->left, key);
    else
        return get_val_recursive(root->right, key);

}

void* get_val(map_t* map, int key)
{
    return get_val_recursive(map->root, key);
}

map_t* erase(map_t* map, int ikey)
{   
    map_t* new_map = new_map_t();

    int i;
    for (i = 0; i < map->size; i++) {
        if (map->keys[i] != ikey) {
            insert(new_map, map->keys[i], get_val(map, map->keys[i]));
        }
    }

    return new_map;
}

void print_recursive(map_node_t* node) 
{
    if (node) {
        print_recursive(node->left);
        mrn_dbg(5, mrn_printf(0,0,0, stderr, "%d\n", node->key));
        print_recursive(node->right);
    }
}

void print(map_t* map)
{
    print_recursive(map->root);
}   

