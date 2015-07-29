/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "byte_order.h"
#include "pdr.h"

/*
 * pdrmem_bytecopy - This is a compatability function for GCC on Solaris 
 */  
void pdrmem_bytecopy (void * source, void * dest, size_t d_size)
{
    size_t i;
    for(i = 0 ; i < d_size; i++)
        *(((int8_t *)(dest))+i)= (int8_t)*(((int8_t *)source)+i);
}

/*
 *  pdrmem_xxxchar(): Procedures for putting/getting 1 byte CHARS.
 */
static bool_t pdrmem_putchar(PDR *pdrs, char *p)
{
    if( SIZEOF_CHAR > pdrs->space ) {
        return FALSE;
    }
    
#ifdef os_solaris 
    pdrmem_bytecopy ((void *)p,(void *)pdrs->cur, SIZEOF_CHAR);
#else
    *((char *)(pdrs->cur)) = *p;
#endif


    pdrs->cur += SIZEOF_CHAR;
    pdrs->space -= SIZEOF_CHAR;
    return TRUE;
}

static bool_t pdrmem_getchar(PDR *pdrs, char *p)
{
    if( SIZEOF_CHAR > pdrs->space ) {
        return FALSE;
    }

#ifdef os_solaris 
    pdrmem_bytecopy ((void *)pdrs->cur,(void *)p, SIZEOF_CHAR);
#else
    *p = *((char *)(pdrs->cur));
#endif
    pdrs->cur += SIZEOF_CHAR;
    pdrs->space -= SIZEOF_CHAR;
    return TRUE;
}


/*
 *  pdrmem_xxxint16(): Procedures for puting/getting 16 bit INTS.
 */
static bool_t pdrmem_putint16(PDR *pdrs, int16_t *p)
{
    if( SIZEOF_INT16 > pdrs->space ) {
        return FALSE;
    }
    assert(pdrs->p_op == PDR_ENCODE);

#ifdef os_solaris 
    pdrmem_bytecopy ((void *)p,(void *)pdrs->cur, SIZEOF_INT16);
#else
    *((int16_t *)pdrs->cur) = *p;
#endif

    pdrs->cur += SIZEOF_INT16;
    pdrs->space -= SIZEOF_INT16;
    return TRUE;
}

static bool_t pdrmem_getint16(PDR *pdrs, int16_t *p)
{
    if( SIZEOF_INT16 > pdrs->space ) {
        return FALSE;
    }
    assert(pdrs->p_op == PDR_DECODE);

#ifdef os_solaris 
    pdrmem_bytecopy ((void *)pdrs->cur,(void *)p, SIZEOF_INT16);
#else
    *p = *((int16_t *)(pdrs->cur));
#endif

    pdrs->cur += SIZEOF_INT16;
    pdrs->space -= SIZEOF_INT16;
    return TRUE;
}

static bool_t pdrmem_getint16_swap(PDR *pdrs, int16_t *p)
{
    if( SIZEOF_INT16 > pdrs->space ) {
        return FALSE;
    }
    assert(pdrs->p_op == PDR_DECODE);
    swap_int16(p,  pdrs->cur);
    pdrs->cur += SIZEOF_INT16;
    pdrs->space -= SIZEOF_INT16;
    return TRUE;
}

/*
 *  pdrmem_xxxint32(): Procedures for puting/getting 32 bit INTS.
 */
static bool_t pdrmem_putint32(PDR *pdrs, int32_t *p)
{
    if( SIZEOF_INT32 > pdrs->space ) {
        return FALSE;
    }
    assert(pdrs->p_op == PDR_ENCODE);

#ifdef os_solaris 
    pdrmem_bytecopy ( (void *) p, (void *)pdrs->cur, SIZEOF_INT32);
#else
    *((int32_t *) pdrs->cur) = *p;
#endif 
    pdrs->cur += SIZEOF_INT32;
    pdrs->space -= SIZEOF_INT32;
    return TRUE;
}

static bool_t pdrmem_getint32(PDR *pdrs, int32_t *p)
{
    if( SIZEOF_INT32 > pdrs->space ) {
        return FALSE;
    }
    assert(pdrs->p_op == PDR_DECODE);

#ifdef os_solaris 
    pdrmem_bytecopy ( (void *) pdrs->cur, (void *)p, SIZEOF_INT32);
#else
    *p = *((int32_t *)pdrs->cur);
#endif
    
    pdrs->cur += SIZEOF_INT32;
    pdrs->space -= SIZEOF_INT32;
    return TRUE;
}

