/*
** $Id: lbaselib.c $
** Basic library
** See Copyright Notice in lau.h
*/

#define lbaselib_c
#define LAU_LIB

#include "lprefix.h"


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lau.h"

#include "lauxlib.h"
#include "laulib.h"
#include "llimits.h"


static int lauB_print (lau_State *L) {
  int n = lau_gettop(L);  /* number of arguments */
  int i;
  for (i = 1; i <= n; i++) {  /* for each argument */
    size_t l;
    const char *s = lauL_tolstring(L, i, &l);  /* convert it to string */
    if (i > 1)  /* not the first element? */
      lau_writestring("\t", 1);  /* add a tab before it */
    lau_writestring(s, l);  /* print it */
    lau_pop(L, 1);  /* pop result */
  }
  lau_writeline();
  return 0;
}


/*
** Creates a warning with all given arguments.
** Check first for errors; otherwise an error may interrupt
** the composition of a warning, leaving it unfinished.
*/
static int lauB_warn (lau_State *L) {
  int n = lau_gettop(L);  /* number of arguments */
  int i;
  lauL_checkstring(L, 1);  /* at least one argument */
  for (i = 2; i <= n; i++)
    lauL_checkstring(L, i);  /* make sure all arguments are strings */
  for (i = 1; i < n; i++)  /* compose warning */
    lau_warning(L, lau_tostring(L, i), 1);
  lau_warning(L, lau_tostring(L, n), 0);  /* close warning */
  return 0;
}


#define SPACECHARS	" \f\n\r\t\v"

static const char *b_str2int (const char *s, unsigned base, lau_Integer *pn) {
  lau_Unsigned n = 0;
  int neg = 0;
  s += strspn(s, SPACECHARS);  /* skip initial spaces */
  if (*s == '-') { s++; neg = 1; }  /* handle sign */
  else if (*s == '+') s++;
  if (!isalnum(cast_uchar(*s)))  /* no digit? */
    return NULL;
  do {
    unsigned digit = cast_uint(isdigit(cast_uchar(*s))
                               ? *s - '0'
                               : (toupper(cast_uchar(*s)) - 'A') + 10);
    if (digit >= base) return NULL;  /* invalid numeral */
    n = n * base + digit;
    s++;
  } while (isalnum(cast_uchar(*s)));
  s += strspn(s, SPACECHARS);  /* skip trailing spaces */
  *pn = (lau_Integer)((neg) ? (0u - n) : n);
  return s;
}


static int lauB_tonumber (lau_State *L) {
  if (lau_isnoneornil(L, 2)) {  /* standard conversion? */
    if (lau_type(L, 1) == LAU_TNUMBER) {  /* already a number? */
      lau_settop(L, 1);  /* yes; return it */
      return 1;
    }
    else {
      size_t l;
      const char *s = lau_tolstring(L, 1, &l);
      if (s != NULL && lau_stringtonumber(L, s) == l + 1)
        return 1;  /* successful conversion to number */
      /* else not a number */
      lauL_checkany(L, 1);  /* (but there must be some parameter) */
    }
  }
  else {
    size_t l;
    const char *s;
    lau_Integer n = 0;  /* to avoid warnings */
    lau_Integer base = lauL_checkinteger(L, 2);
    lauL_checktype(L, 1, LAU_TSTRING);  /* no numbers as strings */
    s = lau_tolstring(L, 1, &l);
    lauL_argcheck(L, 2 <= base && base <= 36, 2, "base out of range");
    if (b_str2int(s, cast_uint(base), &n) == s + l) {
      lau_pushinteger(L, n);
      return 1;
    }  /* else not a number */
  }  /* else not a number */
  lauL_pushfail(L);  /* not a number */
  return 1;
}


static int lauB_error (lau_State *L) {
  int level = (int)lauL_optinteger(L, 2, 1);
  lau_settop(L, 1);
  if (lau_type(L, 1) == LAU_TSTRING && level > 0) {
    lauL_where(L, level);   /* add extra information */
    lau_pushvalue(L, 1);
    lau_concat(L, 2);
  }
  return lau_error(L);
}


static int lauB_getmetatable (lau_State *L) {
  lauL_checkany(L, 1);
  if (!lau_getmetatable(L, 1)) {
    lau_pushnil(L);
    return 1;  /* no metatable */
  }
  lauL_getmetafield(L, 1, "__metatable");
  return 1;  /* returns either __metatable field (if present) or metatable */
}


