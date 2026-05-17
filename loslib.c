/*
** $Id: loslib.c $
** Standard Operating System library
** See Copyright Notice in lau.h
*/

#define loslib_c
#define LAU_LIB

#include "lprefix.h"


#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lau.h"

#include "lauxlib.h"
#include "laulib.h"
#include "llimits.h"


/*
** {==================================================================
** List of valid conversion specifiers for the 'strftime' function;
** options are grouped by length; group of length 2 start with '||'.
** ===================================================================
*/
#if !defined(LAU_STRFTIMEOPTIONS)	/* { */

#if defined(LAU_USE_WINDOWS)
#define LAU_STRFTIMEOPTIONS  "aAbBcdHIjmMpSUwWxXyYzZ%" \
    "||" "#c#x#d#H#I#j#m#M#S#U#w#W#y#Y"  /* two-char options */
#elif defined(LAU_USE_C89)  /* C89 (only 1-char options) */
#define LAU_STRFTIMEOPTIONS  "aAbBcdHIjmMpSUwWxXyYZ%"
#else  /* C99 specification */
#define LAU_STRFTIMEOPTIONS  "aAbBcCdDeFgGhHIjmMnprRStTuUVwWxXyYzZ%" \
    "||" "EcECExEXEyEY" "OdOeOHOIOmOMOSOuOUOVOwOWOy"  /* two-char options */
#endif

#endif					/* } */
/* }================================================================== */


/*
** {==================================================================
** Configuration for time-related stuff
** ===================================================================
*/

/*
** type to represent time_t in Lau
*/
#if !defined(LAU_NUMTIME)	/* { */

#define l_timet			lau_Integer
#define l_pushtime(L,t)		lau_pushinteger(L,(lau_Integer)(t))
#define l_gettime(L,arg)	lauL_checkinteger(L, arg)

#else				/* }{ */

#define l_timet			lau_Number
#define l_pushtime(L,t)		lau_pushnumber(L,(lau_Number)(t))
#define l_gettime(L,arg)	lauL_checknumber(L, arg)

#endif				/* } */

/* }================================================================== */


/*
** {==================================================================
** Configuration for 'tmpnam':
** By default, Lau uses tmpnam except when POSIX is available, where
** it uses mkstemp.
** ===================================================================
*/
#if !defined(lau_tmpnam)	/* { */

#if defined(LAU_USE_POSIX)	/* { */

#include <unistd.h>

#define LAU_TMPNAMBUFSIZE	32

#if !defined(LAU_TMPNAMTEMPLATE)
#define LAU_TMPNAMTEMPLATE	"/tmp/lau_XXXXXX"
#endif

#define lau_tmpnam(b,e) { \
        strcpy(b, LAU_TMPNAMTEMPLATE); \
        e = mkstemp(b); \
        if (e != -1) close(e); \
        e = (e == -1); }

#else				/* }{ */

/* ISO C definitions */
#define LAU_TMPNAMBUFSIZE	L_tmpnam
#define lau_tmpnam(b,e)		{ e = (tmpnam(b) == NULL); }

#endif				/* } */

#endif				/* } */
/* }================================================================== */


#if !defined(l_system)
#if defined(LAU_USE_IOS)
/* Despite claiming to be ISO C, iOS does not implement 'system'. */
#define l_system(cmd) ((cmd) == NULL ? 0 : -1)
#else
#define l_system(cmd)	system(cmd)  /* default definition */
#endif
#endif


static int os_execute (lau_State *L) {
  const char *cmd = lauL_optstring(L, 1, NULL);
  int stat;
  errno = 0;
  stat = l_system(cmd);
  if (cmd != NULL)
    return lauL_execresult(L, stat);
  else {
    lau_pushboolean(L, stat);  /* true if there is a shell */
    return 1;
  }
}


static int os_remove (lau_State *L) {
  const char *filename = lauL_checkstring(L, 1);
  errno = 0;
  return lauL_fileresult(L, remove(filename) == 0, filename);
}


static int os_rename (lau_State *L) {
  const char *fromname = lauL_checkstring(L, 1);
  const char *toname = lauL_checkstring(L, 2);
  errno = 0;
  return lauL_fileresult(L, rename(fromname, toname) == 0, NULL);
}


static int os_tmpname (lau_State *L) {
  char buff[LAU_TMPNAMBUFSIZE];
  int err;
  lau_tmpnam(buff, err);
  if (l_unlikely(err))
    return lauL_error(L, "unable to generate a unique filename");
  lau_pushstring(L, buff);
  return 1;
}


static int os_getenv (lau_State *L) {
  lau_pushstring(L, getenv(lauL_checkstring(L, 1)));  /* if NULL push nil */
  return 1;
}


/*
** {======================================================
** Time/Date operations
** { year=%Y, month=%m, day=%d, hour=%H, min=%M, sec=%S,
**   wday=%w+1, yday=%j, isdst=? }
** =======================================================
*/


static time_t l_checktime (lau_State *L, int arg) {
  l_timet t = l_gettime(L, arg);
  lauL_argcheck(L, (time_t)t == t, arg, "time out-of-bounds");
  return (time_t)t;
}


static int os_difftime (lau_State *L) {
  time_t t1 = l_checktime(L, 1);
  time_t t2 = l_checktime(L, 2);
  lau_pushnumber(L, (lau_Number)difftime(t1, t2));
  return 1;
}

/* }====================================================== */


static int os_setlocale (lau_State *L) {
  static const int cat[] = {LC_ALL, LC_COLLATE, LC_CTYPE, LC_MONETARY,
                      LC_NUMERIC, LC_TIME};
  static const char *const catnames[] = {"all", "collate", "ctype", "monetary",
     "numeric", "time", NULL};
  const char *l = lauL_optstring(L, 1, NULL);
  int op = lauL_checkoption(L, 2, "all", catnames);
  lau_pushstring(L, setlocale(cat[op], l));
  return 1;
}


static int os_exit (lau_State *L) {
  int status;
  if (lau_isboolean(L, 1))
    status = (lau_toboolean(L, 1) ? EXIT_SUCCESS : EXIT_FAILURE);
  else
    status = (int)lauL_optinteger(L, 1, EXIT_SUCCESS);
  if (lau_toboolean(L, 2))
    lau_close(L);
  if (L) exit(status);  /* 'if' to avoid warnings for unreachable 'return' */
  return 0;
}


static const lauL_Reg syslib[] = {
  {"difftime",  os_difftime},
  {"execute",   os_execute},
  {"exit",      os_exit},
  {"getenv",    os_getenv},
  {"remove",    os_remove},
  {"rename",    os_rename},
  {"setlocale", os_setlocale},
  {"tmpname",   os_tmpname},
  {NULL, NULL}
};

/* }====================================================== */



LAUMOD_API int lauopen_os (lau_State *L) {
  lauL_newlib(L, syslib);
  return 1;
}

