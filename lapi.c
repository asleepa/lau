/*
** $Id: lapi.c $
** Lau API
** See Copyright Notice in lau.h
*/

#define lapi_c
#define LAU_CORE

#include "lprefix.h"


#include <limits.h>
#include <stdarg.h>
#include <string.h>

#include "lau.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lundump.h"
#include "lvm.h"



const char lau_ident[] =
  "$LauVersion: " LAU_COPYRIGHT " $"
  "$LauAuthors: " LAU_AUTHORS " $";



/*
** Test for a valid index (one that is not the 'nilvalue').
*/
#define isvalid(L, o)	((o) != &G(L)->nilvalue)


/* test for pseudo index */
#define ispseudo(i)		((i) <= LAU_REGISTRYINDEX)

/* test for upvalue */
#define isupvalue(i)		((i) < LAU_REGISTRYINDEX)


/*
** Convert an acceptable index to a pointer to its respective value.
** Non-valid indices return the special nil value 'G(L)->nilvalue'.
*/
static TValue *index2value (lau_State *L, int idx) {
  CallInfo *ci = L->ci;
  if (idx > 0) {
    StkId o = ci->func.p + idx;
    api_check(L, idx <= ci->top.p - (ci->func.p + 1), "unacceptable index");
    if (o >= L->top.p) return &G(L)->nilvalue;
    else return s2v(o);
  }
  else if (!ispseudo(idx)) {  /* negative index */
    api_check(L, idx != 0 && -idx <= L->top.p - (ci->func.p + 1),
                 "invalid index");
    return s2v(L->top.p + idx);
  }
  else if (idx == LAU_REGISTRYINDEX)
    return &G(L)->l_registry;
  else {  /* upvalues */
    idx = LAU_REGISTRYINDEX - idx;
    api_check(L, idx <= MAXUPVAL + 1, "upvalue index too large");
    if (ttisCclosure(s2v(ci->func.p))) {  /* C closure? */
      CClosure *func = clCvalue(s2v(ci->func.p));
      return (idx <= func->nupvalues) ? &func->upvalue[idx-1]
                                      : &G(L)->nilvalue;
    }
    else {  /* light C function or Lau function (through a hook)?) */
      api_check(L, ttislcf(s2v(ci->func.p)), "caller not a C function");
      return &G(L)->nilvalue;  /* no upvalues */
    }
  }
}



/*
** Convert a valid actual index (not a pseudo-index) to its address.
*/
static StkId index2stack (lau_State *L, int idx) {
  CallInfo *ci = L->ci;
  if (idx > 0) {
    StkId o = ci->func.p + idx;
    api_check(L, o < L->top.p, "invalid index");
    return o;
  }
  else {    /* non-positive index */
    api_check(L, idx != 0 && -idx <= L->top.p - (ci->func.p + 1),
                 "invalid index");
    api_check(L, !ispseudo(idx), "invalid index");
    return L->top.p + idx;
  }
}


LAU_API int lau_checkstack (lau_State *L, int n) {
  int res;
  CallInfo *ci;
  lau_lock(L);
  ci = L->ci;
  api_check(L, n >= 0, "negative 'n'");
  if (L->stack_last.p - L->top.p > n)  /* stack large enough? */
    res = 1;  /* yes; check is OK */
  else  /* need to grow stack */
    res = lauD_growstack(L, n, 0);
  if (res && ci->top.p < L->top.p + n)
    ci->top.p = L->top.p + n;  /* adjust frame top */
  lau_unlock(L);
  return res;
}


LAU_API void lau_xmove (lau_State *from, lau_State *to, int n) {
  int i;
  if (from == to) return;
  lau_lock(to);
  api_checkpop(from, n);
  api_check(from, G(from) == G(to), "moving among independent states");
  api_check(from, to->ci->top.p - to->top.p >= n, "stack overflow");
  from->top.p -= n;
  for (i = 0; i < n; i++) {
    setobjs2s(to, to->top.p, from->top.p + i);
    to->top.p++;  /* stack already checked by previous 'api_check' */
  }
  lau_unlock(to);
}


LAU_API lau_CFunction lau_atpanic (lau_State *L, lau_CFunction panicf) {
  lau_CFunction old;
  lau_lock(L);
  old = G(L)->panic;
  G(L)->panic = panicf;
  lau_unlock(L);
  return old;
}


LAU_API lau_Number lau_version (lau_State *L) {
  UNUSED(L);
  return LAU_VERSION_NUM;
}



/*
** basic stack manipulation
*/


/*
** convert an acceptable stack index into an absolute index
*/
LAU_API int lau_absindex (lau_State *L, int idx) {
  return (idx > 0 || ispseudo(idx))
         ? idx
         : cast_int(L->top.p - L->ci->func.p) + idx;
}


LAU_API int lau_gettop (lau_State *L) {
  return cast_int(L->top.p - (L->ci->func.p + 1));
}


LAU_API void lau_settop (lau_State *L, int idx) {
  CallInfo *ci;
  StkId func, newtop;
  ptrdiff_t diff;  /* difference for new top */
  lau_lock(L);
  ci = L->ci;
  func = ci->func.p;
  if (idx >= 0) {
    api_check(L, idx <= ci->top.p - (func + 1), "new top too large");
    diff = ((func + 1) + idx) - L->top.p;
    for (; diff > 0; diff--)
      setnilvalue2s(L->top.p++);  /* clear new slots */
  }
  else {
    api_check(L, -(idx+1) <= (L->top.p - (func + 1)), "invalid new top");
    diff = idx + 1;  /* will "subtract" index (as it is negative) */
  }
  newtop = L->top.p + diff;
  if (diff < 0 && L->tbclist.p >= newtop) {
    lau_assert(ci->callstatus & CIST_TBC);
    newtop = lauF_close(L, newtop, CLOSEKTOP, 0);
  }
  L->top.p = newtop;  /* correct top only after closing any upvalue */
  lau_unlock(L);
}


