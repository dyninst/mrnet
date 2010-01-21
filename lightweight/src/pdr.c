/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

/*
 * pdr.c, Generic PDR routines implementation.
 *
 * These are the "generic" pdr routines used to serialize and de-serialize
 * most common data items.  See pdr.h for more info on the interface to
 * pdr.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Utils.h"
#include "pdr.h"
#include "pdr_mem.h"

#define LASTUNSIGNED    ((uint32_t) 0-1)

/*
 * Free a data structure using PDR
 * Not a filter, but a convenient utility nonetheless
 */
void pdr_free(pdrproc_t proc, char *objp)
{
    PDR x;
    x.p_op = PDR_FREE;
    (*proc)(&x, objp);
}

bool_t pdr_void(PDR *p,  char *c )
{
    return (true);
}

bool_t pdr_char(PDR *pdrs, char *cp)
{
    switch (pdrs->p_op) {
    case PDR_FREE:
        return true;
    case PDR_ENCODE:
        return pdr_putchar(pdrs, cp);
    case PDR_DECODE:
        return pdr_getchar(pdrs, cp);
    }
    return false;
}

bool_t pdr_uchar(PDR *pdrs, uchar_t *uc)
{
    return pdr_char(pdrs, (char *)uc);
}

bool_t pdr_int16(PDR *pdrs, int16_t *ip)
{
    switch (pdrs->p_op) {
    case PDR_FREE:
        return true;
    case PDR_ENCODE:
        return pdr_putint16(pdrs, ip);
    case PDR_DECODE:
        return pdr_getint16(pdrs, ip);
    }
    return false;
}

bool_t pdr_uint16(PDR *pdrs, uint16_t *up)
{
    return pdr_int16(pdrs, (int16_t *)up);
}

bool_t pdr_int32(PDR *pdrs, int32_t *ip)
{
    switch (pdrs->p_op) {
    case PDR_FREE:
        return true;
    case PDR_ENCODE:
        return pdr_putint32(pdrs, ip);
    case PDR_DECODE:
        return pdr_getint32(pdrs, ip);
    }
    return false;
}
bool_t pdr_uint32(PDR *pdrs, uint32_t *up)
{
    return pdr_int32(pdrs, (int32_t *)up);
}

bool_t pdr_int64(PDR *pdrs, int64_t *ip)
{
    switch (pdrs->p_op) {
    case PDR_FREE:
        return true;
    case PDR_ENCODE:
        return pdr_putint64(pdrs, ip);
    case PDR_DECODE:
        return pdr_getint64(pdrs, ip);
    }
    return false;
}
bool_t pdr_uint64(PDR *pdrs, uint64_t *up)
{
    return pdr_int64(pdrs, (int64_t *)up);
}

bool_t pdr_float(PDR *pdrs, float *ip)
{
    switch (pdrs->p_op) {
    case PDR_FREE:
        return true;
    case PDR_ENCODE:
        return pdr_putfloat(pdrs, ip);
    case PDR_DECODE:
        return pdr_getfloat(pdrs, ip);
    }
    return false;
}
bool_t pdr_double(PDR *pdrs, double *ip)
{
    switch (pdrs->p_op) {
    case PDR_FREE:
        return true;
    case PDR_ENCODE:
        return pdr_putdouble(pdrs, ip);
    case PDR_DECODE:
        return pdr_getdouble(pdrs, ip);
    }
    return false;
}

/*
 * PDR booleans
 */
bool_t pdr_bool(PDR *pdrs, bool_t *bp)
{
    /* All Bool Types passed as a 32 bit int */
    int32_t l;
    switch(pdrs->p_op){
    case PDR_FREE:
        return true;
    case PDR_ENCODE:
        l = *bp;
        return pdr_int32(pdrs, &l);
    case PDR_DECODE:
        if(!pdr_int32(pdrs, &l)){
            return false;
        }
        *bp = l;
        return true;
    }
    return false;
}

/*
 * PDR enumerations
 */
bool_t pdr_enum(PDR *pdrs, enum_t *ep)
{
    /* All Enum Types passed as a 32 bit int */
    int32_t l;
    switch(pdrs->p_op){
    case PDR_FREE:
        return true;
    case PDR_ENCODE:
        l = *ep;
        return pdr_int32(pdrs, &l);
    case PDR_DECODE:
        if(!pdr_int32(pdrs, &l)){
            return false;
        }
        *ep = l;
        return true;
    }
    return false;
}


