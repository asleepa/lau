/*
** $Id: lau.h $
** Lau - A Scripting Language
** Lua.org, PUC-Rio, Brazil (www.lua.org)
** See Copyright Notice at the end of this file
*/


#ifndef lau_h
#define lau_h

#include <stdarg.h>
#include <stddef.h>


#define LAU_COPYRIGHT	LAU_RELEASE "  Copyright (C) 1994-2026 Lua.org, PUC-Rio\nModified 2026, asleepa"
#define LAU_AUTHORS	"R. Ierusalimschy, L. H. de Figueiredo, W. Celes"


#define LAU_VERSION_MAJOR_N	5
#define LAU_VERSION_MINOR_N	5
#define LAU_VERSION_RELEASE_N	1

#define LAU_VERSION_NUM  (LAU_VERSION_MAJOR_N * 100 + LAU_VERSION_MINOR_N)
#define LAU_VERSION_RELEASE_NUM  (LAU_VERSION_NUM * 100 + LAU_VERSION_RELEASE_N)


#include "lauconf.h"


/* mark for precompiled code ('<esc>Lau') */
#define LAU_SIGNATURE	"\x1bLau"

/* option for multiple returns in 'lau_pcall' and 'lau_call' */
#define LAU_MULTRET	(-1)


/*
** Pseudo-indices
** (The stack size is limited to INT_MAX/2; we keep some free empty
** space after that to help overflow detection.)
*/
#define LAU_REGISTRYINDEX	(-(INT_MAX/2 + 1000))
#define lau_upvalueindex(i)	(LAU_REGISTRYINDEX - (i))


/* thread status */
#define LAU_OK		0
#define LAU_YIELD	1
#define LAU_ERRRUN	2
#define LAU_ERRSYNTAX	3
#define LAU_ERRMEM	4
#define LAU_ERRERR	5


typedef struct lau_State lau_State;


/*
** basic types
*/
#define LAU_TNONE		(-1)

#define LAU_TNIL		0
#define LAU_TBOOLEAN		1
#define LAU_TLIGHTUSERDATA	2
#define LAU_TNUMBER		3
#define LAU_TSTRING		4
#define LAU_TTABLE		5
#define LAU_TFUNCTION		6
#define LAU_TUSERDATA		7
#define LAU_TTHREAD		8

#define LAU_NUMTYPES		9



/* minimum Lau stack available to a C function */
#define LAU_MINSTACK	20


/* predefined values in the registry */
/* index 1 is reserved for the reference mechanism */
#define LAU_RIDX_GLOBALS	2
#define LAU_RIDX_MAINTHREAD	3
#define LAU_RIDX_LAST		3


/* type of numbers in Lau */
typedef LAU_NUMBER lau_Number;


/* type for integer functions */
typedef LAU_INTEGER lau_Integer;

/* unsigned integer type */
typedef LAU_UNSIGNED lau_Unsigned;

/* type for continuation-function contexts */
typedef LAU_KCONTEXT lau_KContext;


/*
** Type for C functions registered with Lau
*/
typedef int (*lau_CFunction) (lau_State *L);

/*
** Type for continuation functions
*/
typedef int (*lau_KFunction) (lau_State *L, int status, lau_KContext ctx);


/*
** Type for functions that read/write blocks when loading/dumping Lau chunks
*/
typedef const char * (*lau_Reader) (lau_State *L, void *ud, size_t *sz);

typedef int (*lau_Writer) (lau_State *L, const void *p, size_t sz, void *ud);


/*
** Type for memory-allocation functions
*/
typedef void * (*lau_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);


/*
** Type for warning functions
*/
typedef void (*lau_WarnFunction) (void *ud, const char *msg, int tocont);


/*
** Type used by the debug API to collect debug information
*/
typedef struct lau_Debug lau_Debug;


/*
** Functions to be called by the debugger in specific events
*/
typedef void (*lau_Hook) (lau_State *L, lau_Debug *ar);


