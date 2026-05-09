#include "lau.h"
#include "lauxlib.h"

static int id (lau_State *L) {
  return lau_gettop(L);
}


static const struct lauL_Reg funcs[] = {
  {"id", id},
  {NULL, NULL}
};


LAUMOD_API int lauopen_lib2 (lau_State *L) {
  lau_settop(L, 2);
  lau_setglobal(L, "y");  /* y gets 2nd parameter */
  lau_setglobal(L, "x");  /* x gets 1st parameter */
  lauL_newlib(L, funcs);
  return 1;
}