LAU_API void lau_closeslot (lau_State *L, int idx) {
  StkId level;
  lau_lock(L);
  level = index2stack(L, idx);
  api_check(L, (L->ci->callstatus & CIST_TBC) && (L->tbclist.p == level),
     "no variable to close at given level");
  level = lauF_close(L, level, CLOSEKTOP, 0);
  setnilvalue2s(level);
  lau_unlock(L);
}


/*
** Reverse the stack segment from 'from' to 'to'
** (auxiliary to 'lau_rotate')
** Note that we move(copy) only the value inside the stack.
** (We do not move additional fields that may exist.)
*/
static void reverse (lau_State *L, StkId from, StkId to) {
  for (; from < to; from++, to--) {
    TValue temp;
    setobj(L, &temp, s2v(from));
    setobjs2s(L, from, to);
    setobj2s(L, to, &temp);
  }
}


/*
** Let x = AB, where A is a prefix of length 'n'. Then,
** rotate x n == BA. But BA == (A^r . B^r)^r.
*/
LAU_API void lau_rotate (lau_State *L, int idx, int n) {
  StkId p, t, m;
  lau_lock(L);
  t = L->top.p - 1;  /* end of stack segment being rotated */
  p = index2stack(L, idx);  /* start of segment */
  api_check(L, L->tbclist.p < p, "moving a to-be-closed slot");
  api_check(L, (n >= 0 ? n : -n) <= (t - p + 1), "invalid 'n'");
  m = (n >= 0 ? t - n : p - n - 1);  /* end of prefix */
  reverse(L, p, m);  /* reverse the prefix with length 'n' */
  reverse(L, m + 1, t);  /* reverse the suffix */
  reverse(L, p, t);  /* reverse the entire segment */
  lau_unlock(L);
}


LAU_API void lau_copy (lau_State *L, int fromidx, int toidx) {
  TValue *fr, *to;
  lau_lock(L);
  fr = index2value(L, fromidx);
  to = index2value(L, toidx);
  api_check(L, isvalid(L, to), "invalid index");
  setobj(L, to, fr);
  if (isupvalue(toidx))  /* function upvalue? */
    lauC_barrier(L, clCvalue(s2v(L->ci->func.p)), fr);
  /* LAU_REGISTRYINDEX does not need gc barrier
     (collector revisits it before finishing collection) */
  lau_unlock(L);
}


LAU_API void lau_pushvalue (lau_State *L, int idx) {
  lau_lock(L);
  setobj2s(L, L->top.p, index2value(L, idx));
  api_incr_top(L);
  lau_unlock(L);
}



/*
** access functions (stack -> C)
*/


LAU_API int lau_type (lau_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (isvalid(L, o) ? ttype(o) : LAU_TNONE);
}


LAU_API const char *lau_typename (lau_State *L, int t) {
  UNUSED(L);
  api_check(L, LAU_TNONE <= t && t < LAU_NUMTYPES, "invalid type");
  return ttypename(t);
}


LAU_API int lau_iscfunction (lau_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttislcf(o) || (ttisCclosure(o)));
}


LAU_API int lau_isinteger (lau_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return ttisinteger(o);
}


LAU_API int lau_isnumber (lau_State *L, int idx) {
  lau_Number n;
  const TValue *o = index2value(L, idx);
  return tonumber(o, &n);
}


LAU_API int lau_isstring (lau_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisstring(o) || cvt2str(o));
}


LAU_API int lau_isuserdata (lau_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisfulluserdata(o) || ttislightuserdata(o));
}


LAU_API int lau_rawequal (lau_State *L, int index1, int index2) {
  const TValue *o1 = index2value(L, index1);
  const TValue *o2 = index2value(L, index2);
  return (isvalid(L, o1) && isvalid(L, o2)) ? lauV_rawequalobj(o1, o2) : 0;
}


LAU_API void lau_arith (lau_State *L, int op) {
  lau_lock(L);
  if (op != LAU_OPUNM && op != LAU_OPBNOT)
    api_checkpop(L, 2);  /* all other operations expect two operands */
  else {  /* for unary operations, add fake 2nd operand */
    api_checkpop(L, 1);
    setobjs2s(L, L->top.p, L->top.p - 1);
    api_incr_top(L);
  }
  /* first operand at top - 2, second at top - 1; result go to top - 2 */
  lauO_arith(L, op, s2v(L->top.p - 2), s2v(L->top.p - 1), L->top.p - 2);
  L->top.p--;  /* pop second operand */
  lau_unlock(L);
}


LAU_API int lau_compare (lau_State *L, int index1, int index2, int op) {
  const TValue *o1;
  const TValue *o2;
  int i = 0;
  lau_lock(L);  /* may call tag method */
  o1 = index2value(L, index1);
  o2 = index2value(L, index2);
  if (isvalid(L, o1) && isvalid(L, o2)) {
    switch (op) {
      case LAU_OPEQ: i = lauV_equalobj(L, o1, o2); break;
      case LAU_OPLT: i = lauV_lessthan(L, o1, o2); break;
      case LAU_OPLE: i = lauV_lessequal(L, o1, o2); break;
      default: api_check(L, 0, "invalid option");
    }
  }
  lau_unlock(L);
  return i;
}


LAU_API unsigned lau_numbertocstring (lau_State *L, int idx, char *buff) {
  const TValue *o = index2value(L, idx);
  if (ttisnumber(o)) {
    unsigned len = lauO_tostringbuff(o, buff);
    buff[len++] = '\0';  /* add final zero */
    return len;
  }
  else
    return 0;
}


