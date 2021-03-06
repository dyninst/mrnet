/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#ifndef __PDR_HEADER__
#define __PDR_HEADER__

#ifndef SIZEOF_INT16
# define SIZEOF_INT16 2
#endif
#ifndef SIZEOF_INT32
# define SIZEOF_INT32 4
#endif
#ifndef SIZEOF_INT64
# define SIZEOF_INT64 8
#endif

#ifdef __cplusplus
# include "utils.h"
using namespace MRN;
#else
# include "utils_lightweight.h"
#endif

#ifdef os_windows
#define inline __inline
#endif

#if !defined(SIZEOF_CHAR)
#  define SIZEOF_CHAR sizeof(char)
#endif
#if !defined(SIZEOF_FLOAT)
#  define SIZEOF_FLOAT sizeof(float)
#endif
#if !defined(SIZEOF_DOUBLE)
#  define SIZEOF_DOUBLE sizeof(double)
#endif

#ifndef bool_t
# define bool_t int32_t
#endif

#ifndef enum_t
# define enum_t int32_t
#endif

#ifndef uchar_t
# define uchar_t unsigned char
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef TRUE
#define TRUE (1)
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum pdr_op {
    PDR_FREE=0,
    PDR_ENCODE,
    PDR_DECODE
};

typedef enum pdr_bo {
    PDR_LITTLEENDIAN=0,
    PDR_BIGENDIAN=1
} pdr_byteorder;


/*
 * The PDR handle.
 * Contains operation which is being applied to the stream,
 * an operations vector for the paticular implementation (e.g. see pdr_mem.c),
 * and two private fields for the use of the particular impelementation.
 */
typedef struct PDR PDR;
struct pdr_ops {
    bool_t  (*putchar)(PDR *pdrs, char *cp);
    bool_t  (*getchar)(PDR *pdrs, char *cp);
    bool_t  (*putint16)(PDR *pdrs, int16_t *ip);
    bool_t  (*getint16)(PDR *pdrs, int16_t *ip);
    bool_t  (*putint32)(PDR *pdrs, int32_t *ip);
    bool_t  (*getint32)(PDR *pdrs, int32_t *ip);
    bool_t  (*putint64)(PDR *pdrs, int64_t *ip);
    bool_t  (*getint64)(PDR *pdrs, int64_t *ip);
    bool_t  (*putfloat)(PDR *pdrs, float *fp);
    bool_t  (*getfloat)(PDR *pdrs, float *fp);
    bool_t  (*putdouble)(PDR *pdrs, double *dp);
    bool_t  (*getdouble)(PDR *pdrs, double *dp);
    bool_t  (*putbytes)(PDR *pdrs, char *, uint64_t);
    bool_t  (*getbytes)(PDR *pdrs, char *, uint64_t);
    bool_t  (*setpos)(PDR *pdrs, uint64_t ip);
    uint64_t (*getpos)(PDR *pdrs);
    void    (*destroy)(PDR *pdrs);
};

struct PDR {
    enum pdr_op      p_op;       /* operation; */
    struct pdr_ops * p_ops; 

    char *   cur;      /* pointer to private data */
    char *   base;     /* private used for position info */
    uint64_t space;    /* extra private word */
};

/*
 * A pdrproc_t exists for each data type which is to be encoded or decoded.
 *
 * The second argument to the pdrproc_t is a pointer to an opaque pointer.
 * The opaque pointer generally points to a structure of the data type
 * to be decoded.  If this pointer is 0, then the type routines should
 * allocate dynamic storage of the appropriate size and return it.
 * bool_t       (*pdrproc_t)(PDR *, caddr_t *);
 */
typedef bool_t (*pdrproc_t)(PDR *, void *, ...);
#define NULL_pdrproc_t ((pdrproc_t)0)

/*
* These are the "generic" pdr routines.
*/
extern bool_t   pdr_void(PDR *pdrs, char *addr);
extern bool_t   pdr_char(PDR *pdrs, char *addr);
extern bool_t   pdr_uchar(PDR *pdrs, uchar_t *addr);
extern bool_t   pdr_int16(PDR *pdrs, int16_t *ip);
extern bool_t   pdr_uint16(PDR *pdrs, uint16_t *ip);
extern bool_t   pdr_int32(PDR *pdrs, int32_t *ip);
extern bool_t   pdr_uint32(PDR *pdrs, uint32_t *ip);
extern bool_t   pdr_int64(PDR *pdrs, int64_t *ip);
extern bool_t   pdr_uint64(PDR *pdrs, uint64_t *ip);
extern bool_t   pdr_float(PDR *pdrs, float *ip);
extern bool_t   pdr_double(PDR *pdrs, double *ip);

extern bool_t   pdr_bool(PDR *pdrs, bool_t *bp);
extern bool_t   pdr_enum(PDR *pdrs, enum_t *bp);

extern bool_t   pdr_opaque(PDR *pdrs, char *cp, uint64_t cnt);
extern bool_t   pdr_bytes(PDR *pdrs, char **cpp, uint64_t *sizep,
                          uint64_t maxsize);
extern bool_t   pdr_string(PDR *pdrs, char **cpp, uint32_t maxsize);
extern bool_t   pdr_wrapstring(PDR *pdrs, char **cpp);
extern bool_t   pdr_reference(PDR *pdrs, char **pp, uint32_t size,
                              pdrproc_t proc);
