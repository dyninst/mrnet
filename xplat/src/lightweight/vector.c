/****************************************************************************
 *  Copyright 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef os_windows
#include <unistd.h>
#endif //ifndef os_windows
#include <assert.h>

#include "xplat_lightweight/vector.h"
#include "xplat_lightweight/xplat_utils_lightweight.h"

vector_t* new_empty_vector_t()
{
    vector_t* new_vector = (vector_t*) calloc( 1, sizeof(vector_t) );
    assert( new_vector != NULL );

    new_vector->size = 0;
    new_vector->alloc_size = 64;
    new_vector->vec = (void**) calloc( new_vector->alloc_size, sizeof(void*) );
    assert( new_vector->vec != NULL );

/*     xplat_dbg(5, xplat_printf(FLF, stderr, */
/*                           "new_vector_t() = %p, vec->vec = %p\n", new_vector, new_vector->vec)); */

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
        assert( tovec->vec != NULL );
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
        assert( vector->vec != NULL );
    }
    vector->vec[ vector->size ] = elem;
    vector->size++;
}


void* getBackElement(vector_t* vector )
{
    if( vector->size )
        return vector->vec[ vector->size - 1 ];

    return NULL;
}

void* popBackElement(vector_t* vector)
{
    void* elem = getBackElement( vector );

    /* no need to shrink the allocation, just decrement size */
    vector->size--;

    return elem;
}

void delete_vector_t(vector_t* vector)
{
    /* because elements stored are pointers, they might be in use 
       elsewhere, so don't free */

/*     xplat_dbg(5, xplat_printf(FLF, stderr, */
/*                           "delete_vector_t() = %p, vec->vec = %p\n", vector, vector->vec)); */
    
    if( vector->vec != NULL )
        free(vector->vec);

    free(vector);
}

int findElement(vector_t* vector, void* elem)
{
    size_t i;
    for( i = 0; i < vector->size; i++ ) {
        if( vector->vec[i] == elem )
            return (int)(i+1);
    }
    return 0; 
}


vector_t* eraseElement(vector_t* vector, void* elem)
{
    size_t i;
    for( i = 0; i < vector->size; i++ ) {
        if( vector->vec[i] == elem ) {
            if( i != (vector->size - 1) ) {
                // shift rest of elements by one
                memmove( (void*)(vector->vec + i), (void*)(vector->vec + (i+1)),
                         sizeof(void*) * (vector->size - (i+1)));
            }

            vector->size--;
        }
    }
    return vector;
}