static int lauB_setmetatable (lau_State *L) {
  int t = lau_type(L, 2);
  lauL_checktype(L, 1, LAU_TTABLE);
  lauL_argexpected(L, t == LAU_TNIL || t == LAU_TTABLE, 2, "nil or table");
  if (l_unlikely(lauL_getmetafield(L, 1, "__metatable") != LAU_TNIL))
    return lauL_error(L, "cannot change a protected metatable");
  lau_settop(L, 2);
  lau_setmetatable(L, 1);
  return 1;
}


static int lauB_rawequal (lau_State *L) {
  lauL_checkany(L, 1);
  lauL_checkany(L, 2);
  lau_pushboolean(L, lau_rawequal(L, 1, 2));
  return 1;
}


static int lauB_rawlen (lau_State *L) {
  int t = lau_type(L, 1);
  lauL_argexpected(L, t == LAU_TTABLE || t == LAU_TSTRING, 1,
                      "table or string");
  lau_pushinteger(L, l_castU2S(lau_rawlen(L, 1)));
  return 1;
}


static int lauB_rawget (lau_State *L) {
  lauL_checktype(L, 1, LAU_TTABLE);
  lauL_checkany(L, 2);
  lau_settop(L, 2);
  lau_rawget(L, 1);
  return 1;
}

static int lauB_rawset (lau_State *L) {
  lauL_checktype(L, 1, LAU_TTABLE);
  lauL_checkany(L, 2);
  lauL_checkany(L, 3);
  lau_settop(L, 3);
  lau_rawset(L, 1);
  return 1;
}


static int pushmode (lau_State *L, int oldmode) {
  if (oldmode == -1)
    lauL_pushfail(L);  /* invalid call to 'lau_gc' */
  else
    lau_pushstring(L, (oldmode == LAU_GCINC) ? "incremental"
                                             : "generational");
  return 1;
}


/*
** check whether call to 'lau_gc' was valid (not inside a finalizer)
*/
#define checkvalres(res) { if (res == -1) break; }

static int lauB_collectgarbage (lau_State *L) {
  static const char *const opts[] = {"stop", "restart", "collect",
    "count", "step", "isrunning", "generational", "incremental",
    "param", NULL};
  static const char optsnum[] = {LAU_GCSTOP, LAU_GCRESTART, LAU_GCCOLLECT,
    LAU_GCCOUNT, LAU_GCSTEP, LAU_GCISRUNNING, LAU_GCGEN, LAU_GCINC,
    LAU_GCPARAM};
  int o = optsnum[lauL_checkoption(L, 1, "collect", opts)];
  switch (o) {
    case LAU_GCCOUNT: {
      int k = lau_gc(L, o);
      int b = lau_gc(L, LAU_GCCOUNTB);
      checkvalres(k);
      lau_pushnumber(L, (lau_Number)k + ((lau_Number)b/1024));
      return 1;
    }
    case LAU_GCSTEP: {
      lau_Integer n = lauL_optinteger(L, 2, 0);
      int res = lau_gc(L, o, cast_sizet(n));
      checkvalres(res);
      lau_pushboolean(L, res);
      return 1;
    }
    case LAU_GCISRUNNING: {
      int res = lau_gc(L, o);
      checkvalres(res);
      lau_pushboolean(L, res);
      return 1;
    }
    case LAU_GCGEN: {
      return pushmode(L, lau_gc(L, o));
    }
    case LAU_GCINC: {
      return pushmode(L, lau_gc(L, o));
    }
    case LAU_GCPARAM: {
      static const char *const params[] = {
        "minormul", "majorminor", "minormajor",
        "pause", "stepmul", "stepsize", NULL};
      static const char pnum[] = {
        LAU_GCPMINORMUL, LAU_GCPMAJORMINOR, LAU_GCPMINORMAJOR,
        LAU_GCPPAUSE, LAU_GCPSTEPMUL, LAU_GCPSTEPSIZE};
      int p = pnum[lauL_checkoption(L, 2, NULL, params)];
      lau_Integer value = lauL_optinteger(L, 3, -1);
      lau_pushinteger(L, lau_gc(L, o, p, (int)value));
      return 1;
    }
    default: {
      int res = lau_gc(L, o);
      checkvalres(res);
      lau_pushinteger(L, res);
      return 1;
    }
  }
  lauL_pushfail(L);  /* invalid call (inside a finalizer) */
  return 1;
}


static int lauB_type (lau_State *L) {
  int t = lau_type(L, 1);
  lauL_argcheck(L, t != LAU_TNONE, 1, "value expected");
  lau_pushstring(L, lau_typename(L, t));
  return 1;
}


