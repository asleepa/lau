/*
** $Id: ltasklib.c $
** Library for Tasks and OS-level information
** See Copyright Notice in lau.h
*/

#define ltasklib_c
#define LAU_LIB

#include "lprefix.h"


#include <time.h>

#include "lau.h"
#if defined(LAU_USE_WINDOWS)
  #include <windows.h>
#else
  #include <unistd.h>
#endif

#include "lauxlib.h"
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


#if !defined(l_gmtime)		/* { */
/*
** By default, Lau uses gmtime/localtime, except when POSIX is available,
** where it uses gmtime_r/localtime_r
*/

#if defined(LAU_USE_POSIX)	/* { */

#define l_gmtime(t,r)		gmtime_r(t,r)
#define l_localtime(t,r)	localtime_r(t,r)

#else				/* }{ */

/* ISO C definitions */
#define l_gmtime(t,r)		((void)(r)->tm_sec, gmtime(t))
#define l_localtime(t,r)	((void)(r)->tm_sec, localtime(t))

#endif				/* } */

#endif				/* } */


static const char *checkoption (lau_State *L, const char *conv,
                                size_t convlen, char *buff) {
  const char *option = LAU_STRFTIMEOPTIONS;
  unsigned oplen = 1;  /* length of options being checked */
  for (; *option != '\0' && oplen <= convlen; option += oplen) {
    if (*option == '|')  /* next block? */
      oplen++;  /* will check options with next length (+1) */
    else if (memcmp(conv, option, oplen) == 0) {  /* match? */
      memcpy(buff, conv, oplen);  /* copy valid option to buffer */
      buff[oplen] = '\0';
      return conv + oplen;  /* return next item */
    }
  }
  lauL_argerror(L, 1,
    lau_pushfstring(L, "invalid conversion specifier '%%%s'", conv));
  return conv;  /* to avoid warnings */
}


static time_t l_checktime (lau_State *L, int arg) {
  l_timet t = l_gettime(L, arg);
  lauL_argcheck(L, (time_t)t == t, arg, "time out-of-bounds");
  return (time_t)t;
}


/* maximum size for an individual 'strftime' item */
#define SIZETIMEFMT	250

/* }================================================================== */


/*
** {======================================================
** Time/Date operations
** { year=%Y, month=%m, day=%d, hour=%H, min=%M, sec=%S,
**   wday=%w+1, yday=%j, isdst=? }
** =======================================================
*/

/*
** About the overflow check: an overflow cannot occur when time
** is represented by a lau_Integer, because either lau_Integer is
** large enough to represent all int fields or it is not large enough
** to represent a time that cause a field to overflow.  However, if
** times are represented as doubles and lau_Integer is int, then the
** time 0x1.e1853b0d184f6p+55 would cause an overflow when adding 1900
** to compute the year.
*/
static void setfield (lau_State *L, const char *key, int value, int delta) {
  #if (defined(LAU_NUMTIME) && LAU_MAXINTEGER <= INT_MAX)
    if (l_unlikely(value > LAU_MAXINTEGER - delta))
      lauL_error(L, "field '%s' is out-of-bound", key);
  #endif
  lau_pushinteger(L, (lau_Integer)value + delta);
  lau_setfield(L, -2, key);
}


static void setboolfield (lau_State *L, const char *key, int value) {
  if (value < 0)  /* undefined? */
    return;  /* does not set field */
  lau_pushboolean(L, value);
  lau_setfield(L, -2, key);
}


/*
** Set all fields from structure 'tm' in the table on top of the stack
*/
static void setallfields (lau_State *L, struct tm *stm) {
  setfield(L, "year", stm->tm_year, 1900);
  setfield(L, "month", stm->tm_mon, 1);
  setfield(L, "day", stm->tm_mday, 0);
  setfield(L, "hour", stm->tm_hour, 0);
  setfield(L, "min", stm->tm_min, 0);
  setfield(L, "sec", stm->tm_sec, 0);
  setfield(L, "yday", stm->tm_yday, 1);
  setfield(L, "wday", stm->tm_wday, 1);
  setboolfield(L, "isdst", stm->tm_isdst);
}


static int getboolfield (lau_State *L, const char *key) {
  int res;
  res = (lau_getfield(L, -1, key) == LAU_TNIL) ? -1 : lau_toboolean(L, -1);
  lau_pop(L, 1);
  return res;
}


