/*
** $Id: lfunc.h $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in lau.h
*/

#ifndef lfunc_h
#define lfunc_h


#include "lobject.h"


#define sizeCclosure(n)  \
	(offsetof(CClosure, upvalue) + sizeof(TValue) * cast_uint(n))

#define sizeLclosure(n)  \
	(offsetof(LClosure, upvals) + sizeof(UpVal *) * cast_uint(n))


/* test whether thread is in 'twups' list */
#define isintwups(L)	(L->twups != L)


/*
** maximum number of upvalues in a closure (both C and Lau). (Value
** must fit in a VM register.)
*/
#define MAXUPVAL	255


#define upisopen(up)	((up)->v.p != &(up)->u.value)


#define uplevel(up)	check_exp(upisopen(up), cast(StkId, (up)->v.p))


/*
** maximum number of misses before giving up the cache of closures
** in prototypes
*/
#define MAXMISS		10



/* special status to close upvalues preserving the top of the stack */
#define CLOSEKTOP	(LAU_ERRERR + 1)


LAUI_FUNC Proto *lauF_newproto (lau_State *L);
LAUI_FUNC CClosure *lauF_newCclosure (lau_State *L, int nupvals);
LAUI_FUNC LClosure *lauF_newLclosure (lau_State *L, int nupvals);
LAUI_FUNC void lauF_initupvals (lau_State *L, LClosure *cl);
LAUI_FUNC UpVal *lauF_findupval (lau_State *L, StkId level);
LAUI_FUNC void lauF_newtbcupval (lau_State *L, StkId level);
LAUI_FUNC void lauF_closeupval (lau_State *L, StkId level);
LAUI_FUNC StkId lauF_close (lau_State *L, StkId level, TStatus status, int yy);
LAUI_FUNC void lauF_unlinkupval (UpVal *uv);
LAUI_FUNC lu_mem lauF_protosize (Proto *p);
LAUI_FUNC void lauF_freeproto (lau_State *L, Proto *f);
LAUI_FUNC const char *lauF_getlocalname (const Proto *func, int local_number,
                                         int pc);


#endif
