/*
** $Id: ltm.c $
** Tag methods
** See Copyright Notice in lau.h
*/

#define ltm_c
#define LAU_CORE

#include "lprefix.h"


#include <string.h>

#include "lau.h"

#include "ldebug.h"
#include "ldo.h"
#include "lgc.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lvm.h"


static const char udatatypename[] = "userdata";

LAUI_DDEF const char *const lauT_typenames_[LAU_TOTALTYPES] = {
  "no value",
  "null", "Boolean", udatatypename, "Number",
  "String", "List", "Function", udatatypename, "Thread",
  "upvalue", "proto" /* these last cases are used for tests only */
};


void lauT_init (lau_State *L) {
  static const char *const lauT_eventname[] = {  /* ORDER TM */
    "__index", "__newindex",
    "__gc", "__mode", "__len", "__eq",
    "__add", "__sub", "__mul", "__mod", "__pow",
    "__div",
    "__unm", "__lt", "__le",
    "__concat", "__call", "__close"
  };
  int i;
  for (i=0; i<TM_N; i++) {
    G(L)->tmname[i] = lauS_new(L, lauT_eventname[i]);
    lauC_fix(L, obj2gco(G(L)->tmname[i]));  /* never collect these names */
  }
}


/*
** function to be used with macro "fasttm": optimized for absence of
** tag methods
*/
const TValue *lauT_gettm (Table *events, TMS event, TString *ename) {
  const TValue *tm = lauH_Hgetshortstr(events, ename);
  lau_assert(event <= TM_EQ);
  if (notm(tm)) {  /* no tag method? */
    events->flags |= cast_byte(1u<<event);  /* cache this fact */
    return NULL;
  }
  else return tm;
}


const TValue *lauT_gettmbyobj (lau_State *L, const TValue *o, TMS event) {
  Table *mt;
  switch (ttype(o)) {
    case LAU_TTABLE:
      mt = hvalue(o)->metatable;
      break;
    case LAU_TUSERDATA:
      mt = uvalue(o)->metatable;
      break;
    default:
      mt = G(L)->mt[ttype(o)];
  }
  return (mt ? lauH_Hgetshortstr(mt, G(L)->tmname[event]) : &G(L)->nilvalue);
}


/*
** Return the name of the type of an object. For tables and userdata
** with metatable, use their '__name' metafield, if present.
*/
const char *lauT_objtypename (lau_State *L, const TValue *o) {
  Table *mt;
  if ((ttistable(o) && (mt = hvalue(o)->metatable) != NULL) ||
      (ttisfulluserdata(o) && (mt = uvalue(o)->metatable) != NULL)) {
    const TValue *name = lauH_Hgetshortstr(mt, lauS_new(L, "__name"));
    if (ttisstring(name))  /* is '__name' a string? */
      return getstr(tsvalue(name));  /* use it as type name */
  }
  return ttypename(ttype(o));  /* else use standard type name */
}


void lauT_callTM (lau_State *L, const TValue *f, const TValue *p1,
                  const TValue *p2, const TValue *p3) {
  StkId func = L->top.p;
  setobj2s(L, func, f);  /* push function (assume EXTRA_STACK) */
  setobj2s(L, func + 1, p1);  /* 1st argument */
  setobj2s(L, func + 2, p2);  /* 2nd argument */
  setobj2s(L, func + 3, p3);  /* 3rd argument */
  L->top.p = func + 4;
  /* metamethod may yield only when called from Lau code */
  if (isLaucode(L->ci))
    lauD_call(L, func, 0);
  else
    lauD_callnoyield(L, func, 0);
}


lu_byte lauT_callTMres (lau_State *L, const TValue *f, const TValue *p1,
                        const TValue *p2, StkId res) {
  ptrdiff_t result = savestack(L, res);
  StkId func = L->top.p;
  setobj2s(L, func, f);  /* push function (assume EXTRA_STACK) */
  setobj2s(L, func + 1, p1);  /* 1st argument */
  setobj2s(L, func + 2, p2);  /* 2nd argument */
  L->top.p += 3;
  /* metamethod may yield only when called from Lau code */
  if (isLaucode(L->ci))
    lauD_call(L, func, 1);
  else
    lauD_callnoyield(L, func, 1);
  res = restorestack(L, result);
  setobjs2s(L, res, --L->top.p);  /* move result to its place */
  return ttypetag(s2v(res));  /* return tag of the result */
}


