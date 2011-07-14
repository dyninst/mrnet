/****************************************************************************
 *  Copyright 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "byte_order.h"
#include "pdr_mem.h"

#ifndef os_windows // unix
#include "config.h"
#endif

#if defined(__cplusplus)

#include "utils.h"
using namespace MRN;

#else /* ! defined(__cplusplus) */ 

#include "utils_lightweight.h"
#include <string.h>
#include <assert.h>

#endif /* defined(__cplusplus) */


static struct pdr_ops pdrmem_ops = {
    pdrmem_putchar,
    pdrmem_getchar,
    pdrmem_putint16,
    pdrmem_getint16,
    pdrmem_putint32,
    pdrmem_getint32,
    pdrmem_putint64,
    pdrmem_getint64,
    pdrmem_putfloat,
    pdrmem_getfloat,
    pdrmem_putdouble,
    pdrmem_getdouble,
    pdrmem_putbytes,
    pdrmem_getbytes,
    pdrmem_setpos,
    pdrmem_getpos,
    pdrmem_inline,
    pdrmem_destroy
};

static struct pdr_ops pdrmem_ops_swap = {
    pdrmem_putchar,
    pdrmem_getchar,
    pdrmem_putint16,
    pdrmem_getint16_swap,
    pdrmem_putint32,
    pdrmem_getint32_swap,
    pdrmem_putint64,
    pdrmem_getint64_swap,
    pdrmem_putfloat,
    pdrmem_getfloat_swap,
    pdrmem_putdouble,
    pdrmem_getdouble_swap,
    pdrmem_putbytes,
    pdrmem_getbytes,
    pdrmem_setpos,
    pdrmem_getpos,
    pdrmem_inline,
    pdrmem_destroy
};

/*
 * The procedure pdrmem_create initializes a stream descriptor for a memory buffer. 
 */
void pdrmem_create(PDR *pdrs, char * addr, uint32_t size, enum pdr_op op)
{

#if defined(WORDS_BIGENDIAN)
    static char local_bo = 1;  /* Big Endian */
#else
    static char local_bo = 0;  /* Little Endian */
#endif
    char remote_bo;

    pdrs->cur = pdrs->base = addr;
    pdrs->space = size;
    pdrs->p_ops = &pdrmem_ops;
    pdrs->p_op = op;

    if(pdrs->p_op == PDR_ENCODE){
        if(!pdr_char(pdrs, (char *)&local_bo)){
            pdrs->space = 0;
        }
    }
    else if(pdrs->p_op == PDR_DECODE){
        if(!pdr_char(pdrs, (char *)&remote_bo)){
            pdrs->space = 0;
        }
        if(remote_bo != local_bo){
            pdrs->p_ops = &pdrmem_ops_swap;
            return;
        }
    }
}

/*
 *  pdrmem_xxxchar(): Procedures for putting/getting 1 byte CHARS.
 */
bool_t pdrmem_putchar(PDR *pdrs, char *p)
{
    if(SIZEOF_CHAR > pdrs->space){
        return FALSE;
    }
    *((char *)(pdrs->cur)) = *p;
    pdrs->cur += SIZEOF_CHAR;
    return TRUE;
}
bool_t pdrmem_getchar(PDR *pdrs, char *p)
{
    if(SIZEOF_CHAR > pdrs->space){
        return FALSE;
    }
    *p = *((char *)(pdrs->cur));
    pdrs->cur += SIZEOF_CHAR;
    return TRUE;
}

/*
 *  pdrmem_xxxint16(): Procedures for puting/getting 16 bit INTS.
 */
bool_t pdrmem_putint16(PDR *pdrs, int16_t *p)
{
    int i;
    if(SIZEOF_INT16 > pdrs->space){
        return FALSE;
    }
    assert(pdrs->p_op == PDR_ENCODE);

    for(i = 0 ; i < SIZEOF_INT16 ; i++)
        *(((int8_t *)(pdrs->cur))+i)= (int8_t)*(((int8_t *)p)+i);

    //*((int16_t *)(pdrs->cur)) = *p;
    pdrs->cur += SIZEOF_INT16;

    return TRUE;
}

