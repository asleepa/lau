/*
** $Id: ldblib.c $
** Interface from Lau to its debug API
** See Copyright Notice in lau.h
*/

#define ldblib_c
#define LAU_LIB

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lau.h"

#include "lauxlib.h"
#include "laulib.h"
#include "llimits.h"


/*
** The hook table at registry[HOOKKEY] maps threads to their current
** hook function.
*/
static const char *const HOOKKEY = "_HOOKKEY";


/*
** If L1 != L, L1 can be in any state, and therefore there are no
** guarantees about its stack space; any push in L1 must be
** checked.
*/
static void checkstack (lau_State *L, lau_State *L1, int n) {
  if (l_unlikely(L != L1 && !lau_checkstack(L1, n)))
    lauL_error(L, "stack overflow");
}


static int db_getregistry (lau_State *L) {
  lau_pushvalue(L, LAU_REGISTRYINDEX);
  return 1;
}


static int db_getmetatable (lau_State *L) {
  lauL_checkany(L, 1);
  if (!lau_getmetatable(L, 1)) {
    lau_pushnil(L);  /* no metatable */
  }
  return 1;
}


static int db_setmetatable (lau_State *L) {
  int t = lau_type(L, 2);
  lauL_argexpected(L, t == LAU_TNIL || t == LAU_TTABLE, 2, "nil or table");
  lau_settop(L, 2);
  lau_setmetatable(L, 1);
  return 1;  /* return 1st argument */
}


static int db_getuservalue (lau_State *L) {
  int n = (int)lauL_optinteger(L, 2, 1);
  if (lau_type(L, 1) != LAU_TUSERDATA)
    lauL_pushfail(L);
  else if (lau_getiuservalue(L, 1, n) != LAU_TNONE) {
    lau_pushboolean(L, 1);
    return 2;
  }
  return 1;
}


static int db_setuservalue (lau_State *L) {
  int n = (int)lauL_optinteger(L, 3, 1);
  lauL_checktype(L, 1, LAU_TUSERDATA);
  lauL_checkany(L, 2);
  lau_settop(L, 2);
  if (!lau_setiuservalue(L, 1, n))
    lauL_pushfail(L);
  return 1;
}


/*
** Auxiliary function used by several library functions: check for
** an optional thread as function's first argument and set 'arg' with
** 1 if this argument is present (so that functions can skip it to
** access their other arguments)
*/
static lau_State *getthread (lau_State *L, int *arg) {
  if (lau_isthread(L, 1)) {
    *arg = 1;
    return lau_tothread(L, 1);
  }
  else {
    *arg = 0;
    return L;  /* function will operate over current thread */
  }
}


/*
** Variations of 'lau_settable', used by 'db_getinfo' to put results
** from 'lau_getinfo' into result table. Key is always a string;
** value can be a string, an int, or a boolean.
*/
static void settabss (lau_State *L, const char *k, const char *v) {
  lau_pushstring(L, v);
  lau_setfield(L, -2, k);
}

static void settabsi (lau_State *L, const char *k, int v) {
  lau_pushinteger(L, v);
  lau_setfield(L, -2, k);
}

static void settabsb (lau_State *L, const char *k, int v) {
  lau_pushboolean(L, v);
  lau_setfield(L, -2, k);
}


/*
** In function 'db_getinfo', the call to 'lau_getinfo' may push
** results on the stack; later it creates the result table to put
** these objects. Function 'treatstackoption' puts the result from
** 'lau_getinfo' on top of the result table so that it can call
** 'lau_setfield'.
*/
static void treatstackoption (lau_State *L, lau_State *L1, const char *fname) {
  if (L == L1)
    lau_rotate(L, -2, 1);  /* exchange object and table */
  else
    lau_xmove(L1, L, 1);  /* move object to the "main" stack */
  lau_setfield(L, -2, fname);  /* put object into table */
}


