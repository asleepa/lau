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
      lau_writestring(" ", 1);  /* add a space before it */
    lau_writestring(s, l);  /* print it */
    lau_pop(L, 1);  /* pop result */
  }
  lau_writeline();
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


/* }====================================================== */


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


static int lauB_tostring (lau_State *L) {
  lauL_checkany(L, 1);
  lauL_tolstring(L, 1, NULL);
  return 1;
}


static const lauL_Reg base_funcs[] = {
  {"error", lauB_error},
  {"inpairs", lauB_pairs},
  {"pcall", lauB_pcall},
  {"print", lauB_print},
  {"printn", lauB_print},
  {"tonumber", lauB_tonumber},
  {"tostring", lauB_tostring},
  {"type", lauB_type},
  /* placeholders */
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

