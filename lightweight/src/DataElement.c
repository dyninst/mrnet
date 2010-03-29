/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "mrnet/DataElement.h"
#include "utils_lightweight.h"

DataElement_t* new_DataElement_t()
{
  DataElement_t* data_element = (DataElement_t*)malloc(sizeof(DataElement_t));
  assert(data_element != NULL);
  return data_element;
}


DataType DataType_Fmt2Type (char* cur_fmt) 
{
   
    if( !strcmp(cur_fmt, "c") )
        return CHAR_T;
    else if( !strcmp(cur_fmt, "uc") )
        return UCHAR_T;
    else if( !strcmp(cur_fmt, "ac") )
        return CHAR_ARRAY_T;
    else if( !strcmp(cur_fmt, "auc") )
        return UCHAR_ARRAY_T;
    else if( !strcmp(cur_fmt, "hd") )
        return INT16_T;
    else if( !strcmp(cur_fmt, "uhd") )
        return UINT16_T;
    else if( !strcmp(cur_fmt, "d") ) {
        return INT32_T;}
    else if( !strcmp(cur_fmt, "ud") )
        return UINT32_T;
    else if( !strcmp(cur_fmt, "ahd") )
        return INT16_ARRAY_T;
    else if( !strcmp(cur_fmt, "ld") )
        return INT64_T;
    else if( !strcmp(cur_fmt, "uld") )
        return UINT64_T;
    else if( !strcmp(cur_fmt, "auhd") )
        return UINT16_ARRAY_T;
    else if( !strcmp(cur_fmt, "ad") )
        return INT32_ARRAY_T;
    else if( !strcmp(cur_fmt, "aud") )
        return UINT32_ARRAY_T;
    else if( !strcmp(cur_fmt, "ald") )
        return INT64_ARRAY_T;
    else if( !strcmp(cur_fmt, "auld") )
        return UINT64_ARRAY_T;
    else if( !strcmp(cur_fmt, "f") )
        return FLOAT_T;
    else if( !strcmp(cur_fmt, "af") )
        return FLOAT_ARRAY_T;
    else if( !strcmp(cur_fmt, "lf") )
        return DOUBLE_T;
    else if( !strcmp(cur_fmt, "alf") )
        return DOUBLE_ARRAY_T;
    else if( !strcmp(cur_fmt, "s") )
        return STRING_T;
    else if( !strcmp(cur_fmt, "as") )
        return STRING_ARRAY_T;
    else
        return UNKNOWN_T;
}