/*
 * Non-portable pdr primitives.
 * Care should be taken when moving these routines to new architectures.
 */


/*
 * PDR opaque data
 * Allows the specification of a fixed size sequence of opaque bytes.
 * cp points to the opaque object and cnt gives the byte length.
 */
bool_t pdr_opaque(PDR *pdrs, char * cp, uint32_t cnt)
{
    /* if no data we are done */
    if (cnt == 0) {
        return true;
    }

    switch (pdrs->p_op) {
    case PDR_DECODE:
        mrn_dbg(5, mrn_printf(FLF, stderr, "Calling pdr_getbytes ...\n" ));
        return pdr_getbytes(pdrs, cp, cnt);
    case PDR_ENCODE:
        return pdr_putbytes(pdrs, cp, cnt);
    case PDR_FREE:
        return true;
    }

    return false;
}

/*
 * PDR counted bytes
 * *cpp is a pointer to the bytes, *sizep is the count.
 * If *cpp is NULL maxsize bytes are allocated
 */
bool_t pdr_bytes(PDR *pdrs, char **cpp, uint32_t *sizep, uint32_t maxsize)
{
    char *sp = *cpp;  /* sp is the actual string pointer */
    uint32_t nodesize;

    /*
     * first deal with the length since pdr bytes are counted
     */
    if (!pdr_uint32(pdrs, sizep)) {
        return false;
    }
    nodesize = *sizep;
    if ((nodesize > maxsize) && (pdrs->p_op != PDR_FREE)) {
        return false;
    }

    /*
     * now deal with the actual bytes
     */
    switch (pdrs->p_op) {
    case PDR_DECODE:
        if (nodesize == 0) {
            return true;
        }
        if (sp == NULL) {
            *cpp = sp = (char *)malloc(nodesize);
        }
        if (sp == NULL) {
            return false;
        }
    case PDR_ENCODE:
        return pdr_opaque(pdrs, sp, nodesize);
    case PDR_FREE:
        if (sp != NULL) {
            free(sp);
            *cpp = NULL;
        }
        return true;
    }
    return false;
}

/*
 * PDR null terminated ASCII strings
 * pdr_string deals with "C strings" - arrays of bytes that are
 * terminated by a NULL character.  The parameter cpp references a
 * pointer to storage; If the pointer is null, then the necessary
 * storage is allocated.  The last parameter is the max allowed length
 * of the std::string as specified by a protocol.
 */

bool_t pdr_string(PDR *pdrs, char **cpp, uint32_t maxsize)
{
    char *sp = *cpp;  /* sp is the actual string pointer */
    uint32_t nodesize=0;

    /* mrn_dbg_func_begin(); */

    /* first deal with the length since pdr strings are counted strings */
    switch (pdrs->p_op) {
    case PDR_FREE:
        if (sp != NULL) {
            free(sp);
            *cpp = NULL;
        }
        return true;
    case PDR_ENCODE:
        if (sp == NULL) {
            return false;
        }
        nodesize = strlen(sp) + 1; /* add 1-byte null terminator */
        break;
    case PDR_DECODE:
        break;
    }
		
    if (!pdr_uint32(pdrs, &nodesize)) {
        return false;
    }

    if (nodesize > maxsize) {
        return false;
    }
		
    /*  now deal with the actual bytes */
    switch (pdrs->p_op) {
    case PDR_FREE:  /* Already handled above, but silences compiler warning */
        return true;
    case PDR_DECODE:
        mrn_dbg(5, mrn_printf(0,0,0, stderr, "String size+1: %u\n", nodesize ));
        if (sp == NULL) {
            mrn_dbg(5, mrn_printf(FLF, stderr, "Allocating memory ...\n" ));
            *cpp = sp = (char *)malloc(nodesize);
        }
        if (sp == NULL) {
            mrn_dbg(5, mrn_printf(FLF, stderr, "Malloc failed\n" ));
            return false;
        }
        sp[nodesize-1] = 0;
        /* fall into ... */
				
    case PDR_ENCODE:
        mrn_dbg(5, mrn_printf(FLF, stderr, "Calling pdr_opaque ...\n" ));
        return pdr_opaque(pdrs, sp, nodesize);
    }

    return false;
}

/*
 * Wrapper for pdr_string that can be called directly from routines like clnt_call
 */
bool_t pdr_wrapstring(PDR *pdrs, char **cpp)
{
    return pdr_string(pdrs, cpp, LASTUNSIGNED-1); 
}

