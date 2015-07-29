/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/


#include "pdr.h"

static bool_t _putchar(PDR *pdrs, char * UNUSED(c))
{
    pdrs->space += SIZEOF_CHAR;
    return TRUE;
}
static bool_t _getchar(PDR * UNUSED(prds), char * UNUSED(c))
{
    return FALSE;
}

static bool_t _putint16(PDR *pdrs, int16_t * UNUSED(i))
{
    pdrs->space += SIZEOF_INT16;
    return TRUE;
}
static bool_t _getint16(PDR * UNUSED(pdrs), int16_t * UNUSED(i))
{
    return FALSE;
}

static bool_t _putint32(PDR *pdrs, int32_t * UNUSED(i))
{
    pdrs->space += SIZEOF_INT32;
    return TRUE;
}
static bool_t _getint32(PDR * UNUSED(pdrs), int32_t * UNUSED(i))
{
    return FALSE;
}

static bool_t _putint64(PDR *pdrs, int64_t * UNUSED(i))
{
    pdrs->space += SIZEOF_INT64;
    return TRUE;
}
static bool_t _getint64(PDR * UNUSED(pdrs), int64_t * UNUSED(i))
{
    return FALSE;
}

static bool_t _putfloat(PDR *pdrs, float * UNUSED(f))
{
    pdrs->space += SIZEOF_FLOAT;
    return TRUE;
}
static bool_t _getfloat(PDR * UNUSED(pdrs), float * UNUSED(f))
{
    return FALSE;
}

static bool_t _putdouble(PDR *pdrs, double * UNUSED(d))
{
    pdrs->space += SIZEOF_DOUBLE;
    return TRUE;
}
static bool_t _getdouble(PDR * UNUSED(pdrs), double * UNUSED(d))
{
    return FALSE;
}

static bool_t _putbytes(PDR *pdrs, char * UNUSED(c), uint64_t len)
{
    pdrs->space += SIZEOF_CHAR*len;
    return TRUE;
}
static bool_t _getbytes(PDR * UNUSED(pdrs), char * UNUSED(c), uint64_t UNUSED(u))
{
    return FALSE;
}

static uint64_t _getpos(PDR *pdrs)
{
    return pdrs->space;
}

static bool_t _setpos(PDR * UNUSED(pdrs), uint64_t UNUSED(u))
{
    return FALSE;
}

static void _destroy(PDR *pdrs)
{
    pdrs->cur = NULL;
    pdrs->base = NULL;
    pdrs->space = 0;
}

static struct pdr_ops _ops = {
    _putchar,
    _getchar,
    _putint16,
    _getint16,
    _putint32,
    _getint32,
    _putint64,
    _getint64,
    _putfloat,
    _getfloat,
    _putdouble,
    _getdouble,
    _putbytes,
    _getbytes,
    _setpos,
    _getpos,
    _destroy
};

#if defined(__cplusplus)
extern "C" {
#endif

bool_t pdr_sizeof(pdrproc_t func, void *data,uint64_t * size)
{
    PDR pdrs;
    bool_t rc;

    pdrs.p_op = PDR_ENCODE;
    pdrs.p_ops = &_ops;
    pdrs.space = 0;  /* 1-byte byte ordering entity */
    pdrs.cur = NULL;
    pdrs.base = NULL;

    rc = func (&pdrs, data);
    if (pdrs.cur)
        free (pdrs.cur);

    if (rc == true)
        *size = pdrs.space;
    else
        *size = 0;
    return rc;
}

#if defined(__cplusplus)
} /* extern C */
#endif
