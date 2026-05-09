/*
** $Id: lmem.h $
** Interface to Memory Manager
** See Copyright Notice in lau.h
*/

#ifndef lmem_h
#define lmem_h


#include <stddef.h>

#include "llimits.h"
#include "lau.h"


#define lauM_error(L)	lauD_throw(L, LAU_ERRMEM)


/*
** This macro tests whether it is safe to multiply 'n' by the size of
** type 't' without overflows. Because 'e' is always constant, it avoids
** the runtime division MAX_SIZET/(e).
** (The macro is somewhat complex to avoid warnings:  The 'sizeof'
** comparison avoids a runtime comparison when overflow cannot occur.
** The compiler should be able to optimize the real test by itself, but
** when it does it, it may give a warning about "comparison is always
** false due to limited range of data type"; the +1 tricks the compiler,
** avoiding this warning but also this optimization.)
*/
#define lauM_testsize(n,e)  \
	(sizeof(n) >= sizeof(size_t) && cast_sizet((n)) + 1 > MAX_SIZET/(e))

#define lauM_checksize(L,n,e)  \
	(lauM_testsize(n,e) ? lauM_toobig(L) : cast_void(0))


/*
** Computes the minimum between 'n' and 'MAX_SIZET/sizeof(t)', so that
** the result is not larger than 'n' and cannot overflow a 'size_t'
** when multiplied by the size of type 't'. (Assumes that 'n' is an
** 'int' and that 'int' is not larger than 'size_t'.)
*/
#define lauM_limitN(n,t)  \
  ((cast_sizet(n) <= MAX_SIZET/sizeof(t)) ? (n) :  \
     cast_int((MAX_SIZET/sizeof(t))))


/*
** Arrays of chars do not need any test
*/
#define lauM_reallocvchar(L,b,on,n)  \
  cast_charp(lauM_saferealloc_(L, (b), (on)*sizeof(char), (n)*sizeof(char)))

#define lauM_freemem(L, b, s)	lauM_free_(L, (b), (s))
#define lauM_free(L, b)		lauM_free_(L, (b), sizeof(*(b)))
#define lauM_freearray(L, b, n)   lauM_free_(L, (b), (n)*sizeof(*(b)))

#define lauM_new(L,t)		cast(t*, lauM_malloc_(L, sizeof(t), 0))
#define lauM_newvector(L,n,t)  \
	cast(t*, lauM_malloc_(L, cast_sizet(n)*sizeof(t), 0))
#define lauM_newvectorchecked(L,n,t) \
  (lauM_checksize(L,n,sizeof(t)), lauM_newvector(L,n,t))

#define lauM_newobject(L,tag,s)	lauM_malloc_(L, (s), tag)

#define lauM_newblock(L, size)	lauM_newvector(L, size, char)

#define lauM_growvector(L,v,nelems,size,t,limit,e) \
	((v)=cast(t *, lauM_growaux_(L,v,nelems,&(size),sizeof(t), \
                         lauM_limitN(limit,t),e)))

#define lauM_reallocvector(L, v,oldn,n,t) \
   (cast(t *, lauM_realloc_(L, v, cast_sizet(oldn) * sizeof(t), \
                                  cast_sizet(n) * sizeof(t))))

#define lauM_shrinkvector(L,v,size,fs,t) \
   ((v)=cast(t *, lauM_shrinkvector_(L, v, &(size), fs, sizeof(t))))

LAUI_FUNC l_noret lauM_toobig (lau_State *L);

/* not to be called directly */
LAUI_FUNC void *lauM_realloc_ (lau_State *L, void *block, size_t oldsize,
                                                          size_t size);
LAUI_FUNC void *lauM_saferealloc_ (lau_State *L, void *block, size_t oldsize,
                                                              size_t size);
LAUI_FUNC void lauM_free_ (lau_State *L, void *block, size_t osize);
LAUI_FUNC void *lauM_growaux_ (lau_State *L, void *block, int nelems,
                               int *size, unsigned size_elem, int limit,
                               const char *what);
LAUI_FUNC void *lauM_shrinkvector_ (lau_State *L, void *block, int *nelem,
                                    int final_n, unsigned size_elem);
LAUI_FUNC void *lauM_malloc_ (lau_State *L, size_t size, int tag);

#endif

