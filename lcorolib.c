/*
** $Id: lcorolib.c $
** Coroutine Library
** See Copyright Notice in lau.h
*/

#define lcorolib_c
#define LAU_LIB

#include "lprefix.h"


#include <stdlib.h>

#include "lau.h"

#include "lauxlib.h"
#include "laulib.h"
#include "llimits.h"


static lau_State *getco (lau_State *L) {
  lau_State *co = lau_tothread(L, 1);
  lauL_argexpected(L, co, 1, "thread");
  return co;
}


/*
** Resumes a coroutine. Returns the number of results for non-error
** cases or -1 for errors.
*/
static int auxresume (lau_State *L, lau_State *co, int narg) {
  int status, nres;
  if (l_unlikely(!lau_checkstack(co, narg))) {
    lau_pushliteral(L, "too many arguments to resume");
    return -1;  /* error flag */
  }
  lau_xmove(L, co, narg);
  status = lau_resume(co, L, narg, &nres);
  if (l_likely(status == LAU_OK || status == LAU_YIELD)) {
    if (l_unlikely(!lau_checkstack(L, nres + 1))) {
      lau_pop(co, nres);  /* remove results anyway */
      lau_pushliteral(L, "too many results to resume");
      return -1;  /* error flag */
    }
    lau_xmove(co, L, nres);  /* move yielded values */
    return nres;
  }
  else {
    lau_xmove(co, L, 1);  /* move error message */
    return -1;  /* error flag */
  }
}


static int lauB_coresume (lau_State *L) {
  lau_State *co = getco(L);
  int r;
  r = auxresume(L, co, lau_gettop(L) - 1);
  if (l_unlikely(r < 0)) {
    lau_pushboolean(L, 0);
    lau_insert(L, -2);
    return 2;  /* return false + error message */
  }
  else {
    lau_pushboolean(L, 1);
    lau_insert(L, -(r + 1));
    return r + 1;  /* return true + 'resume' returns */
  }
}


static int lauB_auxwrap (lau_State *L) {
  lau_State *co = lau_tothread(L, lau_upvalueindex(1));
  int r = auxresume(L, co, lau_gettop(L));
  if (l_unlikely(r < 0)) {  /* error? */
    int stat = lau_status(co);
    if (stat != LAU_OK && stat != LAU_YIELD) {  /* error in the coroutine? */
      stat = lau_closethread(co, L);  /* close its tbc variables */
      lau_assert(stat != LAU_OK);
      lau_xmove(co, L, 1);  /* move error message to the caller */
    }
    if (stat != LAU_ERRMEM &&  /* not a memory error and ... */
        lau_type(L, -1) == LAU_TSTRING) {  /* ... error object is a string? */
      lauL_where(L, 1);  /* add extra info, if available */
      lau_insert(L, -2);
      lau_concat(L, 2);
    }
    return lau_error(L);  /* propagate error */
  }
  return r;
}


static int lauB_cocreate (lau_State *L) {
  lau_State *NL;
  lauL_checktype(L, 1, LAU_TFUNCTION);
  NL = lau_newthread(L);
  lau_pushvalue(L, 1);  /* move function to top */
  lau_xmove(L, NL, 1);  /* move function from L to NL */
  return 1;
}


static int lauB_cowrap (lau_State *L) {
  lauB_cocreate(L);
  lau_pushcclosure(L, lauB_auxwrap, 1);
  return 1;
}


static int lauB_yield (lau_State *L) {
  return lau_yield(L, lau_gettop(L));
}


#define COS_RUN		0
#define COS_DEAD	1
#define COS_YIELD	2
#define COS_NORM	3


static const char *const statname[] =
  {"running", "dead", "suspended", "normal"};


static int auxstatus (lau_State *L, lau_State *co) {
  if (L == co) return COS_RUN;
  else {
    switch (lau_status(co)) {
      case LAU_YIELD:
        return COS_YIELD;
      case LAU_OK: {
        lau_Debug ar;
        if (lau_getstack(co, 0, &ar))  /* does it have frames? */
          return COS_NORM;  /* it is running */
        else if (lau_gettop(co) == 0)
            return COS_DEAD;
        else
          return COS_YIELD;  /* initial state */
      }
      default:  /* some error occurred */
        return COS_DEAD;
    }
  }
}


static int lauB_costatus (lau_State *L) {
  lau_State *co = getco(L);
  lau_pushstring(L, statname[auxstatus(L, co)]);
  return 1;
}


static lau_State *getoptco (lau_State *L) {
  return (lau_isnone(L, 1) ? L : getco(L));
}


static int lauB_yieldable (lau_State *L) {
  lau_State *co = getoptco(L);
  lau_pushboolean(L, lau_isyieldable(co));
  return 1;
}


static int lauB_corunning (lau_State *L) {
  int ismain = lau_pushthread(L);
  lau_pushboolean(L, ismain);
  return 2;
}


static int lauB_close (lau_State *L) {
  lau_State *co = getoptco(L);
  int status = auxstatus(L, co);
  switch (status) {
    case COS_DEAD: case COS_YIELD: {
      status = lau_closethread(co, L);
      if (status == LAU_OK) {
        lau_pushboolean(L, 1);
        return 1;
      }
      else {
        lau_pushboolean(L, 0);
        lau_xmove(co, L, 1);  /* move error message */
        return 2;
      }
    }
    case COS_NORM:
      return lauL_error(L, "cannot close a %s coroutine", statname[status]);
    case COS_RUN:
      lau_geti(L, LAU_REGISTRYINDEX, LAU_RIDX_MAINTHREAD);  /* get main */
      if (lau_tothread(L, -1) == co)
        return lauL_error(L, "cannot close main thread");
      lau_closethread(co, L);  /* close itself */
      /* previous call does not return *//* FALLTHROUGH */
    default:
      lau_assert(0);
      return 0;
  }
}


static const lauL_Reg co_funcs[] = {
  {"create", lauB_cocreate},
  {"resume", lauB_coresume},
  {"running", lauB_corunning},
  {"status", lauB_costatus},
  {"wrap", lauB_cowrap},
  {"yield", lauB_yield},
  {"isyieldable", lauB_yieldable},
  {"close", lauB_close},
  {NULL, NULL}
};



LAUMOD_API int lauopen_coroutine (lau_State *L) {
  lauL_newlib(L, co_funcs);
  return 1;
}