/*
** generic extra include file
*/
#if defined(LAU_USER_H)
#include LAU_USER_H
#endif


/*
** RCS ident string
*/
extern const char lau_ident[];


/*
** state manipulation
*/
LAU_API lau_State *(lau_newstate) (lau_Alloc f, void *ud, unsigned seed);
LAU_API void       (lau_close) (lau_State *L);
LAU_API lau_State *(lau_newthread) (lau_State *L);
LAU_API int        (lau_closethread) (lau_State *L, lau_State *from);

LAU_API lau_CFunction (lau_atpanic) (lau_State *L, lau_CFunction panicf);


LAU_API lau_Number (lau_version) (lau_State *L);


/*
** basic stack manipulation
*/
LAU_API int   (lau_absindex) (lau_State *L, int idx);
LAU_API int   (lau_gettop) (lau_State *L);
LAU_API void  (lau_settop) (lau_State *L, int idx);
LAU_API void  (lau_pushvalue) (lau_State *L, int idx);
LAU_API void  (lau_rotate) (lau_State *L, int idx, int n);
LAU_API void  (lau_copy) (lau_State *L, int fromidx, int toidx);
LAU_API int   (lau_checkstack) (lau_State *L, int n);

LAU_API void  (lau_xmove) (lau_State *from, lau_State *to, int n);


/*
** access functions (stack -> C)
*/

LAU_API int             (lau_isnumber) (lau_State *L, int idx);
LAU_API int             (lau_isstring) (lau_State *L, int idx);
LAU_API int             (lau_iscfunction) (lau_State *L, int idx);
LAU_API int             (lau_isinteger) (lau_State *L, int idx);
LAU_API int             (lau_isuserdata) (lau_State *L, int idx);
LAU_API int             (lau_type) (lau_State *L, int idx);
LAU_API const char     *(lau_typename) (lau_State *L, int tp);

LAU_API lau_Number      (lau_tonumberx) (lau_State *L, int idx, int *isnum);
LAU_API lau_Integer     (lau_tointegerx) (lau_State *L, int idx, int *isnum);
LAU_API int             (lau_toboolean) (lau_State *L, int idx);
LAU_API const char     *(lau_tolstring) (lau_State *L, int idx, size_t *len);
LAU_API lau_Unsigned    (lau_rawlen) (lau_State *L, int idx);
LAU_API lau_CFunction   (lau_tocfunction) (lau_State *L, int idx);
LAU_API void	       *(lau_touserdata) (lau_State *L, int idx);
LAU_API lau_State      *(lau_tothread) (lau_State *L, int idx);
LAU_API const void     *(lau_topointer) (lau_State *L, int idx);


/*
** Comparison and arithmetic functions
*/

#define LAU_OPADD	0	/* ORDER TM, ORDER OP */
#define LAU_OPSUB	1
#define LAU_OPMUL	2
#define LAU_OPMOD	3
#define LAU_OPPOW	4
#define LAU_OPDIV	5
#define LAU_OPIDIV	6
#define LAU_OPBAND	7
#define LAU_OPBOR	8
#define LAU_OPBXOR	9
#define LAU_OPSHL	10
#define LAU_OPSHR	11
#define LAU_OPUNM	12
#define LAU_OPBNOT	13

LAU_API void  (lau_arith) (lau_State *L, int op);

#define LAU_OPEQ	0
#define LAU_OPLT	1
#define LAU_OPLE	2

LAU_API int   (lau_rawequal) (lau_State *L, int idx1, int idx2);
LAU_API int   (lau_compare) (lau_State *L, int idx1, int idx2, int op);


/*
** push functions (C -> stack)
*/
LAU_API void        (lau_pushnil) (lau_State *L);
LAU_API void        (lau_pushnumber) (lau_State *L, lau_Number n);
LAU_API void        (lau_pushinteger) (lau_State *L, lau_Integer n);
LAU_API const char *(lau_pushlstring) (lau_State *L, const char *s, size_t len);
LAU_API const char *(lau_pushexternalstring) (lau_State *L,
		const char *s, size_t len, lau_Alloc falloc, void *ud);
