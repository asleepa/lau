/*
** $Id: lvm.h $
** Lau virtual machine
** See Copyright Notice in lau.h
*/

#ifndef lvm_h
#define lvm_h


#include "ldo.h"
#include "lobject.h"
#include "ltm.h"


#if !defined(LAU_NOCVTN2S)
#define cvt2str(o)	ttisnumber(o)
#else
#define cvt2str(o)	0	/* no conversion from numbers to strings */
#endif


#if !defined(LAU_NOCVTS2N)
#define cvt2num(o)	ttisstring(o)
#else
#define cvt2num(o)	0	/* no conversion from strings to numbers */
#endif


/*
** You can define LAU_FLOORN2I if you want to convert floats to integers
** by flooring them (instead of raising an error if they are not
** integral values)
*/
#if !defined(LAU_FLOORN2I)
#define LAU_FLOORN2I		F2Ieq
#endif


/*
** Rounding modes for float->integer coercion
 */
typedef enum {
  F2Ieq,     /* no rounding; accepts only integral values */
  F2Ifloor,  /* takes the floor of the number */
  F2Iceil    /* takes the ceiling of the number */
} F2Imod;


/* convert an object to a float (including string coercion) */
#define tonumber(o,n) \
	(ttisfloat(o) ? (*(n) = fltvalue(o), 1) : lauV_tonumber_(o,n))


/* convert an object to a float (without string coercion) */
#define tonumberns(o,n) \
	(ttisfloat(o) ? ((n) = fltvalue(o), 1) : \
	(ttisinteger(o) ? ((n) = cast_num(ivalue(o)), 1) : 0))


/* convert an object to an integer (including string coercion) */
#define tointeger(o,i) \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                          : lauV_tointeger(o,i,LAU_FLOORN2I))


/* convert an object to an integer (without string coercion) */
#define tointegerns(o,i) \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                          : lauV_tointegerns(o,i,LAU_FLOORN2I))


#define intop(op,v1,v2) l_castU2S(l_castS2U(v1) op l_castS2U(v2))

#define lauV_rawequalobj(t1,t2)		lauV_equalobj(NULL,t1,t2)


/*
** fast track for 'gettable'
*/
#define lauV_fastget(t,k,res,f, tag) \
  (tag = (!ttistable(t) ? LAU_VNOTABLE : f(hvalue(t), k, res)))


/*
** Special case of 'lauV_fastget' for integers, inlining the fast case
** of 'lauH_getint'.
*/
#define lauV_fastgeti(t,k,res,tag) \
  if (!ttistable(t)) tag = LAU_VNOTABLE; \
  else { lauH_fastgeti(hvalue(t), k, res, tag); }


#define lauV_fastset(t,k,val,hres,f) \
  (hres = (!ttistable(t) ? HNOTATABLE : f(hvalue(t), k, val)))

#define lauV_fastseti(t,k,val,hres) \
  if (!ttistable(t)) hres = HNOTATABLE; \
  else { lauH_fastseti(hvalue(t), k, val, hres); }


/*
** Finish a fast set operation (when fast set succeeds).
*/
#define lauV_finishfastset(L,t,v)	lauC_barrierback(L, gcvalue(t), v)


/*
** Shift right is the same as shift left with a negative 'y'
*/
#define lauV_shiftr(x,y)	lauV_shiftl(x,intop(-, 0, y))



LAUI_FUNC int lauV_equalobj (lau_State *L, const TValue *t1, const TValue *t2);
LAUI_FUNC int lauV_lessthan (lau_State *L, const TValue *l, const TValue *r);
LAUI_FUNC int lauV_lessequal (lau_State *L, const TValue *l, const TValue *r);
LAUI_FUNC int lauV_tonumber_ (const TValue *obj, lau_Number *n);
LAUI_FUNC int lauV_tointeger (const TValue *obj, lau_Integer *p, F2Imod mode);
LAUI_FUNC int lauV_tointegerns (const TValue *obj, lau_Integer *p,
                                F2Imod mode);
LAUI_FUNC int lauV_flttointeger (lau_Number n, lau_Integer *p, F2Imod mode);
LAUI_FUNC lu_byte lauV_finishget (lau_State *L, const TValue *t, TValue *key,
                                                StkId val, lu_byte tag);
LAUI_FUNC void lauV_finishset (lau_State *L, const TValue *t, TValue *key,
                                             TValue *val, int aux);
LAUI_FUNC void lauV_finishOp (lau_State *L);
LAUI_FUNC void lauV_execute (lau_State *L, CallInfo *ci);
LAUI_FUNC void lauV_concat (lau_State *L, int total);
LAUI_FUNC lau_Integer lauV_idiv (lau_State *L, lau_Integer x, lau_Integer y);
LAUI_FUNC lau_Integer lauV_mod (lau_State *L, lau_Integer x, lau_Integer y);
LAUI_FUNC lau_Number lauV_modf (lau_State *L, lau_Number x, lau_Number y);
LAUI_FUNC lau_Integer lauV_shiftl (lau_Integer x, lau_Integer y);
LAUI_FUNC void lauV_objlen (lau_State *L, StkId ra, const TValue *rb);

#endif