/*
** Calls 'lau_getinfo' and collects all results in a new table.
** L1 needs stack space for an optional input (function) plus
** two optional outputs (function and line table) from function
** 'lau_getinfo'.
*/
static int db_getinfo (lau_State *L) {
  lau_Debug ar;
  int arg;
  lau_State *L1 = getthread(L, &arg);
  const char *options = lauL_optstring(L, arg+2, "flnSrtu");
  checkstack(L, L1, 3);
  lauL_argcheck(L, options[0] != '>', arg + 2, "invalid option '>'");
  if (lau_isfunction(L, arg + 1)) {  /* info about a function? */
    options = lau_pushfstring(L, ">%s", options);  /* add '>' to 'options' */
    lau_pushvalue(L, arg + 1);  /* move function to 'L1' stack */
    lau_xmove(L, L1, 1);
  }
  else {  /* stack level */
    if (!lau_getstack(L1, (int)lauL_checkinteger(L, arg + 1), &ar)) {
      lauL_pushfail(L);  /* level out of range */
      return 1;
    }
  }
  if (!lau_getinfo(L1, options, &ar))
    return lauL_argerror(L, arg+2, "invalid option");
  lau_newtable(L);  /* table to collect results */
  if (strchr(options, 'S')) {
    lau_pushlstring(L, ar.source, ar.srclen);
    lau_setfield(L, -2, "source");
    settabss(L, "short_src", ar.short_src);
    settabsi(L, "linedefined", ar.linedefined);
    settabsi(L, "lastlinedefined", ar.lastlinedefined);
    settabss(L, "what", ar.what);
  }
  if (strchr(options, 'l'))
    settabsi(L, "currentline", ar.currentline);
  if (strchr(options, 'u')) {
    settabsi(L, "nups", ar.nups);
    settabsi(L, "nparams", ar.nparams);
    settabsb(L, "isvararg", ar.isvararg);
  }
  if (strchr(options, 'n')) {
    settabss(L, "name", ar.name);
    settabss(L, "namewhat", ar.namewhat);
  }
  if (strchr(options, 'r')) {
    settabsi(L, "ftransfer", ar.ftransfer);
    settabsi(L, "ntransfer", ar.ntransfer);
  }
  if (strchr(options, 't')) {
    settabsb(L, "istailcall", ar.istailcall);
    settabsi(L, "extraargs", ar.extraargs);
  }
  if (strchr(options, 'L'))
    treatstackoption(L, L1, "activelines");
  if (strchr(options, 'f'))
    treatstackoption(L, L1, "func");
  return 1;  /* return table */
}


static int db_getlocal (lau_State *L) {
  int arg;
  lau_State *L1 = getthread(L, &arg);
  int nvar = (int)lauL_checkinteger(L, arg + 2);  /* local-variable index */
  if (lau_isfunction(L, arg + 1)) {  /* function argument? */
    lau_pushvalue(L, arg + 1);  /* push function */
    lau_pushstring(L, lau_getlocal(L, NULL, nvar));  /* push local name */
    return 1;  /* return only name (there is no value) */
  }
  else {  /* stack-level argument */
    lau_Debug ar;
    const char *name;
    int level = (int)lauL_checkinteger(L, arg + 1);
    if (l_unlikely(!lau_getstack(L1, level, &ar)))  /* out of range? */
      return lauL_argerror(L, arg+1, "level out of range");
    checkstack(L, L1, 1);
    name = lau_getlocal(L1, &ar, nvar);
    if (name) {
      lau_xmove(L1, L, 1);  /* move local value */
      lau_pushstring(L, name);  /* push name */
      lau_rotate(L, -2, 1);  /* re-order */
      return 2;
    }
    else {
      lauL_pushfail(L);  /* no name (nor value) */
      return 1;
    }
  }
}


static int db_setlocal (lau_State *L) {
  int arg;
  const char *name;
  lau_State *L1 = getthread(L, &arg);
  lau_Debug ar;
  int level = (int)lauL_checkinteger(L, arg + 1);
  int nvar = (int)lauL_checkinteger(L, arg + 2);
  if (l_unlikely(!lau_getstack(L1, level, &ar)))  /* out of range? */
    return lauL_argerror(L, arg+1, "level out of range");
  lauL_checkany(L, arg+3);
  lau_settop(L, arg+3);
  checkstack(L, L1, 1);
  lau_xmove(L, L1, 1);
  name = lau_setlocal(L1, &ar, nvar);
  if (name == NULL)
    lau_pop(L1, 1);  /* pop value (if not popped by 'lau_setlocal') */
  lau_pushstring(L, name);
  return 1;
}


