/*
** $Id: lauxlib.h $
** Auxiliary functions for building Lau libraries
** See Copyright Notice in lau.h
*/


#ifndef lauxlib_h
#define lauxlib_h


#include <stddef.h>
#include <stdio.h>

#include "lauconf.h"
#include "lau.h"


/* global table */
#define LAU_GNAME	"_G"


typedef struct lauL_Buffer lauL_Buffer;


/* extra error code for 'lauL_loadfilex' */
#define LAU_ERRFILE     (LAU_ERRERR+1)


/* key, in the registry, for table of loaded modules */
#define LAU_LOADED_TABLE	"_LOADED"


/* key, in the registry, for table of preloaded loaders */
#define LAU_PRELOAD_TABLE	"_PRELOAD"


typedef struct lauL_Reg {
  const char *name;
  lau_CFunction func;
} lauL_Reg;


#define LAUL_NUMSIZES	(sizeof(lau_Integer)*16 + sizeof(lau_Number))

LAULIB_API void (lauL_checkversion_) (lau_State *L, lau_Number ver, size_t sz);
#define lauL_checkversion(L)  \
	  lauL_checkversion_(L, LAU_VERSION_NUM, LAUL_NUMSIZES)

LAULIB_API int (lauL_getmetafield) (lau_State *L, int obj, const char *e);
LAULIB_API int (lauL_callmeta) (lau_State *L, int obj, const char *e);
LAULIB_API const char *(lauL_tolstring) (lau_State *L, int idx, size_t *len);
LAULIB_API int (lauL_argerror) (lau_State *L, int arg, const char *extramsg);
LAULIB_API int (lauL_typeerror) (lau_State *L, int arg, const char *tname);
LAULIB_API const char *(lauL_checklstring) (lau_State *L, int arg,
                                                          size_t *l);
LAULIB_API const char *(lauL_optlstring) (lau_State *L, int arg,
                                          const char *def, size_t *l);
LAULIB_API lau_Number (lauL_checknumber) (lau_State *L, int arg);
LAULIB_API lau_Number (lauL_optnumber) (lau_State *L, int arg, lau_Number def);

LAULIB_API lau_Integer (lauL_checkinteger) (lau_State *L, int arg);
LAULIB_API lau_Integer (lauL_optinteger) (lau_State *L, int arg,
                                          lau_Integer def);

LAULIB_API void (lauL_checkstack) (lau_State *L, int sz, const char *msg);
LAULIB_API void (lauL_checktype) (lau_State *L, int arg, int t);
LAULIB_API void (lauL_checkany) (lau_State *L, int arg);

LAULIB_API int   (lauL_newmetatable) (lau_State *L, const char *tname);
LAULIB_API void  (lauL_setmetatable) (lau_State *L, const char *tname);
LAULIB_API void *(lauL_testudata) (lau_State *L, int ud, const char *tname);
LAULIB_API void *(lauL_checkudata) (lau_State *L, int ud, const char *tname);

LAULIB_API void (lauL_where) (lau_State *L, int lvl);
LAULIB_API int (lauL_error) (lau_State *L, const char *fmt, ...);

LAULIB_API int (lauL_checkoption) (lau_State *L, int arg, const char *def,
                                   const char *const lst[]);

LAULIB_API int (lauL_fileresult) (lau_State *L, int stat, const char *fname);
LAULIB_API int (lauL_execresult) (lau_State *L, int stat);

LAULIB_API void *(lauL_alloc) (void *ud, void *ptr, size_t osize,
                                                    size_t nsize);


/* predefined references */
#define LAU_NOREF       (-2)
#define LAU_REFNIL      (-1)

LAULIB_API int (lauL_ref) (lau_State *L, int t);
LAULIB_API void (lauL_unref) (lau_State *L, int t, int ref);

LAULIB_API int (lauL_loadfilex) (lau_State *L, const char *filename,
                                               const char *mode);

#define lauL_loadfile(L,f)	lauL_loadfilex(L,f,NULL)

LAULIB_API int (lauL_loadbufferx) (lau_State *L, const char *buff, size_t sz,
                                   const char *name, const char *mode);
LAULIB_API int (lauL_loadstring) (lau_State *L, const char *s);

LAULIB_API lau_State *(lauL_newstate) (void);

LAULIB_API unsigned (lauL_makeseed) (lau_State *L);

LAULIB_API lau_Integer (lauL_len) (lau_State *L, int idx);

LAULIB_API void (lauL_addgsub) (lauL_Buffer *b, const char *s,
                                     const char *p, const char *r);
LAULIB_API const char *(lauL_gsub) (lau_State *L, const char *s,
                                    const char *p, const char *r);

LAULIB_API void (lauL_setfuncs) (lau_State *L, const lauL_Reg *l, int nup);

LAULIB_API int (lauL_getsubtable) (lau_State *L, int idx, const char *fname);

LAULIB_API void (lauL_traceback) (lau_State *L, lau_State *L1,
                                  const char *msg, int level);

LAULIB_API void (lauL_requiref) (lau_State *L, const char *modname,
                                 lau_CFunction openf, int glb);

