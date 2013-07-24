/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(test_nativefilters_h )
#define test_nativefilters_h 1

#include "timer.h"

typedef enum { PROT_EXIT=FirstApplicationTag, PROT_SUM, PROT_MAX } Protocol;

const char CHARVAL=7;
const unsigned char UCHARVAL=7;
const int16_t INT16VAL=-17;
const uint16_t UINT16VAL=17;
const int32_t INT32VAL=-117;
const uint32_t UINT32VAL=117;
const int64_t INT64VAL=-177;
const uint64_t UINT64VAL=177;
const float FLOATVAL=(float)123.450;
const double DOUBLEVAL=123.45678;

#endif /* test_nativefilters_h */