LAU_API const char *(lau_pushstring) (lau_State *L, const char *s);
LAU_API const char *(lau_pushvfstring) (lau_State *L, const char *fmt,
                                                      va_list argp);
LAU_API const char *(lau_pushfstring) (lau_State *L, const char *fmt, ...);
LAU_API void  (lau_pushcclosure) (lau_State *L, lau_CFunction fn, int n);
LAU_API void  (lau_pushboolean) (lau_State *L, int b);
LAU_API void  (lau_pushlightuserdata) (lau_State *L, void *p);
LAU_API int   (lau_pushthread) (lau_State *L);


/*
** get functions (Lau -> stack)
*/
LAU_API int (lau_getglobal) (lau_State *L, const char *name);
LAU_API int (lau_gettable) (lau_State *L, int idx);
LAU_API int (lau_getfield) (lau_State *L, int idx, const char *k);
LAU_API int (lau_geti) (lau_State *L, int idx, lau_Integer n);
LAU_API int (lau_rawget) (lau_State *L, int idx);
LAU_API int (lau_rawgeti) (lau_State *L, int idx, lau_Integer n);
LAU_API int (lau_rawgetp) (lau_State *L, int idx, const void *p);

LAU_API void  (lau_createtable) (lau_State *L, int narr, int nrec);
LAU_API void *(lau_newuserdatauv) (lau_State *L, size_t sz, int nuvalue);
LAU_API int   (lau_getmetatable) (lau_State *L, int objindex);
LAU_API int  (lau_getiuservalue) (lau_State *L, int idx, int n);


/*
** set functions (stack -> Lau)
*/
LAU_API void  (lau_setglobal) (lau_State *L, const char *name);
LAU_API void  (lau_settable) (lau_State *L, int idx);
LAU_API void  (lau_setfield) (lau_State *L, int idx, const char *k);
LAU_API void  (lau_seti) (lau_State *L, int idx, lau_Integer n);
LAU_API void  (lau_rawset) (lau_State *L, int idx);
LAU_API void  (lau_rawseti) (lau_State *L, int idx, lau_Integer n);
LAU_API void  (lau_rawsetp) (lau_State *L, int idx, const void *p);
LAU_API int   (lau_setmetatable) (lau_State *L, int objindex);
LAU_API int   (lau_setiuservalue) (lau_State *L, int idx, int n);


/*
** 'load' and 'call' functions (load and run Lau code)
*/
LAU_API void  (lau_callk) (lau_State *L, int nargs, int nresults,
                           lau_KContext ctx, lau_KFunction k);
#define lau_call(L,n,r)		lau_callk(L, (n), (r), 0, NULL)

LAU_API int   (lau_pcallk) (lau_State *L, int nargs, int nresults, int errfunc,
                            lau_KContext ctx, lau_KFunction k);
#define lau_pcall(L,n,r,f)	lau_pcallk(L, (n), (r), (f), 0, NULL)

LAU_API int   (lau_load) (lau_State *L, lau_Reader reader, void *dt,
                          const char *chunkname, const char *mode);

LAU_API int (lau_dump) (lau_State *L, lau_Writer writer, void *data, int strip);


/*
** coroutine functions
*/
LAU_API int  (lau_yieldk)     (lau_State *L, int nresults, lau_KContext ctx,
                               lau_KFunction k);
LAU_API int  (lau_resume)     (lau_State *L, lau_State *from, int narg,
                               int *nres);
LAU_API int  (lau_status)     (lau_State *L);
LAU_API int (lau_isyieldable) (lau_State *L);

#define lau_yield(L,n)		lau_yieldk(L, (n), 0, NULL)


/*
** Warning-related functions
*/
LAU_API void (lau_setwarnf) (lau_State *L, lau_WarnFunction f, void *ud);
LAU_API void (lau_warning)  (lau_State *L, const char *msg, int tocont);


/*
** garbage-collection options
*/

