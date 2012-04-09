/****************************************************************************
 *  Copyright 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef os_windows
#include <unistd.h>
#endif

#include "xplat_lightweight/map.h"
#include "xplat_lightweight/xplat_utils_lightweight.h"

int hash_key(int key)
{
    // MJB TODO: need 1:1 hash function
    return key;
}

mrn_map_t* new_map_t()
{
    mrn_map_t* new_map = (mrn_map_t*) calloc( (size_t)1, sizeof(mrn_map_t) );
    assert(new_map);

    new_map->root = NULL;
    new_map->size = 0;
    new_map->alloc_size = 8;
    new_map->keys = (int*) calloc( new_map->alloc_size, sizeof(int) );
    assert( new_map->keys != NULL );
    
/*     xplat_dbg(5, xplat_printf(FLF, stderr,  */
/*                           "new_map_t() = %p, map->keys = %p\n", new_map, new_map->keys)); */
    return new_map;
}

void delete_map_nodes(map_node_t* root)
{
    if( root == NULL ) return;

    delete_map_nodes(root->left);
    delete_map_nodes(root->right);
    // we don't free map node values, just nodes
    free(root);
    root = NULL;
}

void delete_map_t(mrn_map_t* map)
{
    if( map == NULL ) return;

/*     xplat_dbg(5, xplat_printf(FLF, stderr,  */
/*                           "delete_map_t() = %p, map->root = %p, map->keys = %p\n", map, map->root, map->keys)); */

    delete_map_nodes(map->root);

    if( map->keys != NULL )
        free( map->keys );

    free(map);
}

void clear_map_t(mrn_map_t** map)
{
    if( *map != NULL )
        delete_map_t( *map );

    *map = new_map_t();
}

map_node_t* insert_recursive(map_node_t* root, int key, int hashed_key, void* val)
{
    map_node_t* new_node;

    xplat_dbg_func_begin();

    if (root == NULL) { // empty subtree, we've found place for new node
 
        new_node = (map_node_t*) malloc( sizeof(map_node_t) );
        assert( new_node != NULL );

        new_node->key = key;
        new_node->hashed_key = hashed_key;
        new_node->val = val;
        new_node->left = NULL;
        new_node->right = NULL;

        xplat_dbg(5, xplat_printf(FLF, stderr, "created new node\n"));
        return new_node;
    }
    else { // check if we need to insert into left subtree or right subtree

        if (hashed_key == root->hashed_key) {
            xplat_dbg(1, xplat_printf(FLF, stderr, 
                                  "Cannot have multiple map entries with same key %d\n", key));
            return NULL;
        }
        else if (hashed_key < root->hashed_key) {
            root->left = insert_recursive(root->left, key, hashed_key, val);
        }
        else {
            root->right = insert_recursive(root->right, key, hashed_key, val);
        }
    }

    xplat_dbg_func_end();

    return root;
}

void insert(mrn_map_t* map, int key, void* val)
{
    // insert new (key, val) pair into map
    int hashed_key = hash_key( key );
    map->root = insert_recursive(map->root, key, hashed_key, val);

    // update map size
    map->size++;

    if( map->size == map->alloc_size ) {
        // grow by doubling allocation as necessary
        map->alloc_size += map->alloc_size;
        map->keys = (int*) realloc( (void*)(map->keys), sizeof(int) * map->alloc_size );
        assert( map->keys != NULL );
    }

    map->keys[ map->size - 1 ] = key;

    print(map);
}

map_node_t* find_node_recursive(map_node_t* root, int hashed_key, map_node_t** parent)
{
    xplat_dbg_func_begin();

    if (root == NULL)
        return NULL;

    if (root->hashed_key == hashed_key)
        return root;

    if( parent != NULL ) 
        *parent = root;

    if (hashed_key < root->hashed_key) {
        return find_node_recursive(root->left, hashed_key, parent);
    }
    else {
        return find_node_recursive(root->right, hashed_key, parent);
    }
}

void* get_val(mrn_map_t* map, int key)
{
    int hashed_key = hash_key( key );
    map_node_t* target = find_node_recursive( map->root, hashed_key, NULL );
    if( target == NULL )
        return NULL;
    else {
        assert( target->key == key );
        return target->val;
    }
}