/*
** ===============================================================
** some useful macros
** ===============================================================
*/


#define lauL_newlibtable(L,l)	\
  lau_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define lauL_newlib(L,l)  \
  (lauL_checkversion(L), lauL_newlibtable(L,l), lauL_setfuncs(L,l,0))

#define lauL_argcheck(L, cond,arg,extramsg)	\
	((void)(laui_likely(cond) || lauL_argerror(L, (arg), (extramsg))))

#define lauL_argexpected(L,cond,arg,tname)	\
	((void)(laui_likely(cond) || lauL_typeerror(L, (arg), (tname))))

#define lauL_checkstring(L,n)	(lauL_checklstring(L, (n), NULL))
#define lauL_optstring(L,n,d)	(lauL_optlstring(L, (n), (d), NULL))

#define lauL_typename(L,i)	lau_typename(L, lau_type(L,(i)))

#define lauL_dofile(L, fn) \
	(lauL_loadfile(L, fn) || lau_pcall(L, 0, LAU_MULTRET, 0))

#define lauL_dostring(L, s) \
	(lauL_loadstring(L, s) || lau_pcall(L, 0, LAU_MULTRET, 0))

#define lauL_getmetatable(L,n)	(lau_getfield(L, LAU_REGISTRYINDEX, (n)))

#define lauL_opt(L,f,n,d)	(lau_isnoneornil(L,(n)) ? (d) : f(L,(n)))

#define lauL_loadbuffer(L,s,sz,n)	lauL_loadbufferx(L,s,sz,n,NULL)


/*
** Perform arithmetic operations on lau_Integer values with wrap-around
** semantics, as the Lau core does.
*/
#define lauL_intop(op,v1,v2)  \
	((lau_Integer)((lau_Unsigned)(v1) op (lau_Unsigned)(v2)))


/* push the value used to represent failure/error */
#if defined(LAU_FAILISFALSE)
#define lauL_pushfail(L)	lau_pushboolean(L, 0)
#else
#define lauL_pushfail(L)	lau_pushnil(L)
#endif



/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

struct lauL_Buffer {
  char *b;  /* buffer address */
  size_t size;  /* buffer size */
  size_t n;  /* number of characters in buffer */
  lau_State *L;
  union {
    LAUI_MAXALIGN;  /* ensure maximum alignment for buffer */
    char b[LAUL_BUFFERSIZE];  /* initial buffer */
  } init;
};


#define lauL_bufflen(bf)	((bf)->n)
#define lauL_buffaddr(bf)	((bf)->b)


#define lauL_addchar(B,c) \
  ((void)((B)->n < (B)->size || lauL_prepbuffsize((B), 1)), \
   ((B)->b[(B)->n++] = (c)))

#define lauL_addsize(B,s)	((B)->n += (s))

#define lauL_buffsub(B,s)	((B)->n -= (s))

LAULIB_API void (lauL_buffinit) (lau_State *L, lauL_Buffer *B);
LAULIB_API char *(lauL_prepbuffsize) (lauL_Buffer *B, size_t sz);
LAULIB_API void (lauL_addlstring) (lauL_Buffer *B, const char *s, size_t l);
LAULIB_API void (lauL_addstring) (lauL_Buffer *B, const char *s);
LAULIB_API void (lauL_addvalue) (lauL_Buffer *B);
LAULIB_API void (lauL_pushresult) (lauL_Buffer *B);
LAULIB_API void (lauL_pushresultsize) (lauL_Buffer *B, size_t sz);
LAULIB_API char *(lauL_buffinitsize) (lau_State *L, lauL_Buffer *B, size_t sz);

#define lauL_prepbuffer(B)	lauL_prepbuffsize(B, LAUL_BUFFERSIZE)

/* }====================================================== */



/*
** {======================================================
** File handles for IO library
** =======================================================
*/

/*
** A file handle is a userdata with metatable 'LAU_FILEHANDLE' and
** initial structure 'lauL_Stream' (it may contain other fields
** after that initial structure).
*/

#define LAU_FILEHANDLE          "FILE*"


typedef struct lauL_Stream {
  FILE *f;  /* stream (NULL for incompletely created streams) */
  lau_CFunction closef;  /* to close stream (NULL for closed streams) */
} lauL_Stream;

/* }====================================================== */


/*
** {============================================================
** Compatibility with deprecated conversions
** =============================================================
*/
#if defined(LAU_COMPAT_APIINTCASTS)

#define lauL_checkunsigned(L,a)	((lau_Unsigned)lauL_checkinteger(L,a))
#define lauL_optunsigned(L,a,d)	\
	((lau_Unsigned)lauL_optinteger(L,a,(lau_Integer)(d)))

#define lauL_checkint(L,n)	((int)lauL_checkinteger(L, (n)))
#define lauL_optint(L,n,d)	((int)lauL_optinteger(L, (n), (d)))

#define lauL_checklong(L,n)	((long)lauL_checkinteger(L, (n)))
#define lauL_optlong(L,n,d)	((long)lauL_optinteger(L, (n), (d)))

#endif
/* }============================================================ */



#endif


