/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
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
  void* vec[1];
  vector_t* new_vector = (vector_t*)malloc(sizeof(vector_t));
  assert(new_vector);
  new_vector->vec = (void**)malloc(sizeof(vec));
  assert(new_vector->vec);
  new_vector->size = 0;

  return new_vector;
}

void clear(vector_t* vector)
{
    /* because elements stored are pointers, they might be in use
     * elsewhere */
    //int i;
    //for ( i = 0; i < vector->size; i++) 
    //    free(vector->vec[i]);

    void* vec[1];
    vector->vec = (void**)realloc(vector->vec, sizeof(vec));
    assert(vector->vec);
    vector->size = 0;
}

void pushBackElement(vector_t* vector, void* elem)
{
	void* vec[1];

    vector->vec = (void**)realloc(vector->vec, sizeof(vec)*(vector->size+1));
    assert(vector->vec);

    vector->vec[vector->size] = (void*)malloc(sizeof(elem));
    assert(vector->vec[vector->size]);

    vector->vec[vector->size] = elem;

    vector->size++;
}


void* popBackElement( vector_t* vector)
{
    void* elem = vector->vec[vector->size-1];

	void* vec[1];

    vector->vec = (void**)realloc(vector->vec, sizeof(vec)*(vector->size-1));
    assert(vector->vec);

    vector->size--;

    return elem;
}

void* getBackElement(vector_t* vector)
{
    return vector->vec[(vector->size)-1];
}

void delete_vector_t( vector_t* vector)
{
    /* because elements stored are pointers, might be in use elsewhere */
    // int i;
    //for ( i = 0; i < vector->size; i++) 
        //free(vector->vec[i]);
    free(vector->vec);
    free(vector);
}

int findElement(vector_t* vector, void* elem)
{
   int i;
  for (i = 0; i < vector->size; i++) {
      if (vector->vec[i] == elem)
          return i+1;
  }

  return false; 
}


vector_t* eraseElement(vector_t* vector, void* elem)
{
    vector_t* new_vec = new_empty_vector_t();
    
    int i;
    for ( i = 0; i < vector->size; i++) {
        if (vector->vec[i] != elem) {
            pushBackElement(new_vec, vector->vec[i]);
        } 
    }

    /* free the vector, but don't free the element because
     * it might be referenced elsewhere */
    free(vector);

    return new_vec;
}