mrn_map_t* erase(mrn_map_t* map, int key)
{
    unsigned int i;
    int hashed_key = hash_key( key );
    map_node_t* parent = NULL;
    map_node_t* target = NULL;
    map_node_t* tmp = NULL;
    char found = 0;

    xplat_dbg(5, xplat_printf(FLF, stderr, "map %p erase(key=%d)\n", map, key));

#ifdef OLD_ERASE_BY_NEW_MAP_INSERT
    mrn_map_t* new_map = new_map_t();

    for( i = 0; i < map->size; i++ ) {
        if (map->keys[i] != key) {
            insert(new_map, map->keys[i], get_val(map, map->keys[i]));
        }
    }

    // free the old map
    delete_map_t(map);

    print(new_map);
    return new_map;
#else

    /* xplat_dbg(5, xplat_printf(FLF, stderr, "erasing map node with key %d, map->size=%"PRIsszt"\n", key, map->size)); */

    // remove from keys
    for( i = 0; i < map->size; i++ ) {
        if( map->keys[i] == key ) {
            found = 1;
            if( i != (map->size - 1) ) {
                // shift rest of keys by one
                memmove( (void*)(map->keys + i), (void*)(map->keys + (i+1)),
                         sizeof(int) * (map->size - (i+1)));
            }
        }
    }
    
    if( found == 0 ) { // not found, we're done
        xplat_dbg(3, xplat_printf(FLF, stderr, "map node with key %d not found\n", key));
        return map;
    }

    

    map->size--;

    // kill the node
    target = find_node_recursive( map->root, hashed_key, &parent );
    assert( target != NULL );
        
    // Case 1: leaf node
    if( (target->left == NULL) && (target->right == NULL) ) {
        if( parent == NULL ) { // target is root
            map->root = NULL;
        }
        else if( parent->left == target ) {
            parent->left = NULL;
        }
        else {
            parent->right = NULL;
        }
        free( target );
    }
    else {
        // Case 2: has left and right subtrees, 
        //         promote rightmost descendant of left subtree
        if( (target->left != NULL) && (target->right != NULL) ) {
            tmp = target->left;
            parent = target;
            while( tmp->right != NULL ) {
                parent = tmp;
                tmp = tmp->right;
            }
            /* xplat_dbg(5, xplat_printf(FLF, stderr, "replacing erased map node with rightmost descendant of left subtree whose key=%d\n", tmp->key)); */
            
            if( parent->left == tmp ) {
                parent->left = tmp->left;
                /* xplat_dbg(5, xplat_printf(FLF, stderr, "moving tmp->left to parent->left\n")); */
            }
            else {
                parent->right = tmp->left;
                /* xplat_dbg(5, xplat_printf(FLF, stderr, "moving tmp->left to parent->right\n")); */
            }            
        }   
        // Case 3: has left subtree, promote it
        else if( target->left != NULL ) {
            tmp = target->left;
            target->left = tmp->left;
            target->right = tmp->right;
            /* xplat_dbg(5, xplat_printf(FLF, stderr, "replacing erased map node with left subtree\n")); */
        }
        // Case 4: has right subtree, promote it
        else if( target->right != NULL ) {
            tmp = target->right;
            target->left = tmp->left;
            target->right = tmp->right;
            /* xplat_dbg(5, xplat_printf(FLF, stderr, "replacing erased map node with right subtree\n")); */
        }
        // move tmp data to target, free tmp
        xplat_dbg(5, xplat_printf(FLF, stderr, "replacing erased map node with key %d with map node with key %d\n", target->key, tmp->key));
        target->key = tmp->key;
        target->hashed_key = tmp->hashed_key;
        target->val = tmp->val;
        free(tmp);
    }

    /* xplat_dbg(5, xplat_printf(FLF, stderr, "after erasing map node with key %d, map->size=%"PRIsszt"\n", key, map->size)); */
        
    print(map);
    return map;
#endif
}

void print_recursive(map_node_t* node) 
{
    if( node != NULL ) {
        print_recursive(node->left);
        xplat_dbg(5, xplat_printf(FLF, stderr, "\t%d\n", node->key));
        print_recursive(node->right);
    }
}

void print(mrn_map_t* map)
{
    xplat_dbg(5, xplat_printf(FLF, stderr, "printing map:\n"));
    print_recursive(map->root);
}   

