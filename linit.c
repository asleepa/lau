/*
** $Id: linit.c $
** Initialization of libraries for lau.c and other clients
** See Copyright Notice in lau.h
*/


#define linit_c
#define LAU_LIB


#include "lprefix.h"


#include <stddef.h>

#include "lau.h"

#include "laulib.h"
#include "lauxlib.h"
#include "llimits.h"


/*
** Standard Libraries. (Must be listed in the same ORDER of their
** respective constants LAU_<libname>K.)
*/
static const lauL_Reg stdlibs[] = {
  {LAU_GNAME, lauopen_base},
  {LAU_LOADLIBNAME, lauopen_package},
  {LAU_COLIBNAME, lauopen_coroutine},
  {LAU_DBLIBNAME, lauopen_debug},
  {LAU_IOLIBNAME, lauopen_io},
  {LAU_MATHLIBNAME, lauopen_math},
  {LAU_OSLIBNAME, lauopen_os},
  {LAU_STRLIBNAME, lauopen_string},
  {LAU_TABLIBNAME, lauopen_table},
  {LAU_UTF8LIBNAME, lauopen_utf8},
  {NULL, NULL}
};


/*
** require and preload selected standard libraries
*/
LAULIB_API void lauL_openselectedlibs (lau_State *L, int load, int preload) {
  int mask;
  const lauL_Reg *lib;
  lauL_getsubtable(L, LAU_REGISTRYINDEX, LAU_PRELOAD_TABLE);
  for (lib = stdlibs, mask = 1; lib->name != NULL; lib++, mask <<= 1) {
    if (load & mask) {  /* selected? */
      lauL_requiref(L, lib->name, lib->func, 1);  /* require library */
      lau_pop(L, 1);  /* remove result from the stack */
    }
    else if (preload & mask) {  /* selected? */
      lau_pushcfunction(L, lib->func);
      lau_setfield(L, -2, lib->name);  /* add library to PRELOAD table */
    }
  }
  lau_assert((mask >> 1) == LAU_UTF8LIBK);
  lau_pop(L, 1);  /* remove PRELOAD table */
}

