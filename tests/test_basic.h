/***********************************************************************
 * Copyright � 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.     *
 **********************************************************************/

#if !defined(test_basic_h)
#define test_basic_h 1

#include "mrnet/Types.h"

typedef enum {PROT_EXIT=FirstApplicationTag,
              PROT_CHAR, PROT_UCHAR,
              PROT_SHORT, PROT_USHORT,
              PROT_INT, PROT_UINT,
              PROT_LONG, PROT_ULONG,
              PROT_FLOAT, PROT_DOUBLE,
              PROT_STRING, PROT_ALL} Protocol;

#endif /* test_basic_h */
