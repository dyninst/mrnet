/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "mrnet_lightweight/DataElement.h"
#include "utils_lightweight.h"

DataElement_t* new_DataElement_t()
{
    DataElement_t* data_element = (DataElement_t*) calloc( 1, sizeof(DataElement_t) );
    assert(data_element != NULL);
    return data_element;
}

void delete_DataElement_t( DataElement_t* de )
{
    uint32_t i;
    char** str_arr;

    if( de == NULL )
        return;

    if( de->destroy_data == true ) {
        switch( de->type ) {
         case STRING_T:
         case CHAR_ARRAY_T:
         case UCHAR_ARRAY_T:
         case INT16_ARRAY_T:
         case UINT16_ARRAY_T:
         case INT32_ARRAY_T:
         case UINT32_ARRAY_T:
         case INT64_ARRAY_T:
         case UINT64_ARRAY_T:
         case FLOAT_ARRAY_T:
         case DOUBLE_ARRAY_T:
         case CHAR_LRG_ARRAY_T:
         case UCHAR_LRG_ARRAY_T:
         case INT16_LRG_ARRAY_T:
         case UINT16_LRG_ARRAY_T:
         case INT32_LRG_ARRAY_T:
         case UINT32_LRG_ARRAY_T:
         case INT64_LRG_ARRAY_T:
         case UINT64_LRG_ARRAY_T:
         case FLOAT_LRG_ARRAY_T:
         case DOUBLE_LRG_ARRAY_T:
             if( de->val.p != NULL ) {
                 free( de->val.p );
             }
             break;
         case STRING_LRG_ARRAY_T:
         case STRING_ARRAY_T:
             if( de->val.p != NULL ) {
                 str_arr = (char**)(de->val.p);
                 for( i = 0; i < de->array_len; i++ ) {
                     if( str_arr[i] != NULL )
                         free( str_arr[i] );
                 } 
                 free( de->val.p );
             }
             break;
         default:
             break;
        }
    }
    free( de );
}

DataType DataType_Fmt2Type (char* cur_fmt) 
{
    switch( cur_fmt[0] ) {
    case 'a':
        if( ! strcmp(cur_fmt, "ac") )
            return CHAR_ARRAY_T;
        else if( ! strcmp(cur_fmt, "auc") )
            return UCHAR_ARRAY_T;
        else if( ! strcmp(cur_fmt, "ad") )
            return INT32_ARRAY_T;
        else if( ! strcmp(cur_fmt, "aud") )
            return UINT32_ARRAY_T;
        else if( ! strcmp(cur_fmt, "ahd") )
            return INT16_ARRAY_T;
        else if( ! strcmp(cur_fmt, "auhd") )
            return UINT16_ARRAY_T;
        else if( ! strcmp(cur_fmt, "ald") )
            return INT64_ARRAY_T;
        else if( ! strcmp(cur_fmt, "auld") )
            return UINT64_ARRAY_T;
        else if( ! strcmp(cur_fmt, "af") )
            return FLOAT_ARRAY_T;
        else if( ! strcmp(cur_fmt, "alf") )
            return DOUBLE_ARRAY_T;
        else if( ! strcmp(cur_fmt, "as") )
            return STRING_ARRAY_T;
        break; 
    case 'A':
        if( ! strcmp(cur_fmt, "Ac") )
            return CHAR_LRG_ARRAY_T;
        else if( ! strcmp(cur_fmt, "Auc") )
            return UCHAR_LRG_ARRAY_T;
        else if( ! strcmp(cur_fmt, "Ad") )
            return INT32_LRG_ARRAY_T;
        else if( ! strcmp(cur_fmt, "Aud") )
            return UINT32_LRG_ARRAY_T;
        else if( ! strcmp(cur_fmt, "Ahd") )
            return INT16_LRG_ARRAY_T;
        else if( ! strcmp(cur_fmt, "Auhd") )
            return UINT16_LRG_ARRAY_T;
        else if( ! strcmp(cur_fmt, "Ald") )
            return INT64_LRG_ARRAY_T;
        else if( ! strcmp(cur_fmt, "Auld") )
            return UINT64_LRG_ARRAY_T;
        else if( ! strcmp(cur_fmt, "Af") )
            return FLOAT_LRG_ARRAY_T;
        else if( ! strcmp(cur_fmt, "Alf") )
            return DOUBLE_LRG_ARRAY_T;
        else if( ! strcmp(cur_fmt, "As") )
            return STRING_LRG_ARRAY_T;
        break;
    
    case 'c':
        if( ! strcmp(cur_fmt, "c") )
            return CHAR_T;
        break;
    case 'd':
        if( ! strcmp(cur_fmt, "d") )
            return INT32_T;
        break;
    case 'f':
        if( ! strcmp(cur_fmt, "f") )
            return FLOAT_T;
        break;
    case 'h':
        if( ! strcmp(cur_fmt, "hd") )
            return INT16_T;
        break;
    case 'l':
        if( ! strcmp(cur_fmt, "ld") )
            return INT64_T;
        else if( ! strcmp(cur_fmt, "lf") )
            return DOUBLE_T;
        break;
    case 's':
        if( ! strcmp(cur_fmt, "s") )
            return STRING_T;
        break;
    case 'u':
        if( ! strcmp(cur_fmt, "uc") )
            return UCHAR_T;
        else if( ! strcmp(cur_fmt, "uhd") )
            return UINT16_T;
        else if( ! strcmp(cur_fmt, "ud") )
            return UINT32_T;
        else if( ! strcmp(cur_fmt, "uld") )
            return UINT64_T;
        break;
    default:
        break;
    }
    return UNKNOWN_T;
}