static int lauB_next (lau_State *L) {
  lauL_checktype(L, 1, LAU_TTABLE);
  lau_settop(L, 2);  /* create a 2nd argument if there isn't one */
  if (lau_next(L, 1))
    return 2;
  else {
    lau_pushnil(L);
    return 1;
  }
}


static int pairscont (lau_State *L, int status, lau_KContext k) {
  (void)L; (void)status; (void)k;  /* unused */
  return 4;  /* __pairs did all the work, just return its results */
}

static int lauB_pairs (lau_State *L) {
  lauL_checkany(L, 1);
  if (lauL_getmetafield(L, 1, "__pairs") == LAU_TNIL) {  /* no metamethod? */
    lau_pushcfunction(L, lauB_next);  /* will return generator and */
    lau_pushvalue(L, 1);  /* state */
    lau_pushnil(L);  /* initial value */
    lau_pushnil(L);  /* to-be-closed object */
  }
  else {
    lau_pushvalue(L, 1);  /* argument 'self' to metamethod */
    lau_callk(L, 1, 4, 0, pairscont);  /* get 4 values from metamethod */
  }
  return 4;
}


static int load_aux (lau_State *L, int status, int envidx) {
  if (l_likely(status == LAU_OK)) {
    if (envidx != 0) {  /* 'env' parameter? */
      lau_pushvalue(L, envidx);  /* environment for loaded function */
      if (!lau_setupvalue(L, -2, 1))  /* set it as 1st upvalue */
        lau_pop(L, 1);  /* remove 'env' if not used by previous call */
    }
    return 1;
  }
  else {  /* error (message is on top of the stack) */
    lauL_pushfail(L);
    lau_insert(L, -2);  /* put before error message */
    return 2;  /* return fail plus error message */
  }
}


static const char *getMode (lau_State *L, int idx) {
  const char *mode = lauL_optstring(L, idx, NULL);
  if (mode != NULL && strchr(mode, 'B') != NULL) {
    /* Lau code cannot use fixed buffers */
    lauL_argerror(L, idx, "invalid mode");
  }
  return mode;
}


static int lauB_loadfile (lau_State *L) {
  const char *fname = lauL_optstring(L, 1, NULL);
  const char *mode = getMode(L, 2);
  int env = (!lau_isnone(L, 3) ? 3 : 0);  /* 'env' index or 0 if no 'env' */
  int status = lauL_loadfilex(L, fname, mode);
  return load_aux(L, status, env);
}


/*
** {======================================================
** Generic Read function
** =======================================================
*/


/*
** Reader for generic 'load' function.
*/
static const char *generic_reader (lau_State *L, void *ud, size_t *size) {
  int *firstcall = cast(int *, ud);
  lauL_checkstack(L, 2, "too many nested functions");
  if (*firstcall)
    *firstcall = 0;
  else
    lau_pop(L, 1);  /* remove previous result */
  lau_pushvalue(L, 1);  /* get function */
  lau_call(L, 0, 1);  /* call it */
  if (lau_isnil(L, -1)) {
    *size = 0;
    return NULL;
  }
  else if (l_unlikely(!lau_isstring(L, -1)))
    lauL_error(L, "reader function must return a string");
  return lau_tolstring(L, -1, size);
}


static int lauB_load (lau_State *L) {
  int status;
  size_t l;
  const char *s = lau_tolstring(L, 1, &l);
  const char *mode = getMode(L, 3);
  int env = (!lau_isnone(L, 4) ? 4 : 0);  /* 'env' index or 0 if no 'env' */
  if (s != NULL) {  /* loading a string? */
    const char *chunkname = lauL_optstring(L, 2, s);
    status = lauL_loadbufferx(L, s, l, chunkname, mode);
  }
  else {  /* loading from a reader function */
    int firstcall = 1;  /* userdata for generic_reader */
    const char *chunkname = lauL_optstring(L, 2, "=(load)");
    lauL_checktype(L, 1, LAU_TFUNCTION);
    status = lau_load(L, generic_reader, &firstcall, chunkname, mode);
  }
  return load_aux(L, status, env);
}

/* }====================================================== */


static int dofilecont (lau_State *L, int d1, lau_KContext d2) {
  (void)d1;  (void)d2;  /* only to match 'lau_Kfunction' prototype */
  return lau_gettop(L) - 1;
}


static int lauB_dofile (lau_State *L) {
  const char *fname = lauL_optstring(L, 1, NULL);
  lau_settop(L, 1);
  if (l_unlikely(lauL_loadfilex(L, fname, "bt") != LAU_OK))
    return lau_error(L);
  lau_callk(L, 0, LAU_MULTRET, 0, dofilecont);
  return dofilecont(L, 0, 0);
}