LAU_API size_t lau_stringtonumber (lau_State *L, const char *s) {
  size_t sz = lauO_str2num(s, s2v(L->top.p));
  if (sz != 0)
    api_incr_top(L);
  return sz;
}


LAU_API lau_Number lau_tonumberx (lau_State *L, int idx, int *pisnum) {
  lau_Number n = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tonumber(o, &n);
  if (pisnum)
    *pisnum = isnum;
  return n;
}


LAU_API lau_Integer lau_tointegerx (lau_State *L, int idx, int *pisnum) {
  lau_Integer res = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tointeger(o, &res);
  if (pisnum)
    *pisnum = isnum;
  return res;
}


LAU_API int lau_toboolean (lau_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return !l_isfalse(o);
}


LAU_API const char *lau_tolstring (lau_State *L, int idx, size_t *len) {
  TValue *o;
  lau_lock(L);
  o = index2value(L, idx);
  if (!ttisstring(o)) {
    if (!cvt2str(o)) {  /* not convertible? */
      if (len != NULL) *len = 0;
      lau_unlock(L);
      return NULL;
    }
    lauO_tostring(L, o);
    lauC_checkGC(L);
    o = index2value(L, idx);  /* previous call may reallocate the stack */
  }
  lau_unlock(L);
  if (len != NULL)
    return getlstr(tsvalue(o), *len);
  else
    return getstr(tsvalue(o));
}


LAU_API lau_Unsigned lau_rawlen (lau_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case LAU_VSHRSTR: return cast(lau_Unsigned, tsvalue(o)->shrlen);
    case LAU_VLNGSTR: return cast(lau_Unsigned, tsvalue(o)->u.lnglen);
    case LAU_VUSERDATA: return cast(lau_Unsigned, uvalue(o)->len);
    case LAU_VTABLE: {
      lau_Unsigned res;
      lau_lock(L);
      res = lauH_getn(L, hvalue(o));
      lau_unlock(L);
      return res;
    }
    default: return 0;
  }
}


LAU_API lau_CFunction lau_tocfunction (lau_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  if (ttislcf(o)) return fvalue(o);
  else if (ttisCclosure(o))
    return clCvalue(o)->f;
  else return NULL;  /* not a C function */
}


l_sinline void *touserdata (const TValue *o) {
  switch (ttype(o)) {
    case LAU_TUSERDATA: return getudatamem(uvalue(o));
    case LAU_TLIGHTUSERDATA: return pvalue(o);
    default: return NULL;
  }
}


LAU_API void *lau_touserdata (lau_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return touserdata(o);
}


LAU_API lau_State *lau_tothread (lau_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (!ttisthread(o)) ? NULL : thvalue(o);
}


/*
** Returns a pointer to the internal representation of an object.
** Note that ISO C does not allow the conversion of a pointer to
** function to a 'void*', so the conversion here goes through
** a 'size_t'. (As the returned pointer is only informative, this
** conversion should not be a problem.)
*/
LAU_API const void *lau_topointer (lau_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case LAU_VLCF: return cast_voidp(cast_sizet(fvalue(o)));
    case LAU_VUSERDATA: case LAU_VLIGHTUSERDATA:
      return touserdata(o);
    default: {
      if (iscollectable(o))
        return gcvalue(o);
      else
        return NULL;
    }
  }
}



/*
** push functions (C -> stack)
*/


LAU_API void lau_pushnil (lau_State *L) {
  lau_lock(L);
  setnilvalue2s(L->top.p);
  api_incr_top(L);
  lau_unlock(L);
}


LAU_API void lau_pushnumber (lau_State *L, lau_Number n) {
  lau_lock(L);
  setfltvalue(s2v(L->top.p), n);
  api_incr_top(L);
  lau_unlock(L);
}


LAU_API void lau_pushinteger (lau_State *L, lau_Integer n) {
  lau_lock(L);
  setivalue(s2v(L->top.p), n);
  api_incr_top(L);
  lau_unlock(L);
}


/*
** Pushes on the stack a string with given length. Avoid using 's' when
** 'len' == 0 (as 's' can be NULL in that case), due to later use of
** 'memcmp' and 'memcpy'.
*/
LAU_API const char *lau_pushlstring (lau_State *L, const char *s, size_t len) {
  TString *ts;
  lau_lock(L);
  ts = (len == 0) ? lauS_new(L, "") : lauS_newlstr(L, s, len);
  setsvalue2s(L, L->top.p, ts);
  api_incr_top(L);
  lauC_checkGC(L);
  lau_unlock(L);
  return getstr(ts);
}


LAU_API const char *lau_pushexternalstring (lau_State *L,
	        const char *s, size_t len, lau_Alloc falloc, void *ud) {
  TString *ts;
  lau_lock(L);
  api_check(L, len <= MAX_SIZE, "string too large");
  api_check(L, s[len] == '\0', "string not ending with zero");
  ts = lauS_newextlstr (L, s, len, falloc, ud);
  setsvalue2s(L, L->top.p, ts);
  api_incr_top(L);
  lauC_checkGC(L);
  lau_unlock(L);
  return getstr(ts);
}


LAU_API const char *lau_pushstring (lau_State *L, const char *s) {
  lau_lock(L);
  if (s == NULL)
    setnilvalue2s(L->top.p);
  else {
    TString *ts;
    ts = lauS_new(L, s);
    setsvalue2s(L, L->top.p, ts);
    s = getstr(ts);  /* internal copy's address */
  }
  api_incr_top(L);
  lauC_checkGC(L);
  lau_unlock(L);
  return s;
}


