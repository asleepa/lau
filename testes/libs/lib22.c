/* implementation for lib2-v2 */

#include <string.h>

#include "lau.h"
#include "lauxlib.h"

static int id (lau_State *L) {
  lau_pushboolean(L, 1);
  lau_insert(L, 1);
  return lau_gettop(L);
}


struct STR {
  void *ud;
  lau_Alloc allocf;
};


static void *t_freestr (void *ud, void *ptr, size_t osize, size_t nsize) {
  struct STR *blk = (struct STR*)ptr - 1;
  blk->allocf(blk->ud, blk, sizeof(struct STR) + osize, 0);
  return NULL;
}


static int newstr (lau_State *L) {
  size_t len;
  const char *str = lauL_checklstring(L, 1, &len);
  void *ud;
  lau_Alloc allocf = lau_getallocf(L, &ud);
  struct STR *blk = (struct STR*)allocf(ud, NULL, 0,
                                        len + 1 + sizeof(struct STR));
  if (blk == NULL) {  /* allocation error? */
    lau_pushliteral(L, "not enough memory");
    lau_error(L);  /* raise a memory error */
  }
  blk->ud = ud;  blk->allocf = allocf;
  memcpy(blk + 1, str, len + 1);
  lau_pushexternalstring(L, (char *)(blk + 1), len, t_freestr, L);
  return 1;
}


/*
** Create an external string and keep it in the registry, so that it
** will test that the library code is still available (to deallocate
** this string) when closing the state.
*/
static void initstr (lau_State *L) {
  lau_pushcfunction(L, newstr);
  lau_pushstring(L,
     "012345678901234567890123456789012345678901234567890123456789");
  lau_call(L, 1, 1);  /* call newstr("0123...") */
  lauL_ref(L, LAU_REGISTRYINDEX);  /* keep string in the registry */
}


static const struct lauL_Reg funcs[] = {
  {"id", id},
  {"newstr", newstr},
  {NULL, NULL}
};


LAUMOD_API int lauopen_lib2 (lau_State *L) {
  lau_settop(L, 2);
  lau_setglobal(L, "y");  /* y gets 2nd parameter */
  lau_setglobal(L, "x");  /* x gets 1st parameter */
  initstr(L);
  lauL_newlib(L, funcs);
  return 1;
}