/*
** get (if 'get' is true) or set an upvalue from a closure
*/
static int auxupvalue (lau_State *L, int get) {
  const char *name;
  int n = (int)lauL_checkinteger(L, 2);  /* upvalue index */
  lauL_checktype(L, 1, LAU_TFUNCTION);  /* closure */
  name = get ? lau_getupvalue(L, 1, n) : lau_setupvalue(L, 1, n);
  if (name == NULL) return 0;
  lau_pushstring(L, name);
  lau_insert(L, -(get+1));  /* no-op if get is false */
  return get + 1;
}


static int db_getupvalue (lau_State *L) {
  return auxupvalue(L, 1);
}


static int db_setupvalue (lau_State *L) {
  lauL_checkany(L, 3);
  return auxupvalue(L, 0);
}


/*
** Check whether a given upvalue from a given closure exists and
** returns its index
*/
static void *checkupval (lau_State *L, int argf, int argnup, int *pnup) {
  void *id;
  int nup = (int)lauL_checkinteger(L, argnup);  /* upvalue index */
  lauL_checktype(L, argf, LAU_TFUNCTION);  /* closure */
  id = lau_upvalueid(L, argf, nup);
  if (pnup) {
    lauL_argcheck(L, id != NULL, argnup, "invalid upvalue index");
    *pnup = nup;
  }
  return id;
}


static int db_upvalueid (lau_State *L) {
  void *id = checkupval(L, 1, 2, NULL);
  if (id != NULL)
    lau_pushlightuserdata(L, id);
  else
    lauL_pushfail(L);
  return 1;
}


static int db_upvaluejoin (lau_State *L) {
  int n1, n2;
  checkupval(L, 1, 2, &n1);
  checkupval(L, 3, 4, &n2);
  lauL_argcheck(L, !lau_iscfunction(L, 1), 1, "Lau function expected");
  lauL_argcheck(L, !lau_iscfunction(L, 3), 3, "Lau function expected");
  lau_upvaluejoin(L, 1, n1, 3, n2);
  return 0;
}


/*
** Call hook function registered at hook table for the current
** thread (if there is one)
*/
static void hookf (lau_State *L, lau_Debug *ar) {
  static const char *const hooknames[] =
    {"call", "return", "line", "count", "tail call"};
  lau_getfield(L, LAU_REGISTRYINDEX, HOOKKEY);
  lau_pushthread(L);
  if (lau_rawget(L, -2) == LAU_TFUNCTION) {  /* is there a hook function? */
    lau_pushstring(L, hooknames[(int)ar->event]);  /* push event name */
    if (ar->currentline >= 0)
      lau_pushinteger(L, ar->currentline);  /* push current line */
    else lau_pushnil(L);
    lau_assert(lau_getinfo(L, "lS", ar));
    lau_call(L, 2, 0);  /* call hook function */
  }
}


/*
** Convert a string mask (for 'sethook') into a bit mask
*/
static int makemask (const char *smask, int count) {
  int mask = 0;
  if (strchr(smask, 'c')) mask |= LAU_MASKCALL;
  if (strchr(smask, 'r')) mask |= LAU_MASKRET;
  if (strchr(smask, 'l')) mask |= LAU_MASKLINE;
  if (count > 0) mask |= LAU_MASKCOUNT;
  return mask;
}


/*
** Convert a bit mask (for 'gethook') into a string mask
*/
static char *unmakemask (int mask, char *smask) {
  int i = 0;
  if (mask & LAU_MASKCALL) smask[i++] = 'c';
  if (mask & LAU_MASKRET) smask[i++] = 'r';
  if (mask & LAU_MASKLINE) smask[i++] = 'l';
  smask[i] = '\0';
  return smask;
}