LAU_API const char *lau_pushvfstring (lau_State *L, const char *fmt,
                                      va_list argp) {
  const char *ret;
  lau_lock(L);
  ret = lauO_pushvfstring(L, fmt, argp);
  lauC_checkGC(L);
  lau_unlock(L);
  return ret;
}


LAU_API const char *lau_pushfstring (lau_State *L, const char *fmt, ...) {
  const char *ret;
  va_list argp;
  lau_lock(L);
  pushvfstring(L, argp, fmt, ret);
  lauC_checkGC(L);
  lau_unlock(L);
  return ret;
}


LAU_API void lau_pushcclosure (lau_State *L, lau_CFunction fn, int n) {
  lau_lock(L);
  if (n == 0) {
    setfvalue(s2v(L->top.p), fn);
    api_incr_top(L);
  }
  else {
    int i;
    CClosure *cl;
    api_checkpop(L, n);
    api_check(L, n <= MAXUPVAL, "upvalue index too large");
    cl = lauF_newCclosure(L, n);
    cl->f = fn;
    for (i = 0; i < n; i++) {
      setobj2n(L, &cl->upvalue[i], s2v(L->top.p - n + i));
      /* does not need barrier because closure is white */
      lau_assert(iswhite(cl));
    }
    L->top.p -= n;
    setclCvalue(L, s2v(L->top.p), cl);
    api_incr_top(L);
    lauC_checkGC(L);
  }
  lau_unlock(L);
}


LAU_API void lau_pushboolean (lau_State *L, int b) {
  lau_lock(L);
  if (b)
    setbtvalue(s2v(L->top.p));
  else
    setbfvalue(s2v(L->top.p));
  api_incr_top(L);
  lau_unlock(L);
}


LAU_API void lau_pushlightuserdata (lau_State *L, void *p) {
  lau_lock(L);
  setpvalue(s2v(L->top.p), p);
  api_incr_top(L);
  lau_unlock(L);
}


LAU_API int lau_pushthread (lau_State *L) {
  lau_lock(L);
  setthvalue(L, s2v(L->top.p), L);
  api_incr_top(L);
  lau_unlock(L);
  return (mainthread(G(L)) == L);
}



/*
** get functions (Lau -> stack)
*/


static int auxgetstr (lau_State *L, const TValue *t, const char *k) {
  lu_byte tag;
  TString *str = lauS_new(L, k);
  lauV_fastget(t, str, s2v(L->top.p), lauH_getstr, tag);
  if (!tagisempty(tag))
    api_incr_top(L);
  else {
    setsvalue2s(L, L->top.p, str);
    api_incr_top(L);
    tag = lauV_finishget(L, t, s2v(L->top.p - 1), L->top.p - 1, tag);
  }
  lau_unlock(L);
  return novariant(tag);
}


/*
** The following function assumes that the registry cannot be a weak
** table; so, an emergency collection while using the global table
** cannot collect it.
*/
static void getGlobalTable (lau_State *L, TValue *gt) {
  Table *registry = hvalue(&G(L)->l_registry);
  lu_byte tag = lauH_getint(registry, LAU_RIDX_GLOBALS, gt);
  (void)tag;  /* avoid not-used warnings when checks are off */
  api_check(L, novariant(tag) == LAU_TTABLE, "global table must exist");
}


LAU_API int lau_getglobal (lau_State *L, const char *name) {
  TValue gt;
  lau_lock(L);
  getGlobalTable(L, &gt);
  return auxgetstr(L, &gt, name);
}


LAU_API int lau_gettable (lau_State *L, int idx) {
  lu_byte tag;
  TValue *t;
  lau_lock(L);
  api_checkpop(L, 1);
  t = index2value(L, idx);
  lauV_fastget(t, s2v(L->top.p - 1), s2v(L->top.p - 1), lauH_get, tag);
  if (tagisempty(tag))
    tag = lauV_finishget(L, t, s2v(L->top.p - 1), L->top.p - 1, tag);
  lau_unlock(L);
  return novariant(tag);
}


LAU_API int lau_getfield (lau_State *L, int idx, const char *k) {
  lau_lock(L);
  return auxgetstr(L, index2value(L, idx), k);
}


LAU_API int lau_geti (lau_State *L, int idx, lau_Integer n) {
  TValue *t;
  lu_byte tag;
  lau_lock(L);
  t = index2value(L, idx);
  lauV_fastgeti(t, n, s2v(L->top.p), tag);
  if (tagisempty(tag)) {
    TValue key;
    setivalue(&key, n);
    tag = lauV_finishget(L, t, &key, L->top.p, tag);
  }
  api_incr_top(L);
  lau_unlock(L);
  return novariant(tag);
}


static int finishrawget (lau_State *L, lu_byte tag) {
  if (tagisempty(tag))  /* avoid copying empty items to the stack */
    setnilvalue2s(L->top.p);
  api_incr_top(L);
  lau_unlock(L);
  return novariant(tag);
}


l_sinline Table *gettable (lau_State *L, int idx) {
  TValue *t = index2value(L, idx);
  api_check(L, ttistable(t), "table expected");
  return hvalue(t);
}


LAU_API int lau_rawget (lau_State *L, int idx) {
  Table *t;
  lu_byte tag;
  lau_lock(L);
  api_checkpop(L, 1);
  t = gettable(L, idx);
  tag = lauH_get(t, s2v(L->top.p - 1), s2v(L->top.p - 1));
  L->top.p--;  /* pop key */
  return finishrawget(L, tag);
}


LAU_API int lau_rawgeti (lau_State *L, int idx, lau_Integer n) {
  Table *t;
  lu_byte tag;
  lau_lock(L);
  t = gettable(L, idx);
  lauH_fastgeti(t, n, s2v(L->top.p), tag);
  return finishrawget(L, tag);
}