bool_t pdrmem_getint16(PDR *pdrs, int16_t *p)
{
    int i;
    if(SIZEOF_INT16 > pdrs->space){
        return FALSE;
    }
    assert(pdrs->p_op == PDR_DECODE);

    for(i = 0 ; i < SIZEOF_INT16 ; i++)
        *(((int8_t *)p)+i) = *(((int8_t *)(pdrs->cur))+i);

    //*p = *((int16_t *)(pdrs->cur));
    pdrs->cur += SIZEOF_INT16;
    return TRUE;
}

bool_t pdrmem_getint16_swap(PDR *pdrs, int16_t *p)
{
    if(SIZEOF_INT16 > pdrs->space){
        return FALSE;
    }
    assert(pdrs->p_op == PDR_DECODE);
    swap_int16(p,  pdrs->cur);
    pdrs->cur += SIZEOF_INT16;
    return TRUE;
}

/*
 *  pdrmem_xxxint32(): Procedures for puting/getting 32 bit INTS.
 */
bool_t pdrmem_putint32(PDR *pdrs, int32_t *p)
{
    int i;
    if(SIZEOF_INT32 > pdrs->space){
        return FALSE;
    }

    assert(pdrs->p_op == PDR_ENCODE);

    for(i = 0 ; i < SIZEOF_INT32 ; i++)
        *(((int8_t *)(pdrs->cur))+i)= (int8_t)*(((int8_t *)p)+i);

    pdrs->cur += SIZEOF_INT32;

    return TRUE;
}

bool_t pdrmem_getint32(PDR *pdrs, int32_t *p)
{
    int i;
    if(SIZEOF_INT32 > pdrs->space){
        return FALSE;
    }
    assert(pdrs->p_op == PDR_DECODE);

    for(i = 0 ; i < SIZEOF_INT32 ; i++)
        *(((int8_t *)p)+i) = *(((int8_t *)(pdrs->cur))+i);

    pdrs->cur += SIZEOF_INT32;
    return TRUE;
}

bool_t pdrmem_getint32_swap(PDR *pdrs, int32_t *p)
{
    if(SIZEOF_INT32 > pdrs->space){
        return FALSE;
    }
    assert(pdrs->p_op == PDR_DECODE);
    swap_int32(p,  pdrs->cur);
    pdrs->cur += SIZEOF_INT32;
    return TRUE;
}

/*
 *  pdrmem_xxxint64(): Procedures for puting/getting 64 bit INTS.
 */
bool_t pdrmem_putint64(PDR *pdrs, int64_t *p)
{
    int i;
    if(SIZEOF_INT64 > pdrs->space){
        return FALSE;
    }

    assert(pdrs->p_op == PDR_ENCODE);
    for(i = 0 ; i < SIZEOF_INT64 ; i++)
        *(((int8_t *)(pdrs->cur))+i)= (int8_t)*(((int8_t *)p)+i);

    pdrs->cur += SIZEOF_INT64;

    return TRUE;
}

bool_t pdrmem_getint64(PDR *pdrs, int64_t *p)
{
    int i;
    if(SIZEOF_INT64 > pdrs->space){
        return FALSE;
    }
    assert(pdrs->p_op == PDR_DECODE);

    for(i = 0 ; i < SIZEOF_INT64 ; i++)
        *(((int8_t *)p)+i) = *(((int8_t *)(pdrs->cur))+i);

    pdrs->cur += SIZEOF_INT64;

    return TRUE;
}

bool_t pdrmem_getint64_swap(PDR *pdrs, int64_t *p)
{

    if(SIZEOF_INT64 > pdrs->space){
        return FALSE;
    }
    assert(pdrs->p_op == PDR_DECODE);
    swap_int64(p,  pdrs->cur);
    pdrs->cur += SIZEOF_INT64;
    return TRUE;
}

/*
 *  pdrmem_xxxfloat(): Procedures for puting/getting 32 bit FLOATS.
 */
bool_t pdrmem_putfloat(PDR *pdrs, float *p)
{
    int i;
    if(SIZEOF_FLOAT > pdrs->space){
        return FALSE;
    }

    assert(pdrs->p_op == PDR_ENCODE);
    for(i = 0 ; i < SIZEOF_FLOAT ; i++)
        *(((int8_t *)(pdrs->cur))+i)= (int8_t)*(((int8_t *)p)+i);

    pdrs->cur += SIZEOF_FLOAT;

    return TRUE;
}