/*
 * PDR an indirect pointer
 * pdr_reference is for recursively translating a structure that is
 * referenced by a pointer inside the structure that is currently being
 * translated.  pp references a pointer to storage. If *pp is null
 * the  necessary storage is allocated.
 * size is the sizeof the referneced structure.
 * proc is the routine to handle the referenced structure.
 */
bool_t pdr_reference(PDR *pdrs, char * *pp, uint32_t size, pdrproc_t proc)
{
    char * loc = *pp;
    bool_t stat;

    if (loc == NULL){
        switch (pdrs->p_op) {
        case PDR_FREE:
            return (true);

        case PDR_DECODE:
            *pp = loc = (char *) malloc(size);
            if (loc == NULL) {
                return (false);
            }
            memset(loc, 0, (int)size);
            break;

        case PDR_ENCODE:
            break;
        }
    }

    stat = (*proc)(pdrs, loc, LASTUNSIGNED);

    if (pdrs->p_op == PDR_FREE) {
        free(loc);
        *pp = NULL;
    }
    return (stat);
}


/*
 * pdr_pointer()
 *
 * PDR a pointer to a possibly recursive data structure. This
 * differs with pdr_reference in that it can serialize/deserialiaze
 * trees correctly.
 *
 *  What's sent is actually a union
 *
 *  union object_pointer switch (boolean b) {
 *  case true object_data data;
 *  case false void nothing;
 *  }
 *
 * > objpp Pointer to the pointer to the object.
 * > obj_size size of the object.
 * > pdr_obj routine to PDR an object.
 *
 */
bool_t pdr_pointer(PDR *pdrs, char **objpp, uint32_t obj_size, pdrproc_t pdr_obj)
{
    bool_t more_data = (*objpp != NULL);
    if (! pdr_bool(pdrs,&more_data)) {
        return (false);
    }
    if (! more_data) {
        *objpp = NULL;
        return (true);
    }
    return (pdr_reference(pdrs,objpp,obj_size,pdr_obj));
}
 
/*
 * PDR an array of arbitrary elements
 * *addrp is a pointer to the array, *sizep is the number of elements.
 * If addrp is NULL (*sizep * elsize) bytes are allocated.
 * elsize is the size (in bytes) of each element, and elproc is the
 * pdr procedure to call to handle each element of the array.
 */
bool_t pdr_array(PDR *pdrs, void **addrp, uint32_t *sizep, uint32_t maxsize,
                 uint32_t elsize, pdrproc_t elproc)
{
    uint32_t i;
    char * target = (char*) *addrp;
    uint32_t c;  /* the actual element count */
    bool_t stat = true;
    uint32_t nodesize;
 
    /* like strings, arrays are really counted arrays */
    if (! pdr_uint32(pdrs, sizep)) {
        return (false);
    }
    c = *sizep;
    if ((c > maxsize) && (pdrs->p_op != PDR_FREE)) {
        return (false);
    }
    nodesize = c * elsize;
 
    /*
     * if we are deserializing, we may need to allocate an array.
     * We also save time by checking for a null array if we are freeing.
     */
    if (target == NULL){
        switch (pdrs->p_op) {
        case PDR_DECODE:
            if (c == 0)
            return (true);
            *addrp = malloc(nodesize);
            target = (char*) *addrp;
            if (target == NULL) {
              return (false);
            }
            memset(target, 0, nodesize);
            break;
        case PDR_FREE:
            return (true);
        case PDR_ENCODE:
            break;
        }
    }
        
   /*
    * now we pdr each element of array
    */
     for (i = 0; (i < c) && stat; i++) {
         stat = (*elproc)(pdrs, target);
         target += elsize;
     }

     /*
       * the array may need freeing
       */
     if (pdrs->p_op == PDR_FREE) {
         free(*addrp);
         *addrp = NULL;
     }
     return (stat);
}

 /*
  * pdr_vector():
  *
  * PDR a fixed length array. Unlike variable-length arrays,
  * the storage of fixed length arrays is static and unfreeable.
  * > basep: base of the array
  * > size: size of the array
  * > elemsize: size of each element
  * > pdr_elem: routine to PDR each element
  */
bool_t pdr_vector(PDR *pdrs, char *basep, uint32_t nelem, uint32_t elemsize,
                  pdrproc_t pdr_elem)   
{
    uint32_t i;
    char *elptr;

    elptr = basep;
    for (i = 0; i < nelem; i++) {
        if (! (*pdr_elem)(pdrs, elptr)) {
            return(false);
        }
        elptr += elemsize;
    }
    return(true);  
}