static int callbinTM (lau_State *L, const TValue *p1, const TValue *p2,
                      StkId res, TMS event) {
  const TValue *tm = lauT_gettmbyobj(L, p1, event);  /* try first operand */
  if (notm(tm))
    tm = lauT_gettmbyobj(L, p2, event);  /* try second operand */
  if (notm(tm))
    return -1;  /* tag method not found */
  else  /* call tag method and return the tag of the result */
    return lauT_callTMres(L, tm, p1, p2, res);
}


void lauT_trybinTM (lau_State *L, const TValue *p1, const TValue *p2,
                    StkId res, TMS event) {
  if (l_unlikely(callbinTM(L, p1, p2, res, event) < 0)) {
    switch (event) {
      /* calls never return, but to avoid warnings: *//* FALLTHROUGH */
      default:
        lauG_opinterror(L, p1, p2, "perform arithmetic on");
    }
  }
}


/*
** The use of 'p1' after 'callbinTM' is safe because, when a tag
** method is not found, 'callbinTM' cannot change the stack.
*/
void lauT_tryconcatTM (lau_State *L) {
  StkId p1 = L->top.p - 2;  /* first argument */
  if (l_unlikely(callbinTM(L, s2v(p1), s2v(p1 + 1), p1, TM_CONCAT) < 0))
    lauG_concaterror(L, s2v(p1), s2v(p1 + 1));
}


void lauT_trybinassocTM (lau_State *L, const TValue *p1, const TValue *p2,
                                       int flip, StkId res, TMS event) {
  if (flip)
    lauT_trybinTM(L, p2, p1, res, event);
  else
    lauT_trybinTM(L, p1, p2, res, event);
}


void lauT_trybiniTM (lau_State *L, const TValue *p1, lau_Integer i2,
                                   int flip, StkId res, TMS event) {
  TValue aux;
  setivalue(&aux, i2);
  lauT_trybinassocTM(L, p1, &aux, flip, res, event);
}


/*
** Calls an order tag method.
*/
int lauT_callorderTM (lau_State *L, const TValue *p1, const TValue *p2,
                      TMS event) {
  int tag = callbinTM(L, p1, p2, L->top.p, event);  /* try original event */
  if (tag >= 0)  /* found tag method? */
    return !tagisfalse(tag);
  lauG_ordererror(L, p1, p2);  /* no metamethod found */
  return 0;  /* to avoid warnings */
}


int lauT_callorderiTM (lau_State *L, const TValue *p1, int v2,
                       int flip, int isfloat, TMS event) {
  TValue aux; const TValue *p2;
  if (isfloat) {
    setfltvalue(&aux, cast_num(v2));
  }
  else
    setivalue(&aux, v2);
  if (flip) {  /* arguments were exchanged? */
    p2 = p1; p1 = &aux;  /* correct them */
  }
  else
    p2 = &aux;
  return lauT_callorderTM(L, p1, p2, event);
}


/*
** Create a vararg table at the top of the stack, with 'n' elements
** starting at 'f'.
*/
static void createvarargtab (lau_State *L, StkId f, int n) {
  int i;
  TValue key, value;
  Table *t = lauH_new(L);
  sethvalue(L, s2v(L->top.p), t);
  L->top.p++;
  lauH_resize(L, t, cast_uint(n), 1);
  setsvalue(L, &key, lauS_new(L, "n"));  /* key is "n" */
  setivalue(&value, n);  /* value is n */
  /* No need to anchor the key: Due to the resize, the next operation
     cannot trigger a garbage collection */
  lauH_set(L, t, &key, &value);  /* t.n = n */
  for (i = 0; i < n; i++)
    lauH_setint(L, t, i + 1, s2v(f + i));
  lauC_checkGC(L);
}


/*
** initial stack:  func arg1 ... argn extra1 ...
**                 ^ ci->func                    ^ L->top
** final stack: func nil ... nil extra1 ... func arg1 ... argn
**                                          ^ ci->func
*/
static void buildhiddenargs (lau_State *L, CallInfo *ci, const Proto *p,
                             int totalargs, int nfixparams, int nextra) {
  int i;
  ci->u.l.nextraargs = nextra;
  lauD_checkstack(L, p->maxstacksize + 1);
  /* copy function to the top of the stack, after extra arguments */
  setobjs2s(L, L->top.p++, ci->func.p);
  /* move fixed parameters to after the copied function */
  for (i = 1; i <= nfixparams; i++) {
    setobjs2s(L, L->top.p++, ci->func.p + i);
    setnilvalue2s(ci->func.p + i);  /* erase original parameter (for GC) */
  }
  ci->func.p += totalargs + 1;  /* 'func' now lives after hidden arguments */
  ci->top.p += totalargs + 1;
}