bool_t pdrmem_getfloat(PDR *pdrs, float *p)
{
    int i;
    if(SIZEOF_FLOAT > pdrs->space){
        return FALSE;
    }
    assert(pdrs->p_op == PDR_DECODE);
    for(i = 0 ; i < SIZEOF_FLOAT ; i++)
        *(((int8_t *)p)+i) = *(((int8_t *)(pdrs->cur))+i);

    pdrs->cur += SIZEOF_FLOAT;
    return TRUE;
}

bool_t pdrmem_getfloat_swap(PDR *pdrs, float *p)
{
    if(SIZEOF_FLOAT > pdrs->space){
        return FALSE;
    }
    assert(pdrs->p_op == PDR_DECODE);
    swap_float(p,  pdrs->cur);
    pdrs->cur += SIZEOF_FLOAT;
    return TRUE;
}

/*
 *  pdrmem_xxxdouble(): Procedures for puting/getting 16 bit INTS.
 */
bool_t pdrmem_putdouble(PDR *pdrs, double *p)
{
    int i;
    if(SIZEOF_DOUBLE > pdrs->space){
        return FALSE;
    }

    assert(pdrs->p_op == PDR_ENCODE);
    for(i = 0 ; i < SIZEOF_DOUBLE ; i++)
        *(((int8_t *)(pdrs->cur))+i)= (int8_t)*(((int8_t *)p)+i);

    pdrs->cur += SIZEOF_DOUBLE;

    return TRUE;
}

bool_t pdrmem_getdouble(PDR *pdrs, double *p)
{
    int i;
    if(SIZEOF_DOUBLE > pdrs->space){
        return FALSE;
    }
    assert(pdrs->p_op == PDR_DECODE);
    for(i = 0 ; i < SIZEOF_DOUBLE ; i++)
        *(((int8_t *)p)+i) = *(((int8_t *)(pdrs->cur))+i);

    pdrs->cur += SIZEOF_DOUBLE;
    return TRUE;
}

bool_t pdrmem_getdouble_swap(PDR *pdrs, double *p)
{
    if(SIZEOF_DOUBLE > pdrs->space){
        return FALSE;
    }
    assert(pdrs->p_op == PDR_DECODE);
    swap_double(p,  pdrs->cur);
    pdrs->cur += SIZEOF_DOUBLE;
    return TRUE;
}

bool_t pdrmem_getbytes(PDR *pdrs, char * addr,  uint32_t len)
{
    if(len > pdrs->space) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "Not enough data left: %u\n",
                              pdrs->space ));
        return (FALSE);
    }
    memcpy(addr, pdrs->cur, (size_t)len);
    pdrs->cur += len;
    return (TRUE);
}

bool_t pdrmem_putbytes(PDR *pdrs, char * addr,  uint32_t len)
{
    if(len > pdrs->space) {
        return (FALSE);
    }
    memcpy(pdrs->cur, addr, (size_t)len);
    pdrs->cur += len;
    return (TRUE);
}

#if defined( os_solaris )
#include <sys/int_limits.h>
#endif

/* TODO: is the return size right, given 64-bit pointers? */
uint32_t pdrmem_getpos( PDR *pdrs )
{
	unsigned long difference = ((unsigned long)pdrs->cur) - ((unsigned long)pdrs->base);
	assert( difference < UINT32_MAX );
	return (uint32_t) difference;
}

/* TODO: is the parameter size right, given 64-bit pointers? */
bool_t pdrmem_setpos( PDR *pdrs, uint32_t pos )
{
    unsigned long difference;
	char * newaddr = pdrs->base + pos;
    char * lastaddr = pdrs->cur + pdrs->space;

	if( newaddr > lastaddr )
		{
	    return (FALSE);
		}

    pdrs->cur = newaddr;
	difference = ((unsigned long)lastaddr) - ((unsigned long)newaddr);
	assert( difference < UINT32_MAX );   	
    pdrs->space = difference;
    return (TRUE);
}

int32_t * pdrmem_inline(PDR *pdrs, int32_t len)
{
    int32_t *buf = 0;

    if (pdrs->space >= (uint32_t)len) {
        pdrs->space -= len;
        buf = (int32_t *) pdrs->cur;
        pdrs->cur += len;
    }
    return (buf);
}

void pdrmem_destroy(PDR *pdrs)
{
}