LAU_API int lau_rawgetp (lau_State *L, int idx, const void *p) {
  Table *t;
  TValue k;
  lau_lock(L);
  t = gettable(L, idx);
  setpvalue(&k, cast_voidp(p));
  return finishrawget(L, lauH_get(t, &k, s2v(L->top.p)));
}


LAU_API void lau_createtable (lau_State *L, int narray, int nrec) {
  Table *t;
  lau_lock(L);
  t = lauH_new(L);
  sethvalue2s(L, L->top.p, t);
  api_incr_top(L);
  if (narray > 0 || nrec > 0)
    lauH_resize(L, t, cast_uint(narray), cast_uint(nrec));
  lauC_checkGC(L);
  lau_unlock(L);
}


LAU_API int lau_getmetatable (lau_State *L, int objindex) {
  const TValue *obj;
  Table *mt;
  int res = 0;
  lau_lock(L);
  obj = index2value(L, objindex);
  switch (ttype(obj)) {
    case LAU_TTABLE:
      mt = hvalue(obj)->metatable;
      break;
    case LAU_TUSERDATA:
      mt = uvalue(obj)->metatable;
      break;
    default:
      mt = G(L)->mt[ttype(obj)];
      break;
  }
  if (mt != NULL) {
    sethvalue2s(L, L->top.p, mt);
    api_incr_top(L);
    res = 1;
  }
  lau_unlock(L);
  return res;
}


LAU_API int lau_getiuservalue (lau_State *L, int idx, int n) {
  TValue *o;
  int t;
  lau_lock(L);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (n <= 0 || n > uvalue(o)->nuvalue) {
    setnilvalue2s(L->top.p);
    t = LAU_TNONE;
  }
  else {
    setobj2s(L, L->top.p, &uvalue(o)->uv[n - 1].uv);
    t = ttype(s2v(L->top.p));
  }
  api_incr_top(L);
  lau_unlock(L);
  return t;
}


/*
** set functions (stack -> Lau)
*/

/*
** t[k] = value at the top of the stack (where 'k' is a string)
*/
static void auxsetstr (lau_State *L, const TValue *t, const char *k) {
  int hres;
  TString *str = lauS_new(L, k);
  api_checkpop(L, 1);
  lauV_fastset(t, str, s2v(L->top.p - 1), hres, lauH_psetstr);
  if (hres == HOK) {
    lauV_finishfastset(L, t, s2v(L->top.p - 1));
    L->top.p--;  /* pop value */
  }
  else {
    setsvalue2s(L, L->top.p, str);  /* push 'str' (to make it a TValue) */
    api_incr_top(L);
    lauV_finishset(L, t, s2v(L->top.p - 1), s2v(L->top.p - 2), hres);
    L->top.p -= 2;  /* pop value and key */
  }
  lau_unlock(L);  /* lock done by caller */
}


LAU_API void lau_setglobal (lau_State *L, const char *name) {
  TValue gt;
  lau_lock(L);  /* unlock done in 'auxsetstr' */
  getGlobalTable(L, &gt);
  auxsetstr(L, &gt, name);
}


LAU_API void lau_settable (lau_State *L, int idx) {
  TValue *t;
  int hres;
  lau_lock(L);
  api_checkpop(L, 2);
  t = index2value(L, idx);
  lauV_fastset(t, s2v(L->top.p - 2), s2v(L->top.p - 1), hres, lauH_pset);
  if (hres == HOK)
    lauV_finishfastset(L, t, s2v(L->top.p - 1));
  else
    lauV_finishset(L, t, s2v(L->top.p - 2), s2v(L->top.p - 1), hres);
  L->top.p -= 2;  /* pop index and value */
  lau_unlock(L);
}


LAU_API void lau_setfield (lau_State *L, int idx, const char *k) {
  lau_lock(L);  /* unlock done in 'auxsetstr' */
  auxsetstr(L, index2value(L, idx), k);
}


LAU_API void lau_seti (lau_State *L, int idx, lau_Integer n) {
  TValue *t;
  int hres;
  lau_lock(L);
  api_checkpop(L, 1);
  t = index2value(L, idx);
  lauV_fastseti(t, n, s2v(L->top.p - 1), hres);
  if (hres == HOK)
    lauV_finishfastset(L, t, s2v(L->top.p - 1));
  else {
    TValue temp;
    setivalue(&temp, n);
    lauV_finishset(L, t, &temp, s2v(L->top.p - 1), hres);
  }
  L->top.p--;  /* pop value */
  lau_unlock(L);
}


static void aux_rawset (lau_State *L, int idx, TValue *key, int n) {
  Table *t;
  lau_lock(L);
  api_checkpop(L, n);
  t = gettable(L, idx);
  lauH_set(L, t, key, s2v(L->top.p - 1));
  invalidateTMcache(t);
  lauC_barrierback(L, obj2gco(t), s2v(L->top.p - 1));
  L->top.p -= n;
  lau_unlock(L);
}


LAU_API void lau_rawset (lau_State *L, int idx) {
  aux_rawset(L, idx, s2v(L->top.p - 2), 2);
}


LAU_API void lau_rawsetp (lau_State *L, int idx, const void *p) {
  TValue k;
  setpvalue(&k, cast_voidp(p));
  aux_rawset(L, idx, &k, 1);
}


LAU_API void lau_rawseti (lau_State *L, int idx, lau_Integer n) {
  Table *t;
  lau_lock(L);
  api_checkpop(L, 1);
  t = gettable(L, idx);
  lauH_setint(L, t, n, s2v(L->top.p - 1));
  lauC_barrierback(L, obj2gco(t), s2v(L->top.p - 1));
  L->top.p--;
  lau_unlock(L);
}


