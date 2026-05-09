/*
** $Id: ldo.h $
** Stack and Call structure of Lau
** See Copyright Notice in lau.h
*/

#ifndef ldo_h
#define ldo_h


#include "llimits.h"
#include "lobject.h"
#include "lstate.h"
#include "lzio.h"


/*
** Macro to check stack size and grow stack if needed.  Parameters
** 'pre'/'pos' allow the macro to preserve a pointer into the
** stack across reallocations, doing the work only when needed.
** It also allows the running of one GC step when the stack is
** reallocated.
** 'condmovestack' is used in heavy tests to force a stack reallocation
** at every check.
*/

#if !defined(HARDSTACKTESTS)
#define condmovestack(L,pre,pos)	((void)0)
#else
/* realloc stack keeping its size */
#define condmovestack(L,pre,pos)  \
  { int sz_ = stacksize(L); pre; lauD_reallocstack((L), sz_, 0); pos; }
#endif

#define lauD_checkstackaux(L,n,pre,pos)  \
	if (l_unlikely(L->stack_last.p - L->top.p <= (n))) \
	  { pre; lauD_growstack(L, n, 1); pos; } \
	else { condmovestack(L,pre,pos); }

/* In general, 'pre'/'pos' are empty (nothing to save) */
#define lauD_checkstack(L,n)	lauD_checkstackaux(L,n,(void)0,(void)0)



#define savestack(L,pt)		(cast_charp(pt) - cast_charp(L->stack.p))
#define restorestack(L,n)	cast(StkId, cast_charp(L->stack.p) + (n))


/* macro to check stack size, preserving 'p' */
#define checkstackp(L,n,p)  \
  lauD_checkstackaux(L, n, \
    ptrdiff_t t__ = savestack(L, p),  /* save 'p' */ \
    p = restorestack(L, t__))  /* 'pos' part: restore 'p' */


/*
** Maximum depth for nested C calls, syntactical nested non-terminals,
** and other features implemented through recursion in C. (Value must
** fit in a 16-bit unsigned integer. It must also be compatible with
** the size of the C stack.)
*/
#if !defined(LAUI_MAXCCALLS)
#define LAUI_MAXCCALLS		200
#endif


/* type of protected functions, to be ran by 'runprotected' */
typedef void (*Pfunc) (lau_State *L, void *ud);

LAUI_FUNC l_noret lauD_errerr (lau_State *L);
LAUI_FUNC void lauD_seterrorobj (lau_State *L, TStatus errcode, StkId oldtop);
LAUI_FUNC TStatus lauD_protectedparser (lau_State *L, ZIO *z,
                                                  const char *name,
                                                  const char *mode);
LAUI_FUNC void lauD_hook (lau_State *L, int event, int line,
                                        int fTransfer, int nTransfer);
LAUI_FUNC void lauD_hookcall (lau_State *L, CallInfo *ci);
LAUI_FUNC int lauD_pretailcall (lau_State *L, CallInfo *ci, StkId func,
                                              int narg1, int delta);
LAUI_FUNC CallInfo *lauD_precall (lau_State *L, StkId func, int nResults);
LAUI_FUNC void lauD_call (lau_State *L, StkId func, int nResults);
LAUI_FUNC void lauD_callnoyield (lau_State *L, StkId func, int nResults);
LAUI_FUNC TStatus lauD_closeprotected (lau_State *L, ptrdiff_t level,
                                                     TStatus status);
LAUI_FUNC TStatus lauD_pcall (lau_State *L, Pfunc func, void *u,
                                        ptrdiff_t oldtop, ptrdiff_t ef);
LAUI_FUNC void lauD_poscall (lau_State *L, CallInfo *ci, int nres);
LAUI_FUNC int lauD_reallocstack (lau_State *L, int newsize, int raiseerror);
LAUI_FUNC int lauD_growstack (lau_State *L, int n, int raiseerror);
LAUI_FUNC void lauD_shrinkstack (lau_State *L);
LAUI_FUNC void lauD_inctop (lau_State *L);
LAUI_FUNC int lauD_checkminstack (lau_State *L);
LAUI_FUNC void lauD_anchorobj (lau_State *L, Table *anchor, GCObject *obj);

LAUI_FUNC l_noret lauD_throw (lau_State *L, TStatus errcode);
LAUI_FUNC l_noret lauD_throwbaselevel (lau_State *L, TStatus errcode);
LAUI_FUNC TStatus lauD_rawrunprotected (lau_State *L, Pfunc f, void *ud);

#endif