static int db_sethook (lau_State *L) {
  int arg, mask, count;
  lau_Hook func;
  lau_State *L1 = getthread(L, &arg);
  if (lau_isnoneornil(L, arg+1)) {  /* no hook? */
    lau_settop(L, arg+1);
    func = NULL; mask = 0; count = 0;  /* turn off hooks */
  }
  else {
    const char *smask = lauL_checkstring(L, arg+2);
    lauL_checktype(L, arg+1, LAU_TFUNCTION);
    count = (int)lauL_optinteger(L, arg + 3, 0);
    func = hookf; mask = makemask(smask, count);
  }
  if (!lauL_getsubtable(L, LAU_REGISTRYINDEX, HOOKKEY)) {
    /* table just created; initialize it */
    lau_pushliteral(L, "k");
    lau_setfield(L, -2, "__mode");  /** hooktable.__mode = "k" */
    lau_pushvalue(L, -1);
    lau_setmetatable(L, -2);  /* metatable(hooktable) = hooktable */
  }
  checkstack(L, L1, 1);
  lau_pushthread(L1); lau_xmove(L1, L, 1);  /* key (thread) */
  lau_pushvalue(L, arg + 1);  /* value (hook function) */
  lau_rawset(L, -3);  /* hooktable[L1] = new Lau hook */
  lau_sethook(L1, func, mask, count);
  return 0;
}


static int db_gethook (lau_State *L) {
  int arg;
  lau_State *L1 = getthread(L, &arg);
  char buff[5];
  int mask = lau_gethookmask(L1);
  lau_Hook hook = lau_gethook(L1);
  if (hook == NULL) {  /* no hook? */
    lauL_pushfail(L);
    return 1;
  }
  else if (hook != hookf)  /* external hook? */
    lau_pushliteral(L, "external hook");
  else {  /* hook table must exist */
    lau_getfield(L, LAU_REGISTRYINDEX, HOOKKEY);
    checkstack(L, L1, 1);
    lau_pushthread(L1); lau_xmove(L1, L, 1);
    lau_rawget(L, -2);   /* 1st result = hooktable[L1] */
    lau_remove(L, -2);  /* remove hook table */
  }
  lau_pushstring(L, unmakemask(mask, buff));  /* 2nd result = mask */
  lau_pushinteger(L, lau_gethookcount(L1));  /* 3rd result = count */
  return 3;
}


static int db_debug (lau_State *L) {
  for (;;) {
    char buffer[250];
    lau_writestringerror("%s", "lau_debug> ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL ||
        strcmp(buffer, "cont\n") == 0)
      return 0;
    if (lauL_loadbufferx(L, buffer, strlen(buffer), "=(debug command)", "t") ||
        lau_pcall(L, 0, 0, 0))
      lau_writestringerror("%s\n", lauL_tolstring(L, -1, NULL));
    lau_settop(L, 0);  /* remove eventual returns */
  }
}


static int db_traceback (lau_State *L) {
  int arg;
  lau_State *L1 = getthread(L, &arg);
  const char *msg = lau_tostring(L, arg + 1);
  if (msg == NULL && !lau_isnoneornil(L, arg + 1))  /* non-string 'msg'? */
    lau_pushvalue(L, arg + 1);  /* return it untouched */
  else {
    int level = (int)lauL_optinteger(L, arg + 2, (L == L1) ? 1 : 0);
    lauL_traceback(L, L1, msg, level);
  }
  return 1;
}


static const lauL_Reg dblib[] = {
  {"debug", db_debug},
  {"getuservalue", db_getuservalue},
  {"gethook", db_gethook},
  {"getinfo", db_getinfo},
  {"getlocal", db_getlocal},
  {"getregistry", db_getregistry},
  {"getmetatable", db_getmetatable},
  {"getupvalue", db_getupvalue},
  {"upvaluejoin", db_upvaluejoin},
  {"upvalueid", db_upvalueid},
  {"setuservalue", db_setuservalue},
  {"sethook", db_sethook},
  {"setlocal", db_setlocal},
  {"setmetatable", db_setmetatable},
  {"setupvalue", db_setupvalue},
  {"traceback", db_traceback},
  {NULL, NULL}
};


LAUMOD_API int lauopen_debug (lau_State *L) {
  lauL_newlib(L, dblib);
  return 1;
}