void lauT_adjustvarargs (lau_State *L, CallInfo *ci, const Proto *p) {
  int totalargs = cast_int(L->top.p - ci->func.p) - 1;
  int nfixparams = p->numparams;
  int nextra = totalargs - nfixparams;  /* number of extra arguments */
  if (p->flag & PF_VATAB) {  /* does it need a vararg table? */
    lau_assert(!(p->flag & PF_VAHID));
    createvarargtab(L, ci->func.p + nfixparams + 1, nextra);
    /* move table to proper place (last parameter) */
    setobjs2s(L, ci->func.p + nfixparams + 1, L->top.p - 1);
  }
  else {  /* no table */
    lau_assert(p->flag & PF_VAHID);
    buildhiddenargs(L, ci, p, totalargs, nfixparams, nextra);
    /* set vararg parameter to nil */
    setnilvalue2s(ci->func.p + nfixparams + 1);
    lau_assert(L->top.p <= ci->top.p && ci->top.p <= L->stack_last.p);
  }
}


void lauT_getvararg (CallInfo *ci, StkId ra, TValue *rc) {
  int nextra = ci->u.l.nextraargs;
  lau_Integer n;
  if (tointegerns(rc, &n)) {  /* integral value? */
    if (l_castS2U(n) - 1 < cast_uint(nextra)) {
      StkId slot = ci->func.p - nextra + cast_int(n) - 1;
      setobjs2s(((lau_State*)NULL), ra, slot);
      return;
    }
  }
  else if (ttisstring(rc)) {  /* string value? */
    size_t len;
    const char *s = getlstr(tsvalue(rc), len);
    if (len == 1 && s[0] == 'n') {  /* key is "n"? */
      setivalue(s2v(ra), nextra);
      return;
    }
  }
  setnilvalue2s(ra);  /* else produce nil */
}


/*
** Get the number of extra arguments in a vararg function. If vararg
** table has been optimized away, that number is in the call info.
** Otherwise, get the field 'n' from the vararg table and check that it
** has a proper value (non-negative integer not larger than the stack
** limit).
*/
static int getnumargs (lau_State *L, CallInfo *ci, Table *h) {
  if (h == NULL)  /* no vararg table? */
    return ci->u.l.nextraargs;
  else {
    TValue res;
    if (lauH_getshortstr(h, lauS_new(L, "n"), &res) != LAU_VNUMINT ||
        l_castS2U(ivalue(&res)) > cast_uint(INT_MAX/2))
      lauG_runerror(L, "vararg table has no proper 'n'");
    return cast_int(ivalue(&res));
  }
}


/*
** Get 'wanted' vararg arguments and put them in 'where'. 'vatab' is
** the register of the vararg table or -1 if there is no vararg table.
*/
void lauT_getvarargs (lau_State *L, CallInfo *ci, StkId where, int wanted,
                                    int vatab) {
  Table *h = (vatab < 0) ? NULL : hvalue(s2v(ci->func.p + vatab + 1));
  int nargs = getnumargs(L, ci, h);  /* number of available vararg args. */
  int i, touse;  /* 'touse' is minimum between 'wanted' and 'nargs' */
  if (wanted < 0) {
    touse = wanted = nargs;  /* get all extra arguments available */
    checkstackp(L, nargs, where);  /* ensure stack space */
    L->top.p = where + nargs;  /* next instruction will need top */
  }
  else
    touse = (nargs > wanted) ? wanted : nargs;
  if (h == NULL) {  /* no vararg table? */
    for (i = 0; i < touse; i++)  /* get vararg values from the stack */
      setobjs2s(L, where + i, ci->func.p - nargs + i);
  }
  else {  /* get vararg values from vararg table */
    for (i = 0; i < touse; i++) {
      lu_byte tag = lauH_getint(h, i + 1, s2v(where + i));
      if (tagisempty(tag))
       setnilvalue2s(where + i);
    }
  }
  for (; i < wanted; i++)   /* complete required results with nil */
    setnilvalue2s(where + i);
}

