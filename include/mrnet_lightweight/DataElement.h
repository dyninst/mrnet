/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__dataelement_h)
#define __dataelement_h 1

#include <stdarg.h>

#include "mrnet_lightweight/Types.h"

typedef union {
    char c;
    unsigned char uc;
    int16_t hd;
    uint16_t uhd;
    int32_t d;
    uint32_t ud;
    int64_t ld;
    uint64_t uld;
    float f;
    double lf;
    void * p; // may need to be allocated by pdr routine
} DataValue;

typedef enum {
    UNKNOWN_T = 0, 
    CHAR_T, UCHAR_T,
    CHAR_ARRAY_T, UCHAR_ARRAY_T,
    STRING_T, STRING_ARRAY_T,
    INT16_T, UINT16_T,
    INT16_ARRAY_T, UINT16_ARRAY_T,
    INT32_T, UINT32_T,
    INT32_ARRAY_T, UINT32_ARRAY_T,
    INT64_T, UINT64_T,
    INT64_ARRAY_T, UINT64_ARRAY_T,
    FLOAT_T, DOUBLE_T,
    FLOAT_ARRAY_T, DOUBLE_ARRAY_T,
    CHAR_LRG_ARRAY_T, UCHAR_LRG_ARRAY_T,
    STRING_LRG_ARRAY_T,
    INT16_LRG_ARRAY_T, UINT16_LRG_ARRAY_T,
    INT32_LRG_ARRAY_T, UINT32_LRG_ARRAY_T,
    INT64_LRG_ARRAY_T, UINT64_LRG_ARRAY_T,
    FLOAT_LRG_ARRAY_T, DOUBLE_LRG_ARRAY_T
} DataType;

typedef struct{
    DataValue val;
    DataType type;
    uint64_t array_len;
    int destroy_data;
} DataElement_t;

DataElement_t* new_DataElement_t();

void delete_DataElement_t( DataElement_t* de );

DataType DataType_Fmt2Type(char* cur_fmt);

#endif /* __dataelement_h */