static bool_t pdrmem_getint32_swap(PDR *pdrs, int32_t *p)
{
    if( SIZEOF_INT32 > pdrs->space ) {
        return FALSE;
    }
    assert(pdrs->p_op == PDR_DECODE);

    swap_int32(p,  pdrs->cur);
    
    pdrs->cur += SIZEOF_INT32;
    pdrs->space -= SIZEOF_INT32;
    return TRUE;
}

/*
 *  pdrmem_xxxint64(): Procedures for puting/getting 64 bit INTS.
 */
static bool_t pdrmem_putint64(PDR *pdrs, int64_t *p)
{
    if( SIZEOF_INT64 > pdrs->space ) {
        return FALSE;
    }
    assert(pdrs->p_op == PDR_ENCODE);

#ifdef os_solaris 
    pdrmem_bytecopy ( (void *) p, (void *)pdrs->cur, SIZEOF_INT64);
#else
    *((int64_t*)pdrs->cur) = *p;
#endif
    pdrs->cur += SIZEOF_INT64;
    pdrs->space -= SIZEOF_INT64;
    return TRUE;
}

static bool_t pdrmem_getint64(PDR *pdrs, int64_t *p)
{
    if( SIZEOF_INT64 > pdrs->space ) {
        return FALSE;
    }
    assert(pdrs->p_op == PDR_DECODE);

#ifdef os_solaris 
    pdrmem_bytecopy ( (void *) pdrs->cur, (void *)p, SIZEOF_INT64);
#else
    *p = *((int64_t*)pdrs->cur);
#endif

    pdrs->cur += SIZEOF_INT64;
    pdrs->space -= SIZEOF_INT64;
    return TRUE;
}

static bool_t pdrmem_getint64_swap(PDR *pdrs, int64_t *p)
{
    if( SIZEOF_INT64 > pdrs->space ) {
        return FALSE;
    }
    assert(pdrs->p_op == PDR_DECODE);

    swap_int64(p,  pdrs->cur);

    pdrs->cur += SIZEOF_INT64;
    pdrs->space -= SIZEOF_INT64;
    return TRUE;
}

/*
 *  pdrmem_xxxfloat(): Procedures for puting/getting 32 bit FLOATS.
 */
static bool_t pdrmem_putfloat(PDR *pdrs, float *p)
{
    if( SIZEOF_FLOAT > pdrs->space ) {
        return FALSE;
    }
    assert(pdrs->p_op == PDR_ENCODE);

#ifdef os_solaris 
    pdrmem_bytecopy ( (void *) p, (void *)pdrs->cur, SIZEOF_FLOAT);
#else
    *((float *)pdrs->cur) = *p;
#endif

    pdrs->cur += SIZEOF_FLOAT;
    pdrs->space -= SIZEOF_FLOAT;
    return TRUE;
}

static bool_t pdrmem_getfloat(PDR *pdrs, float *p)
{
    if( SIZEOF_FLOAT > pdrs->space ) {
        return FALSE;
    }
    assert(pdrs->p_op == PDR_DECODE);

#ifdef os_solaris 
    pdrmem_bytecopy ( (void *) pdrs->cur, (void *) p, SIZEOF_FLOAT);
#else
    *p = *((float *)pdrs->cur);
#endif
    pdrs->cur += SIZEOF_FLOAT;
    pdrs->space -= SIZEOF_FLOAT;
    return TRUE;
}

static bool_t pdrmem_getfloat_swap(PDR *pdrs, float *p)
{
    if( SIZEOF_FLOAT > pdrs->space ) {
        return FALSE;
    }
    assert(pdrs->p_op == PDR_DECODE);

    swap_float(p,  pdrs->cur);

    pdrs->cur += SIZEOF_FLOAT;
    pdrs->space -= SIZEOF_FLOAT;
    return TRUE;
}

/*
 *  pdrmem_xxxdouble(): Procedures for puting/getting 16 bit INTS.
 */