LAU_API int lau_setmetatable (lau_State *L, int objindex) {
  TValue *obj;
  Table *mt;
  lau_lock(L);
  api_checkpop(L, 1);
  obj = index2value(L, objindex);
  if (ttisnil(s2v(L->top.p - 1)))
    mt = NULL;
  else {
    api_check(L, ttistable(s2v(L->top.p - 1)), "table expected");
    mt = hvalue(s2v(L->top.p - 1));
  }
  switch (ttype(obj)) {
    case LAU_TTABLE: {
      hvalue(obj)->metatable = mt;
      if (mt) {
        lauC_objbarrier(L, gcvalue(obj), mt);
        lauC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    case LAU_TUSERDATA: {
      uvalue(obj)->metatable = mt;
      if (mt) {
        lauC_objbarrier(L, uvalue(obj), mt);
        lauC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    default: {
      G(L)->mt[ttype(obj)] = mt;
      break;
    }
  }
  L->top.p--;
  lau_unlock(L);
  return 1;
}


LAU_API int lau_setiuservalue (lau_State *L, int idx, int n) {
  TValue *o;
  int res;
  lau_lock(L);
  api_checkpop(L, 1);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (!(cast_uint(n) - 1u < cast_uint(uvalue(o)->nuvalue)))
    res = 0;  /* 'n' not in [1, uvalue(o)->nuvalue] */
  else {
    setobj(L, &uvalue(o)->uv[n - 1].uv, s2v(L->top.p - 1));
    lauC_barrierback(L, gcvalue(o), s2v(L->top.p - 1));
    res = 1;
  }
  L->top.p--;
  lau_unlock(L);
  return res;
}


/*
** 'load' and 'call' functions (run Lau code)
*/


#define checkresults(L,na,nr) \
     (api_check(L, (nr) == LAU_MULTRET \
               || (L->ci->top.p - L->top.p >= (nr) - (na)), \
	"results from function overflow current stack size"), \
      api_check(L, LAU_MULTRET <= (nr) && (nr) <= MAXRESULTS,  \
                   "invalid number of results"))


LAU_API void lau_callk (lau_State *L, int nargs, int nresults,
                        lau_KContext ctx, lau_KFunction k) {
  StkId func;
  lau_lock(L);
  api_check(L, k == NULL || !isLau(L->ci),
    "cannot use continuations inside hooks");
  api_checkpop(L, nargs + 1);
  api_check(L, L->status == LAU_OK, "cannot do calls on non-normal thread");
  checkresults(L, nargs, nresults);
  func = L->top.p - (nargs+1);
  if (k != NULL && yieldable(L)) {  /* need to prepare continuation? */
    L->ci->u.c.k = k;  /* save continuation */
    L->ci->u.c.ctx = ctx;  /* save context */
    lauD_call(L, func, nresults);  /* do the call */
  }
  else  /* no continuation or no yieldable */
    lauD_callnoyield(L, func, nresults);  /* just do the call */
  adjustresults(L, nresults);
  lau_unlock(L);
}



/*
** Execute a protected call.
*/
struct CallS {  /* data to 'f_call' */
  StkId func;
  int nresults;
};


static void f_call (lau_State *L, void *ud) {
  struct CallS *c = cast(struct CallS *, ud);
  lauD_callnoyield(L, c->func, c->nresults);
}



LAU_API int lau_pcallk (lau_State *L, int nargs, int nresults, int errfunc,
                        lau_KContext ctx, lau_KFunction k) {
  struct CallS c;
  TStatus status;
  ptrdiff_t func;
  lau_lock(L);
  api_check(L, k == NULL || !isLau(L->ci),
    "cannot use continuations inside hooks");
  api_checkpop(L, nargs + 1);
  api_check(L, L->status == LAU_OK, "cannot do calls on non-normal thread");
  checkresults(L, nargs, nresults);
  if (errfunc == 0)
    func = 0;
  else {
    StkId o = index2stack(L, errfunc);
    api_check(L, ttisfunction(s2v(o)), "error handler must be a function");
    func = savestack(L, o);
  }
  c.func = L->top.p - (nargs+1);  /* function to be called */
  if (k == NULL || !yieldable(L)) {  /* no continuation or no yieldable? */
    c.nresults = nresults;  /* do a 'conventional' protected call */
    status = lauD_pcall(L, f_call, &c, savestack(L, c.func), func);
  }
  else {  /* prepare continuation (call is already protected by 'resume') */
    CallInfo *ci = L->ci;
    ci->u.c.k = k;  /* save continuation */
    ci->u.c.ctx = ctx;  /* save context */
    /* save information for error recovery */
    ci->u2.funcidx = cast_int(savestack(L, c.func));
    ci->u.c.old_errfunc = L->errfunc;
    L->errfunc = func;
    setoah(ci, L->allowhook);  /* save value of 'allowhook' */
    ci->callstatus |= CIST_YPCALL;  /* function can do error recovery */
    lauD_call(L, c.func, nresults);  /* do the call */
    ci->callstatus &= ~CIST_YPCALL;
    L->errfunc = ci->u.c.old_errfunc;
    status = LAU_OK;  /* if it is here, there were no errors */
  }
  adjustresults(L, nresults);
  lau_unlock(L);
  return APIstatus(status);
}


LAU_API int lau_load (lau_State *L, lau_Reader reader, void *data,
                      const char *chunkname, const char *mode) {
  ZIO z;
  TStatus status;
  lau_lock(L);
  lauC_checkGC(L);
  if (!chunkname) chunkname = "?";
  lauZ_init(L, &z, reader, data);
  status = lauD_protectedparser(L, &z, chunkname, mode);
  if (status == LAU_OK) {  /* no errors? */
    LClosure *f = clLvalue(s2v(L->top.p - 1));  /* get new function */
    if (f->nupvalues >= 1) {  /* does it have an upvalue? */
      /* get global table from registry */
      TValue gt;
      getGlobalTable(L, &gt);
      /* set global table as 1st upvalue of 'f' (may be LAU_ENV) */
      setobj(L, f->upvals[0]->v.p, &gt);
      lauC_barrier(L, f->upvals[0], &gt);
    }
  }
  lau_unlock(L);
  return APIstatus(status);
}


/*
** Dump a Lau function, calling 'writer' to write its parts. Ensure
** the stack returns with its original size.
*/
LAU_API int lau_dump (lau_State *L, lau_Writer writer, void *data, int strip) {
  int status;
  ptrdiff_t otop = savestack(L, L->top.p);  /* original top */
  TValue *f = s2v(L->top.p - 1);  /* function to be dumped */
  lau_lock(L);
  api_checkpop(L, 1);
  api_check(L, isLfunction(f), "Lau function expected");
  status = lauU_dump(L, clLvalue(f)->p, writer, data, strip);
  L->top.p = restorestack(L, otop);  /* restore top */
  lau_unlock(L);
  return status;
}


LAU_API int lau_status (lau_State *L) {
  return APIstatus(L->status);
}


/*
** Garbage-collection function
*/
LAU_API int lau_gc (lau_State *L, int what, ...) {
  va_list argp;
  int res = 0;
  global_State *g = G(L);
  if (g->gcstp & (GCSTPGC | GCSTPCLS))  /* internal stop? */
    return -1;  /* all options are invalid when stopped */
  lau_lock(L);
  va_start(argp, what);
  switch (what) {
    case LAU_GCSTOP: {
      g->gcstp = GCSTPUSR;  /* stopped by the user */
      break;
    }
    case LAU_GCRESTART: {
      lauE_setdebt(g, 0);
      g->gcstp = 0;  /* (other bits must be zero here) */
      break;
    }
    case LAU_GCCOLLECT: {
      lauC_fullgc(L, 0);
      break;
    }
    case LAU_GCCOUNT: {
      /* GC values are expressed in Kbytes: #bytes/2^10 */
      res = cast_int(gettotalbytes(g) >> 10);
      break;
    }
    case LAU_GCCOUNTB: {
      res = cast_int(gettotalbytes(g) & 0x3ff);
      break;
    }
    case LAU_GCSTEP: {
      lu_byte oldstp = g->gcstp;
      l_mem n = cast(l_mem, va_arg(argp, size_t));
      l_mem newdebt;
      int work = 0;  /* true if GC did some work */
      g->gcstp = 0;  /* allow GC to run (other bits must be zero here) */
      if (n <= 0)
        newdebt = 0;  /* force to run one basic step */
      else if (g->GCdebt >= n - MAX_LMEM)  /* no overflow? */
        newdebt = g->GCdebt - n;
      else  /* overflow */
        newdebt = -MAX_LMEM;  /* set debt to miminum value */
      lauE_setdebt(g, newdebt);
      lauC_condGC(L, (void)0, work = 1);
      if (work && g->gcstate == GCSpause)  /* end of cycle? */
        res = 1;  /* signal it */
      g->gcstp = oldstp;  /* restore previous state */
      break;
    }
    case LAU_GCISRUNNING: {
      res = gcrunning(g);
      break;
    }
    case LAU_GCGEN: {
      res = (g->gckind == KGC_INC) ? LAU_GCINC : LAU_GCGEN;
      lauC_changemode(L, KGC_GENMINOR);
      break;
    }
    case LAU_GCINC: {
      res = (g->gckind == KGC_INC) ? LAU_GCINC : LAU_GCGEN;
      lauC_changemode(L, KGC_INC);
      break;
    }
    case LAU_GCPARAM: {
      int param = va_arg(argp, int);
      int value = va_arg(argp, int);
      api_check(L, 0 <= param && param < LAU_GCPN, "invalid parameter");
      res = cast_int(lauO_applyparam(g->gcparams[param], 100));
      if (value >= 0)
        g->gcparams[param] = lauO_codeparam(cast_uint(value));
      break;
    }
    default: res = -1;  /* invalid option */
  }
  va_end(argp);
  lau_unlock(L);
  return res;
}



/*
** miscellaneous functions
*/


LAU_API int lau_error (lau_State *L) {
  TValue *errobj;
  lau_lock(L);
  errobj = s2v(L->top.p - 1);
  api_checkpop(L, 1);
  /* error object is the memory error message? */
  if (ttisshrstring(errobj) && eqshrstr(tsvalue(errobj), G(L)->memerrmsg))
    lauM_error(L);  /* raise a memory error */
  else
    lauG_errormsg(L);  /* raise a regular error */
  /* code unreachable; will unlock when control actually leaves the kernel */
  return 0;  /* to avoid warnings */
}


LAU_API int lau_next (lau_State *L, int idx) {
  Table *t;
  int more;
  lau_lock(L);
  api_checkpop(L, 1);
  t = gettable(L, idx);
  more = lauH_next(L, t, L->top.p - 1);
  if (more)
    api_incr_top(L);
  else  /* no more elements */
    L->top.p--;  /* pop key */
  lau_unlock(L);
  return more;
}


LAU_API void lau_toclose (lau_State *L, int idx) {
  StkId o;
  lau_lock(L);
  o = index2stack(L, idx);
  api_check(L, L->tbclist.p < o, "given index below or equal a marked one");
  lauF_newtbcupval(L, o);  /* create new to-be-closed upvalue */
  L->ci->callstatus |= CIST_TBC;  /* mark that function has TBC slots */
  lau_unlock(L);
}


LAU_API void lau_concat (lau_State *L, int n) {
  lau_lock(L);
  api_checknelems(L, n);
  if (n > 0) {
    lauV_concat(L, n);
    lauC_checkGC(L);
  }
  else {  /* nothing to concatenate */
    setsvalue2s(L, L->top.p, lauS_newlstr(L, "", 0));  /* push empty string */
    api_incr_top(L);
  }
  lau_unlock(L);
}


LAU_API void lau_len (lau_State *L, int idx) {
  TValue *t;
  lau_lock(L);
  t = index2value(L, idx);
  lauV_objlen(L, L->top.p, t);
  api_incr_top(L);
  lau_unlock(L);
}


LAU_API lau_Alloc lau_getallocf (lau_State *L, void **ud) {
  lau_Alloc f;
  lau_lock(L);
  if (ud) *ud = G(L)->ud;
  f = G(L)->frealloc;
  lau_unlock(L);
  return f;
}


LAU_API void lau_setallocf (lau_State *L, lau_Alloc f, void *ud) {
  lau_lock(L);
  G(L)->ud = ud;
  G(L)->frealloc = f;
  lau_unlock(L);
}


void lau_setwarnf (lau_State *L, lau_WarnFunction f, void *ud) {
  lau_lock(L);
  G(L)->ud_warn = ud;
  G(L)->warnf = f;
  lau_unlock(L);
}


void lau_warning (lau_State *L, const char *msg, int tocont) {
  lau_lock(L);
  lauE_warning(L, msg, tocont);
  lau_unlock(L);
}



LAU_API void *lau_newuserdatauv (lau_State *L, size_t size, int nuvalue) {
  Udata *u;
  lau_lock(L);
  api_check(L, 0 <= nuvalue && nuvalue < SHRT_MAX, "invalid value");
  u = lauS_newudata(L, size, cast(unsigned short, nuvalue));
  setuvalue(L, s2v(L->top.p), u);
  api_incr_top(L);
  lauC_checkGC(L);
  lau_unlock(L);
  return getudatamem(u);
}



static const char *aux_upvalue (TValue *fi, int n, TValue **val,
                                GCObject **owner) {
  switch (ttypetag(fi)) {
    case LAU_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (!(cast_uint(n) - 1u < cast_uint(f->nupvalues)))
        return NULL;  /* 'n' not in [1, f->nupvalues] */
      *val = &f->upvalue[n-1];
      if (owner) *owner = obj2gco(f);
      return "";
    }
    case LAU_VLCL: {  /* Lau closure */
      LClosure *f = clLvalue(fi);
      TString *name;
      Proto *p = f->p;
      if (!(cast_uint(n) - 1u  < cast_uint(p->sizeupvalues)))
        return NULL;  /* 'n' not in [1, p->sizeupvalues] */
      *val = f->upvals[n-1]->v.p;
      if (owner) *owner = obj2gco(f->upvals[n - 1]);
      name = p->upvalues[n-1].name;
      return (name == NULL) ? "(no name)" : getstr(name);
    }
    default: return NULL;  /* not a closure */
  }
}


LAU_API const char *lau_getupvalue (lau_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  lau_lock(L);
  name = aux_upvalue(index2value(L, funcindex), n, &val, NULL);
  if (name) {
    setobj2s(L, L->top.p, val);
    api_incr_top(L);
  }
  lau_unlock(L);
  return name;
}


LAU_API const char *lau_setupvalue (lau_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  GCObject *owner = NULL;  /* to avoid warnings */
  TValue *fi;
  lau_lock(L);
  fi = index2value(L, funcindex);
  api_checknelems(L, 1);
  name = aux_upvalue(fi, n, &val, &owner);
  if (name) {
    L->top.p--;
    setobj(L, val, s2v(L->top.p));
    lauC_barrier(L, owner, val);
  }
  lau_unlock(L);
  return name;
}


static UpVal **getupvalref (lau_State *L, int fidx, int n, LClosure **pf) {
  static const UpVal *const nullup = NULL;
  LClosure *f;
  TValue *fi = index2value(L, fidx);
  api_check(L, ttisLclosure(fi), "Lau function expected");
  f = clLvalue(fi);
  if (pf) *pf = f;
  if (1 <= n && n <= f->p->sizeupvalues)
    return &f->upvals[n - 1];  /* get its upvalue pointer */
  else
    return (UpVal**)&nullup;
}


LAU_API void *lau_upvalueid (lau_State *L, int fidx, int n) {
  TValue *fi = index2value(L, fidx);
  switch (ttypetag(fi)) {
    case LAU_VLCL: {  /* lau closure */
      return *getupvalref(L, fidx, n, NULL);
    }
    case LAU_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (1 <= n && n <= f->nupvalues)
        return &f->upvalue[n - 1];
      /* else */
    }  /* FALLTHROUGH */
    case LAU_VLCF:
      return NULL;  /* light C functions have no upvalues */
    default: {
      api_check(L, 0, "function expected");
      return NULL;
    }
  }
}


LAU_API void lau_upvaluejoin (lau_State *L, int fidx1, int n1,
                                            int fidx2, int n2) {
  LClosure *f1;
  UpVal **up1 = getupvalref(L, fidx1, n1, &f1);
  UpVal **up2 = getupvalref(L, fidx2, n2, NULL);
  api_check(L, *up1 != NULL && *up2 != NULL, "invalid upvalue index");
  *up1 = *up2;
  lauC_objbarrier(L, f1, *up1);
}


