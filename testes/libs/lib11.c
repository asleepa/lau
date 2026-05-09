#include "lau.h"

/* function from lib1.c */
LAUMOD_API int lib1_export (lau_State *L);

LAUMOD_API int lauopen_lib11 (lau_State *L) {
  return lib1_export(L);
}