static int getfield (lau_State *L, const char *key, int d, int delta) {
  int isnum;
  int t = lau_getfield(L, -1, key);  /* get field and its type */
  lau_Integer res = lau_tointegerx(L, -1, &isnum);
  if (!isnum) {  /* field is not an integer? */
    if (l_unlikely(t != LAU_TNIL))  /* some other value? */
      return lauL_error(L, "field '%s' is not an integer", key);
    else if (l_unlikely(d < 0))  /* absent field; no default? */
      return lauL_error(L, "field '%s' missing in date table", key);
    res = d;
  }
  else {
    if (!(res >= 0 ? res - delta <= INT_MAX : INT_MIN + delta <= res))
      return lauL_error(L, "field '%s' is out-of-bound", key);
    res -= delta;
  }
  lau_pop(L, 1);
  return (int)res;
}


static int task_date (lau_State *L) {
  size_t slen;
  const char *s = lauL_optlstring(L, 1, "%c", &slen);
  time_t t = lauL_opt(L, l_checktime, 2, time(NULL));
  const char *se = s + slen;  /* 's' end */
  struct tm tmr, *stm;
  if (*s == '!') {  /* UTC? */
    stm = l_gmtime(&t, &tmr);
    s++;  /* skip '!' */
  }
  else
    stm = l_localtime(&t, &tmr);
  if (stm == NULL)  /* invalid date? */
    return lauL_error(L,
                 "date result cannot be represented in this installation");
  if (strcmp(s, "*t") == 0) {
    lau_createtable(L, 0, 9);  /* 9 = number of fields */
    setallfields(L, stm);
  }
  else {
    char cc[4];  /* buffer for individual conversion specifiers */
    lauL_Buffer b;
    cc[0] = '%';
    lauL_buffinit(L, &b);
    while (s < se) {
      if (*s != '%')  /* not a conversion specifier? */
        lauL_addchar(&b, *s++);
      else {
        size_t reslen;
        char *buff = lauL_prepbuffsize(&b, SIZETIMEFMT);
        s++;  /* skip '%' */
        /* copy specifier to 'cc' */
        s = checkoption(L, s, ct_diff2sz(se - s), cc + 1);
        reslen = strftime(buff, SIZETIMEFMT, cc, stm);
        lauL_addsize(&b, reslen);
      }
    }
    lauL_pushresult(&b);
  }
  return 1;
}


static int task_time (lau_State *L) {
  time_t t;
  if (lau_isnoneornil(L, 1))  /* called without args? */
    t = time(NULL);  /* get current time */
  else {
    struct tm ts;
    lauL_checktype(L, 1, LAU_TTABLE);
    lau_settop(L, 1);  /* make sure table is at the top */
    ts.tm_year = getfield(L, "year", -1, 1900);
    ts.tm_mon = getfield(L, "month", -1, 1);
    ts.tm_mday = getfield(L, "day", -1, 0);
    ts.tm_hour = getfield(L, "hour", 12, 0);
    ts.tm_min = getfield(L, "min", 0, 0);
    ts.tm_sec = getfield(L, "sec", 0, 0);
    ts.tm_isdst = getboolfield(L, "isdst");
    t = mktime(&ts);
    setallfields(L, &ts);  /* update fields with normalized values */
  }
  if (t != (time_t)(l_timet)t || t == (time_t)(-1))
    return lauL_error(L,
                  "time result cannot be represented in this installation");
  l_pushtime(L, t);
  return 1;
}

static int task_clock (lau_State *L) {
  lau_pushnumber(L, ((lau_Number)clock())/(lau_Number)CLOCKS_PER_SEC);
  return 1;
}

static int task_wait (lau_State *L) {
  double sec = lauL_checknumber(L, 1);
  #if defined(LAU_USE_WINDOWS)
    Sleep((DWORD)(sec * (1000)));
  #else
    struct timespec ts;
    ts.tv_sec = (time_t)sec;
    ts.tv_nsec = (long)((seconds - ts.tv_sec) * (1000000000));
    nanosleep(&ts, NULL);
  #endif
  return 0;
}

/* }====================================================== */


static const lauL_Reg task_funcs[] = {
  {"date", task_date},
  {"clock", task_clock},
  {"time", task_time},
  {"wait", task_wait},
  {NULL, NULL}
};


LAUMOD_API int lauopen_task (lau_State *L) {
  lauL_newlib(L, task_funcs);
  return 1;
}

