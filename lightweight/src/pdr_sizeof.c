/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

//#include <cstdlib>
//#include <cstdio>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdr.h"
#include "pdr_mem.h"
#include "Utils.h"
//#include "config.h"

#if defined(__cplusplus)
extern "C" {
#endif
static bool_t _putchar(PDR *pdrs, char *c)
{
    pdrs->space += SIZEOF_CHAR;
    return true;
}
static bool_t _getchar(PDR *p, char *c)
{
    return false;
}

static bool_t _putint16(PDR *pdrs, int16_t *i)
{
    pdrs->space += SIZEOF_INT16;
    return true;
}
static bool_t _getint16(PDR *p, int16_t *i)
{
    return false;
}

static bool_t _putint32(PDR *pdrs, int32_t *i)
{
    pdrs->space += SIZEOF_INT32;
    return true;
}
static bool_t _getint32(PDR *p, int32_t *i)
{
    return false;
}

static bool_t _putint64(PDR *pdrs, int64_t *i)
{
    pdrs->space += SIZEOF_INT64;
    return true;
}
static bool_t _getint64(PDR *p, int64_t *i)
{
    return false;
}

static bool_t _putfloat(PDR *pdrs, float *f)
{
    pdrs->space += SIZEOF_FLOAT;
    return true;
}
static bool_t _getfloat(PDR *p, float *f)
{
    return false;
}

static bool_t _putdouble(PDR *pdrs, double *d)
{
    pdrs->space += SIZEOF_DOUBLE;
    return true;
}
static bool_t _getdouble(PDR *p, double *d)
{
    return false;
}

static bool_t _putbytes(PDR *pdrs, char *c, uint32_t len)
{
    pdrs->space += SIZEOF_CHAR*len;
    return true;
}
static bool_t _getbytes(PDR *p, char *c, uint32_t u)
{
    return false;
}

static uint32_t _getpos(PDR *pdrs)
{
    return pdrs->space;
}

static bool_t _setpos(PDR *p, uint32_t u)
{
    return false;
}

static void _destroy(PDR *pdrs)
{
    pdrs->space = 0;
    pdrs->cur = 0;
    if (pdrs->base){
        free (pdrs->base);
        pdrs->base = NULL;
    }
    return;
}


static int32_t * _makeinline (PDR *pdrs, int32_t len)
{
    if (len == 0 || pdrs->p_op != PDR_ENCODE){
        return NULL;
    }

    if (len < (int) (long int) pdrs->base){
        /* cur was already allocated */
        pdrs->space += len;
        return (int32_t *) pdrs->cur;
    }
    else {
        /* Free the earlier space and allocate new area */
        if (pdrs->cur)
            free (pdrs->cur);
        if ((pdrs->cur = (char *) malloc (len)) == NULL) {
            pdrs->base = 0;
            return NULL;
        }
        pdrs->base = (char *) (long) len;
        pdrs->space += len;
        return (int32_t *) pdrs->cur;
    }
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
    _makeinline,
    _destroy
};

uint32_t pdr_sizeof(pdrproc_t func, void *data)
{
    
    PDR pdrs;
    bool_t stat;

    pdrs.p_op = PDR_ENCODE;
    pdrs.p_ops = &_ops;
    pdrs.space = 1;  /* 1-byte byte ordering entity */
    pdrs.cur = (caddr_t) NULL;
    pdrs.base = (caddr_t) 0;
  
    stat = func (&pdrs, data);
    if (pdrs.cur)
        free (pdrs.cur);

    return stat == true ? pdrs.space : 0;
}

#if defined(__cplusplus)
}
#endif
