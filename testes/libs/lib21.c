#include "lau.h"


int lauopen_lib2 (lau_State *L);

LAUMOD_API int lauopen_lib21 (lau_State *L) {
  return lauopen_lib2(L);
}