#define LAU_GCSTOP		0
#define LAU_GCRESTART		1
#define LAU_GCCOLLECT		2
#define LAU_GCCOUNT		3
#define LAU_GCCOUNTB		4
#define LAU_GCSTEP		5
#define LAU_GCISRUNNING		6
#define LAU_GCGEN		7
#define LAU_GCINC		8
#define LAU_GCPARAM		9


/*
** garbage-collection parameters
*/
/* parameters for generational mode */
#define LAU_GCPMINORMUL		0  /* control minor collections */
#define LAU_GCPMAJORMINOR	1  /* control shift major->minor */
#define LAU_GCPMINORMAJOR	2  /* control shift minor->major */

/* parameters for incremental mode */
#define LAU_GCPPAUSE		3  /* size of pause between successive GCs */
#define LAU_GCPSTEPMUL		4  /* GC "speed" */
#define LAU_GCPSTEPSIZE		5  /* GC granularity */

/* number of parameters */
#define LAU_GCPN		6


LAU_API int (lau_gc) (lau_State *L, int what, ...);


/*
** miscellaneous functions
*/

LAU_API int   (lau_error) (lau_State *L);

LAU_API int   (lau_next) (lau_State *L, int idx);

LAU_API void  (lau_concat) (lau_State *L, int n);
LAU_API void  (lau_len)    (lau_State *L, int idx);

#define LAU_N2SBUFFSZ	64
LAU_API unsigned  (lau_numbertocstring) (lau_State *L, int idx, char *buff);
LAU_API size_t  (lau_stringtonumber) (lau_State *L, const char *s);

LAU_API lau_Alloc (lau_getallocf) (lau_State *L, void **ud);
LAU_API void      (lau_setallocf) (lau_State *L, lau_Alloc f, void *ud);

LAU_API void (lau_toclose) (lau_State *L, int idx);
LAU_API void (lau_closeslot) (lau_State *L, int idx);


/*
** {==============================================================
** some useful macros
** ===============================================================
*/

#define lau_getextraspace(L)	((void *)((char *)(L) - LAU_EXTRASPACE))

#define lau_tonumber(L,i)	lau_tonumberx(L,(i),NULL)
#define lau_tointeger(L,i)	lau_tointegerx(L,(i),NULL)

#define lau_pop(L,n)		lau_settop(L, -(n)-1)

#define lau_newtable(L)		lau_createtable(L, 0, 0)

#define lau_register(L,n,f) (lau_pushcfunction(L, (f)), lau_setglobal(L, (n)))

#define lau_pushcfunction(L,f)	lau_pushcclosure(L, (f), 0)

#define lau_isfunction(L,n)	(lau_type(L, (n)) == LAU_TFUNCTION)
#define lau_istable(L,n)	(lau_type(L, (n)) == LAU_TTABLE)
#define lau_islightuserdata(L,n)	(lau_type(L, (n)) == LAU_TLIGHTUSERDATA)
#define lau_isnil(L,n)		(lau_type(L, (n)) == LAU_TNIL)
#define lau_isboolean(L,n)	(lau_type(L, (n)) == LAU_TBOOLEAN)
#define lau_isthread(L,n)	(lau_type(L, (n)) == LAU_TTHREAD)
#define lau_isnone(L,n)		(lau_type(L, (n)) == LAU_TNONE)
#define lau_isnoneornil(L, n)	(lau_type(L, (n)) <= 0)

#define lau_pushliteral(L, s)	lau_pushstring(L, "" s)

#define lau_pushglobaltable(L)  \
	((void)lau_rawgeti(L, LAU_REGISTRYINDEX, LAU_RIDX_GLOBALS))

#define lau_tostring(L,i)	lau_tolstring(L, (i), NULL)


#define lau_insert(L,idx)	lau_rotate(L, (idx), 1)

#define lau_remove(L,idx)	(lau_rotate(L, (idx), -1), lau_pop(L, 1))

#define lau_replace(L,idx)	(lau_copy(L, -1, (idx)), lau_pop(L, 1))