extern bool_t   pdr_pointer(PDR *pdrs, char **objpp, uint32_t obj_size,
                            pdrproc_t pdr_obj);

extern bool_t   pdr_array(PDR *pdrs, void **addrp, uint64_t *sizep,
                          uint64_t maxsize, uint32_t elsize, pdrproc_t elproc);
extern bool_t   pdr_vector(PDR *pdrs, char *basep, uint64_t nelem,
                          uint64_t elemsize, pdrproc_t pdr_elem);


/*
 * These are the public routines for the various implementations of
 * pdr streams.
 */
extern void pdrmem_create(PDR *pdrs, char * addr, uint64_t size,
                          enum pdr_op op, pdr_byteorder bo);          /* PDR using memory buffers */
extern void pdr_free(pdrproc_t proc, char *objp);

extern bool_t pdr_sizeof(pdrproc_t, void *, uint64_t * size);
extern pdr_byteorder pdrmem_getbo(void);  // Get local machine byte ordering (use for encode)
#ifdef __cplusplus
} /* extern C */
#endif

#ifdef NEED_XDR_COMPAT_MACROS

/****************************/
/* XDR compatibility macros */
/****************************/

#define xdr_op pdr_op
#define XDR_ENCODE PDR_ENCODE
#define XDR_DECODE PDR_DECODE
#define XDR_FREE PDR_FREE

#define XDR PDR
#define x_op p_op

#define xdrproc_t pdrproc_t
#ifndef NULL_xdrproc_t
# define NULL_xdrproc_t NULL_pdrproc_t
#endif

#define xdr_void          pdr_void
#define xdr_char          pdr_char
#define xdr_u_char        pdr_uchar
#define xdr_short         pdr_int16
#define xdr_u_short       pdr_uint16
#define xdr_int           pdr_int32
#define xdr_u_int         pdr_uint32
#define xdr_long          pdr_int64
#define xdr_u_long        pdr_uint64
#define xdr_hyper         pdr_int64
#define xdr_u_hyper       pdr_uint64
#define xdr_longlong_t    pdr_int64
#define xdr_u_longlong_t  pdr_uint64
#define xdr_float         pdr_float
#define xdr_double        pdr_double
#define xdr_bool          pdr_bool
#define xdr_enum          pdr_enum
#define xdr_array         pdr_array
#define xdr_vector        pdr_vector
#define xdr_bytes         pdr_bytes
#define xdr_string        pdr_string
#define xdr_sizeof        pdr_sizeof
#define xdr_opaque        pdr_opaque
#define xdr_reference     pdr_reference
#define xdr_pointer       pdr_pointer
#define xdr_wrapstring    pdr_wrapstring

/*
 * These are the public routines for the various implementations of
 * xdr streams.
 */
#define xdrmem_create pdrmem_create
#define xdr_free pdr_free

#endif /* NEED_XDR_COMPAT_MACROS */

/* Convenience Macros */
#define pdr_getchar(pdrs, charp)                        \
        (*(pdrs)->p_ops->getchar)(pdrs, charp)
#define pdr_putchar(pdrs, charp)                        \
        (*(pdrs)->p_ops->putchar)(pdrs, charp)

#define pdr_getint16(pdrs, int16p)                        \
        (*(pdrs)->p_ops->getint16)(pdrs, int16p)
#define pdr_putint16(pdrs, int16p)                        \
        (*(pdrs)->p_ops->putint16)(pdrs, int16p)

#define pdr_getint32(pdrs, int32p)                        \
        (*(pdrs)->p_ops->getint32)(pdrs, int32p)
#define pdr_putint32(pdrs, int32p)                        \
        (*(pdrs)->p_ops->putint32)(pdrs, int32p)

#define pdr_getint64(pdrs, int64p)                        \
        (*(pdrs)->p_ops->getint64)(pdrs, int64p)
#define pdr_putint64(pdrs, int64p)                        \
        (*(pdrs)->p_ops->putint64)(pdrs, int64p)

#define pdr_getfloat(pdrs, floatp)                        \
        (*(pdrs)->p_ops->getfloat)(pdrs, floatp)
#define pdr_putfloat(pdrs, floatp)                        \
        (*(pdrs)->p_ops->putfloat)(pdrs, floatp)

#define pdr_getdouble(pdrs, doublep)                        \
        (*(pdrs)->p_ops->getdouble)(pdrs, doublep)
#define pdr_putdouble(pdrs, doublep)                        \
        (*(pdrs)->p_ops->putdouble)(pdrs, doublep)

#define pdr_getbytes(pdrs, addr, len)                        \
        (*(pdrs)->p_ops->getbytes)(pdrs, addr, len)
#define pdr_putbytes(pdrs, addr, len)                        \
        (*(pdrs)->p_ops->putbytes)(pdrs, addr, len)

#define pdr_getpos(pdrs)                                \
        (*(pdrs)->p_ops->getpos)(pdrs)
#define pdr_setpos(pdrs, pos)                           \
        (*(pdrs)->p_ops->setpos)(pdrs, pos)

#define pdr_destroy(pdrs)                               \
        (*(pdrs)->p_ops->destroy)(pdrs)

#endif /* __PDR_HEADER__ */
