/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#ifndef  __byte_order_h
#define __byte_order_h 1

#ifdef __cplusplus
# include "utils.h"
extern "C" {
#else
# include "utils_lightweight.h"
#endif /* __cplusplus */

#define ntoh_bytes hton_bytes
void hton_bytes(char * out, char * in, uint32_t elsize);
/* Functions convert to/from network byte order (big-endian) as necessary */
#define ntoh_int16 hton_int16
#define hton_int16(out, in) \
        hton_bytes((char *)out, (char *)in, sizeof(int16_t));
#define ntoh_int32 hton_int32
#define hton_int32(out, in) \
        hton_bytes((char *)out, (char *)in, sizeof(int32_t));
#define ntoh_int64 hton_int64
#define hton_int64(out, in) \
        hton_bytes((char *)out, (char *)in, sizeof(int64_t));
#define ntoh_float hton_float
#define hton_float(out, in) \
        hton_bytes((char *)out, (char *)in, sizeof(float));
#define ntoh_double hton_double
#define hton_double(out, in) \
        hton_bytes((char *)out, (char *)in, sizeof(double));

void byte_swap(char * out, char * in, uint32_t nelems, uint32_t elemsize);
/* These functions always swap byte order*/
#define swap_int16(out, in) \
    byte_swap((char *) out, (char *)in, 1, (uint32_t) sizeof(uint16_t) );
#define swap_int32(out, in) \
    byte_swap((char *) out, (char *)in, 1, (uint32_t) sizeof(uint32_t) );
#define swap_int64(out, in) \
    byte_swap((char *) out, (char *)in, 1, (uint32_t) sizeof(uint64_t) );
#define swap_float(out, in) \
    byte_swap((char *) out, (char *)in, 1, (uint32_t) sizeof(float) );
#define swap_double(out, in) \
    byte_swap((char *) out, (char *)in, 1, (uint32_t) sizeof(double) );

void byte_swap_inplace(char * inout, uint32_t nelems, uint32_t elemsize);
#define swap_inplace_int16(in) \
        byte_swap_inplace((char *)in, 1, (uint32_t) sizeof(uint16_t) );
#define swap_inplace_int32(in) \
        byte_swap_inplace((char *)in, 1, (uint32_t) sizeof(uint32_t) );
#define swap_inplace_int64(in) \
        byte_swap_inplace((char *)in, 1, (uint32_t) sizeof(uint64_t) );
#define swap_inplace_float(in) \
        byte_swap_inplace((char *)in, 1, (uint32_t) sizeof(float) );
#define swap_inplace_double(in) \
        byte_swap_inplace((char *)in, 1, (uint32_t) sizeof(double) );

#ifdef __cplusplus
} /* extern C */
#endif

#endif /* __byte_order_h */
