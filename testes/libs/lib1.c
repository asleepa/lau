#include "lau.h"
#include "lauxlib.h"

static int id (lau_State *L) {
  return lau_gettop(L);
}


static const struct lauL_Reg funcs[] = {
  {"id", id},
  {NULL, NULL}
};


/* function used by lib11.c */
LAUMOD_API int lib1_export (lau_State *L) {
  lau_pushstring(L, "exported");
  return 1;
}


LAUMOD_API int onefunction (lau_State *L) {
  lauL_checkversion(L);
  lau_settop(L, 2);
  lau_pushvalue(L, 1);
  return 2;
}


LAUMOD_API int anotherfunc (lau_State *L) {
  lauL_checkversion(L);
  lau_pushfstring(L, "%d%%%d\n", (int)lau_tointeger(L, 1),
                                 (int)lau_tointeger(L, 2));
  return 1;
} 


LAUMOD_API int lauopen_lib1_sub (lau_State *L) {
  lau_setglobal(L, "y");  /* 2nd arg: extra value (file name) */
  lau_setglobal(L, "x");  /* 1st arg: module name */
  lauL_newlib(L, funcs);
  return 1;
}

