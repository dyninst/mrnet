/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__vector_h)
#define __vector_h 1

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    void** vec;
    int size;
} vector_t;

vector_t* new_empty_vector_t();

void clear(vector_t* vector);

void pushBackElement(vector_t* vector, void* elem);

void* popBackElement(vector_t* vector);

void* getBackElement(vector_t* vector);

void delete_vector_t(vector_t* vector);

int findElement(vector_t* vector, void* elem);

vector_t* eraseElement(vector_t* vector, void* elem);

#endif /* __vector_h */