static int lauB_assert (lau_State *L) {
  if (l_likely(lau_toboolean(L, 1)))  /* condition is true? */
    return lau_gettop(L);  /* return all arguments */
  else {  /* error */
    lauL_checkany(L, 1);  /* there must be a condition */
    lau_remove(L, 1);  /* remove it */
    lau_pushliteral(L, "assertion failed!");  /* default message */
    lau_settop(L, 1);  /* leave only message (default if no other one) */
    return lauB_error(L);  /* call 'error' */
  }
}


static int lauB_select (lau_State *L) {
  int n = lau_gettop(L);
  if (lau_type(L, 1) == LAU_TSTRING && *lau_tostring(L, 1) == '#') {
    lau_pushinteger(L, n-1);
    return 1;
  }
  else {
    lau_Integer i = lauL_checkinteger(L, 1);
    if (i < 0) i = n + i;
    else if (i > n) i = n;
    lauL_argcheck(L, 1 <= i, 1, "index out of range");
    return n - (int)i;
  }
}


/*
** Continuation function for 'pcall' and 'xpcall'. Both functions
** already pushed a 'true' before doing the call, so in case of success
** 'finishpcall' only has to return everything in the stack minus
** 'extra' values (where 'extra' is exactly the number of items to be
** ignored).
*/
static int finishpcall (lau_State *L, int status, lau_KContext extra) {
  if (l_unlikely(status != LAU_OK && status != LAU_YIELD)) {  /* error? */
    lau_pushboolean(L, 0);  /* first result (false) */
    lau_pushvalue(L, -2);  /* error message */
    return 2;  /* return false, msg */
  }
  else
    return lau_gettop(L) - (int)extra;  /* return all results */
}


static int lauB_pcall (lau_State *L) {
  int status;
  lauL_checkany(L, 1);
  lau_pushboolean(L, 1);  /* first result if no errors */
  lau_insert(L, 1);  /* put it in place */
  status = lau_pcallk(L, lau_gettop(L) - 2, LAU_MULTRET, 0, 0, finishpcall);
  return finishpcall(L, status, 0);
}


/*
** Do a protected call with error handling. After 'lau_rotate', the
** stack will have <f, err, true, f, [args...]>; so, the function passes
** 2 to 'finishpcall' to skip the 2 first values when returning results.
*/
static int lauB_xpcall (lau_State *L) {
  int status;
  int n = lau_gettop(L);
  lauL_checktype(L, 2, LAU_TFUNCTION);  /* check error function */
  lau_pushboolean(L, 1);  /* first result */
  lau_pushvalue(L, 1);  /* function */
  lau_rotate(L, 3, 2);  /* move them below function's arguments */
  status = lau_pcallk(L, n - 2, LAU_MULTRET, 2, 2, finishpcall);
  return finishpcall(L, status, 2);
}


static int lauB_tostring (lau_State *L) {
  lauL_checkany(L, 1);
  lauL_tolstring(L, 1, NULL);
  return 1;
}


static const lauL_Reg base_funcs[] = {
  {"assert", lauB_assert},
  {"collectgarbage", lauB_collectgarbage},
  {"dofile", lauB_dofile},
  {"error", lauB_error},
  {"getmetatable", lauB_getmetatable},
  {"loadfile", lauB_loadfile},
  {"load", lauB_load},
  {"next", lauB_next},
  {"inpairs", lauB_pairs},
  {"pcall", lauB_pcall},
  {"print", lauB_print},
  {"warn", lauB_warn},
  {"rawequal", lauB_rawequal},
  {"rawlen", lauB_rawlen},
  {"rawget", lauB_rawget},
  {"rawset", lauB_rawset},
  {"select", lauB_select},
  {"setmetatable", lauB_setmetatable},
  {"tonumber", lauB_tonumber},
  {"tostring", lauB_tostring},
  {"type", lauB_type},
  {"xpcall", lauB_xpcall},
  /* placeholders */
  {LAU_GNAME, NULL},
  {"_VERSION", NULL},
  {NULL, NULL}
};


LAUMOD_API int lauopen_base (lau_State *L) {
  /* open lib into global table */
  lau_pushglobaltable(L);
  lauL_setfuncs(L, base_funcs, 0);
  /* set global _G */
  lau_pushvalue(L, -1);
  lau_setfield(L, -2, LAU_GNAME);
  /* set global _VERSION */
  lau_pushliteral(L, LAU_VERSION);
  lau_setfield(L, -2, "_VERSION");
  return 1;
}

