/****************************************************************************
 *  Copyright 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef os_windows
#include <unistd.h>
#endif //ifndef os_windows
#include <assert.h>

#include "vector.h"
#include "utils_lightweight.h"

vector_t* new_empty_vector_t()
{
    vector_t* new_vector = (vector_t*)malloc(sizeof(vector_t));
    assert(new_vector);
    new_vector->alloc_size = 64;
    new_vector->vec = (void**) malloc( sizeof(void*) * new_vector->alloc_size );
    assert(new_vector->vec);
    new_vector->size = 0;
    
    return new_vector;
}

void clear(vector_t* vector)
{
    /* because elements stored are pointers, they might be in use
     * elsewhere, so don't free */

    /* no need to shrink the allocation, just clear the contents */
    memset( (void*)(vector->vec), 0, (vector->alloc_size * sizeof(void*)) );
    vector->size = 0;
}

void copy_vector(vector_t* fromvec, vector_t* tovec)
{
    /* grow tovec to number of elements in fromvec, if necessary */
    if( fromvec->size > tovec->alloc_size ) {
        tovec->vec = (void**) realloc( tovec->vec, sizeof(void*) * fromvec->size );
        tovec->alloc_size = fromvec->size;
    }
    memcpy( tovec->vec, fromvec->vec, sizeof(void*) * fromvec->size );
    tovec->size = fromvec->size;
}

void pushBackElement(vector_t* vector, void* elem)
{
    /* grow by doubling allocation as necessary */
    if( (vector->size + 1) == vector->alloc_size ) {
        vector->alloc_size += vector->alloc_size;
        vector->vec = (void**) realloc( vector->vec, sizeof(void*) * vector->alloc_size );
        assert(vector->vec);
    }
    vector->vec[ vector->size ] = elem;
    vector->size++;
}


void* popBackElement(vector_t* vector)
{
    void* elem = vector->vec[ vector->size - 1 ];

    /* no need to shrink the allocation, just decrement size */
    vector->size--;

    return elem;
}

void* getBackElement(vector_t* vector )
{
    return vector->vec[ vector->size - 1 ];
}

void delete_vector_t(vector_t* vector)
{
    /* because elements stored are pointers, they might be in use 
       elsewhere, so don't free */
    free(vector->vec);
    free(vector);
}

int findElement(vector_t* vector, void* elem)
{
    int i;
    for( i = 0; i < vector->size; i++ ) {
        if( vector->vec[i] == elem )
            return i+1;
    }
    return false; 
}


vector_t* eraseElement(vector_t* vector, void* elem)
{
    int i;
    for( i = 0; i < vector->size; i++ ) {
        if( vector->vec[i] == elem ) {
            /* shift the elements after this one */
            while( (i+1) < vector->size ) {
                vector[i] = vector[i+1];
                i++;
            }
            vector->size--;
        }
    }
    return vector;
}

