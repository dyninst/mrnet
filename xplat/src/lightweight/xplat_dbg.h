/****************************************************************************
 *  Copyright 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__xplat_dbg_lightweight_h)
#define __xplat_dbg_lightweight_h 1

#define XPLAT_OUTPUT_LEVEL 1

#define xplat_dbg( x, y) \
do{ \
    if (XPLAT_OUTPUT_LEVEL >= x) {      \
        y;                            \
    }                                 \
}while(0)

#endif /* __xplat_dbg_lightweight_h */