static bool_t pdrmem_putdouble(PDR *pdrs, double *p)
{
    if( SIZEOF_DOUBLE > pdrs->space ) {
        return FALSE;
    }
    assert(pdrs->p_op == PDR_ENCODE);

#ifdef os_solaris 
    pdrmem_bytecopy ( (void *) p, (void *)pdrs->cur, SIZEOF_DOUBLE);
#else
    *((double *)pdrs->cur) = *p;
#endif

    pdrs->cur += SIZEOF_DOUBLE;
    pdrs->space -= SIZEOF_DOUBLE;
    return TRUE;
}

static bool_t pdrmem_getdouble(PDR *pdrs, double *p)
{
    if( SIZEOF_DOUBLE > pdrs->space ) {
        return FALSE;
    }
    assert(pdrs->p_op == PDR_DECODE);


#ifdef os_solaris 
    pdrmem_bytecopy ( (void *) pdrs->cur, (void *)p, SIZEOF_DOUBLE);
#else
    *p = *((double *)pdrs->cur);
#endif
    pdrs->cur += SIZEOF_DOUBLE;
    pdrs->space -= SIZEOF_DOUBLE;
    return TRUE;
}

static bool_t pdrmem_getdouble_swap(PDR *pdrs, double *p)
{
    if( SIZEOF_DOUBLE > pdrs->space ) {
        return FALSE;
    }
    assert(pdrs->p_op == PDR_DECODE);

    swap_double(p,  pdrs->cur);

    pdrs->cur += SIZEOF_DOUBLE;
    pdrs->space -= SIZEOF_DOUBLE;
    return TRUE;
}

static bool_t pdrmem_getbytes(PDR *pdrs, char *addr,  uint64_t len)
{
    if( len > pdrs->space ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "Not enough data left: %u\n",
                              pdrs->space ));
        return FALSE;
    }

    memcpy(addr, pdrs->cur, (size_t)len);

    pdrs->cur += len;
    pdrs->space -= len;
    return TRUE;
}

static bool_t pdrmem_putbytes(PDR *pdrs, char *addr,  uint64_t len)
{
    if( len > pdrs->space ) {
        return FALSE;
    }

    memcpy(pdrs->cur, addr, (size_t)len);

    pdrs->cur += len;
    pdrs->space -= len;
    return TRUE;
}

static uint64_t pdrmem_getpos( PDR *pdrs )
{
    unsigned long diff = ((unsigned long)pdrs->cur) - ((unsigned long)pdrs->base);
    return (uint64_t) diff;
}

static bool_t pdrmem_setpos( PDR *pdrs, uint64_t pos )
{
    unsigned long diff;
    char *newaddr = pdrs->base + pos;
    char *lastaddr = pdrs->cur + pdrs->space;

    if( newaddr > lastaddr )
        return FALSE;

    pdrs->cur = newaddr;
    diff = ((unsigned long)lastaddr) - ((unsigned long)newaddr);   	
    pdrs->space = (uint64_t) diff;
    return TRUE;
}

// We never allocate anything
static void pdrmem_destroy(PDR *pdrs)
{
    pdrs->cur = NULL;
    pdrs->base = NULL;
    pdrs->space = 0;
}

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
    pdrmem_destroy
};

#if defined(__cplusplus)
extern "C" {
#endif

/** 
 *  Gets the local byte ordering
 */
pdr_byteorder pdrmem_getbo(void)
{
#if defined(WORDS_BIGENDIAN)
    return PDR_BIGENDIAN; 
#else
    return PDR_LITTLEENDIAN;
#endif
}

/*
 * The procedure pdrmem_create initializes a stream descriptor for a memory buffer. 
 */
void pdrmem_create(PDR *pdrs, char *addr, uint64_t size, enum pdr_op op, pdr_byteorder bo)
{
    char local_bo = pdrmem_getbo();
    char remote_bo = bo;

    pdrs->cur = pdrs->base = addr;
    pdrs->space = size;
    pdrs->p_ops = &pdrmem_ops;
    pdrs->p_op = op;

    // @NOTE:WELTON This is where the byte order is set in pdr.
    if( PDR_DECODE == pdrs->p_op ) {
        // Set byte ordering flag
        if( remote_bo != local_bo ) {
            pdrs->p_ops = &pdrmem_ops_swap;
            return;
        }
    }
}

#if defined(__cplusplus)
} /* extern C */
#endif
