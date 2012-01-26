/****************************************************************************
 *  Copyright 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__pdr_mem_h)
#define __pdr_mem_h 

#include "pdr.h"

#ifdef __cplusplus
extern "C" {
#endif

inline bool_t   pdrmem_putchar(PDR *, char *);
inline bool_t   pdrmem_getchar(PDR *, char *);

inline bool_t   pdrmem_putint16(PDR *, int16_t *);
inline bool_t   pdrmem_getint16(PDR *, int16_t *);
inline bool_t   pdrmem_getint16_swap(PDR *, int16_t *);

inline bool_t   pdrmem_putint32(PDR *, int32_t *);
inline bool_t   pdrmem_getint32(PDR *, int32_t *);
inline bool_t   pdrmem_getint32_swap(PDR *, int32_t *);

inline bool_t   pdrmem_putint64(PDR *, int64_t *);
inline bool_t   pdrmem_getint64(PDR *, int64_t *);
inline bool_t   pdrmem_getint64_swap(PDR *, int64_t *);

inline bool_t   pdrmem_putfloat(PDR *, float *);
inline bool_t   pdrmem_getfloat(PDR *, float *);
inline bool_t   pdrmem_getfloat_swap(PDR *, float *);

inline bool_t   pdrmem_putdouble(PDR *, double *);
inline bool_t   pdrmem_getdouble(PDR *, double *);
inline bool_t   pdrmem_getdouble_swap(PDR *, double *);

inline bool_t   pdrmem_getbytes(PDR *, char *, uint64_t);
inline bool_t   pdrmem_putbytes(PDR *, char *, uint64_t);

bool_t   pdrmem_setpos(PDR *, uint32_t);
inline uint32_t pdrmem_getpos(PDR *);
int32_t* pdrmem_inline(PDR *, int32_t);
void     pdrmem_destroy(PDR *);

#ifdef __cplusplus
} /* extern C */
#endif

#endif /* __pdr_mem_h */