/* }============================================================== */


/*
** {==============================================================
** compatibility macros
** ===============================================================
*/

#define lau_newuserdata(L,s)	lau_newuserdatauv(L,s,1)
#define lau_getuservalue(L,idx)	lau_getiuservalue(L,idx,1)
#define lau_setuservalue(L,idx)	lau_setiuservalue(L,idx,1)

#define lau_resetthread(L)	lau_closethread(L,NULL)

/* }============================================================== */

/*
** {======================================================================
** Debug API
** =======================================================================
*/


/*
** Event codes
*/
#define LAU_HOOKCALL	0
#define LAU_HOOKRET	1
#define LAU_HOOKLINE	2
#define LAU_HOOKCOUNT	3
#define LAU_HOOKTAILCALL 4


/*
** Event masks
*/
#define LAU_MASKCALL	(1 << LAU_HOOKCALL)
#define LAU_MASKRET	(1 << LAU_HOOKRET)
#define LAU_MASKLINE	(1 << LAU_HOOKLINE)
#define LAU_MASKCOUNT	(1 << LAU_HOOKCOUNT)


LAU_API int (lau_getstack) (lau_State *L, int level, lau_Debug *ar);
LAU_API int (lau_getinfo) (lau_State *L, const char *what, lau_Debug *ar);
LAU_API const char *(lau_getlocal) (lau_State *L, const lau_Debug *ar, int n);
LAU_API const char *(lau_setlocal) (lau_State *L, const lau_Debug *ar, int n);
LAU_API const char *(lau_getupvalue) (lau_State *L, int funcindex, int n);
LAU_API const char *(lau_setupvalue) (lau_State *L, int funcindex, int n);

LAU_API void *(lau_upvalueid) (lau_State *L, int fidx, int n);
LAU_API void  (lau_upvaluejoin) (lau_State *L, int fidx1, int n1,
                                               int fidx2, int n2);

LAU_API void (lau_sethook) (lau_State *L, lau_Hook func, int mask, int count);
LAU_API lau_Hook (lau_gethook) (lau_State *L);
LAU_API int (lau_gethookmask) (lau_State *L);
LAU_API int (lau_gethookcount) (lau_State *L);


struct lau_Debug {
  int event;
  const char *name;	/* (n) */
  const char *namewhat;	/* (n) 'global', 'local', 'field', 'method' */
  const char *what;	/* (S) 'Lau', 'C', 'main', 'tail' */
  const char *source;	/* (S) */
  size_t srclen;	/* (S) */
  int currentline;	/* (l) */
  int linedefined;	/* (S) */
  int lastlinedefined;	/* (S) */
  unsigned char nups;	/* (u) number of upvalues */
  unsigned char nparams;/* (u) number of parameters */
  char isvararg;        /* (u) */
  unsigned char extraargs;  /* (t) number of extra arguments */
  char istailcall;	/* (t) */
  int ftransfer;   /* (r) index of first value transferred */
  int ntransfer;   /* (r) number of transferred values */
  char short_src[LAU_IDSIZE]; /* (S) */
  /* private part */
  struct CallInfo *i_ci;  /* active function */
};

/* }====================================================================== */


#define LAUI_TOSTRAUX(x)	#x
#define LAUI_TOSTR(x)		LAUI_TOSTRAUX(x)

#define LAU_VERSION_MAJOR	LAUI_TOSTR(LAU_VERSION_MAJOR_N)
#define LAU_VERSION_MINOR	LAUI_TOSTR(LAU_VERSION_MINOR_N)
#define LAU_VERSION_RELEASE	LAUI_TOSTR(LAU_VERSION_RELEASE_N)

#define LAU_VERSION	"Lau " LAU_VERSION_MAJOR "." LAU_VERSION_MINOR
#define LAU_RELEASE	LAU_VERSION "." LAU_VERSION_RELEASE


/******************************************************************************
* Copyright (C) 1994-2026 Lua.org, PUC-Rio.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/


#endif
