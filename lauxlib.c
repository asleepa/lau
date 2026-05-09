/*
** $Id: lauxlib.c $
** Auxiliary functions for building Lau libraries
** See Copyright Notice in lau.h
*/

#define lauxlib_c
#define LAU_LIB

#include "lprefix.h"


#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
** This file uses only the official API of Lau.
** Any function declared here could be written as an application function.
*/

#include "lau.h"

#include "lauxlib.h"
#include "llimits.h"


/*
** {======================================================
** Traceback
** =======================================================
*/


#define LEVELS1	10	/* size of the first part of the stack */
#define LEVELS2	11	/* size of the second part of the stack */



/*
** Search for 'objidx' in table at index -1. ('objidx' must be an
** absolute index.) Return 1 + string at top if it found a good name.
*/
static int findfield (lau_State *L, int objidx, int level) {
  if (level == 0 || !lau_istable(L, -1))
    return 0;  /* not found */
  lau_pushnil(L);  /* start 'next' loop */
  while (lau_next(L, -2)) {  /* for each pair in table */
    if (lau_type(L, -2) == LAU_TSTRING) {  /* ignore non-string keys */
      if (lau_rawequal(L, objidx, -1)) {  /* found object? */
        lau_pop(L, 1);  /* remove value (but keep name) */
        return 1;
      }
      else if (findfield(L, objidx, level - 1)) {  /* try recursively */
        /* stack: lib_name, lib_table, field_name (top) */
        lau_pushliteral(L, ".");  /* place '.' between the two names */
        lau_replace(L, -3);  /* (in the slot occupied by table) */
        lau_concat(L, 3);  /* lib_name.field_name */
        return 1;
      }
    }
    lau_pop(L, 1);  /* remove value */
  }
  return 0;  /* not found */
}


/*
** Search for a name for a function in all loaded modules
*/
static int pushglobalfuncname (lau_State *L, lau_Debug *ar) {
  int top = lau_gettop(L);
  lau_getinfo(L, "f", ar);  /* push function */
  lau_getfield(L, LAU_REGISTRYINDEX, LAU_LOADED_TABLE);
  lauL_checkstack(L, 6, "not enough stack");  /* slots for 'findfield' */
  if (findfield(L, top + 1, 2)) {
    const char *name = lau_tostring(L, -1);
    if (strncmp(name, LAU_GNAME ".", 3) == 0) {  /* name start with '_G.'? */
      lau_pushstring(L, name + 3);  /* push name without prefix */
      lau_remove(L, -2);  /* remove original name */
    }
    lau_copy(L, -1, top + 1);  /* copy name to proper place */
    lau_settop(L, top + 1);  /* remove table "loaded" and name copy */
    return 1;
  }
  else {
    lau_settop(L, top);  /* remove function and global table */
    return 0;
  }
}


static void pushfuncname (lau_State *L, lau_Debug *ar) {
  if (*ar->namewhat != '\0')  /* is there a name from code? */
    lau_pushfstring(L, "%s '%s'", ar->namewhat, ar->name);  /* use it */
  else if (*ar->what == 'm')  /* main? */
      lau_pushliteral(L, "main chunk");
  else if (pushglobalfuncname(L, ar)) {  /* try a global name */
    lau_pushfstring(L, "function '%s'", lau_tostring(L, -1));
    lau_remove(L, -2);  /* remove name */
  }
  else if (*ar->what != 'C')  /* for Lau functions, use <file:line> */
    lau_pushfstring(L, "function <%s:%d>", ar->short_src, ar->linedefined);
  else  /* nothing left... */
    lau_pushliteral(L, "?");
}


static int lastlevel (lau_State *L) {
  lau_Debug ar;
  int li = 1, le = 1;
  /* find an upper bound */
  while (lau_getstack(L, le, &ar)) { li = le; le *= 2; }
  /* do a binary search */
  while (li < le) {
    int m = (li + le)/2;
    if (lau_getstack(L, m, &ar)) li = m + 1;
    else le = m;
  }
  return le - 1;
}


LAULIB_API void lauL_traceback (lau_State *L, lau_State *L1,
                                const char *msg, int level) {
  lauL_Buffer b;
  lau_Debug ar;
  int last = lastlevel(L1);
  int limit2show = (last - level > LEVELS1 + LEVELS2) ? LEVELS1 : -1;
  lauL_buffinit(L, &b);
  if (msg) {
    lauL_addstring(&b, msg);
    lauL_addchar(&b, '\n');
  }
  lauL_addstring(&b, "stack traceback:");
  while (lau_getstack(L1, level++, &ar)) {
    if (limit2show-- == 0) {  /* too many levels? */
      int n = last - level - LEVELS2 + 1;  /* number of levels to skip */
      lau_pushfstring(L, "\n\t...\t(skipping %d levels)", n);
      lauL_addvalue(&b);  /* add warning about skip */
      level += n;  /* and skip to last levels */
    }
    else {
      lau_getinfo(L1, "Slnt", &ar);
      if (ar.currentline <= 0)
        lau_pushfstring(L, "\n\t%s: in ", ar.short_src);
      else
        lau_pushfstring(L, "\n\t%s:%d: in ", ar.short_src, ar.currentline);
      lauL_addvalue(&b);
      pushfuncname(L, &ar);
      lauL_addvalue(&b);
      if (ar.istailcall)
        lauL_addstring(&b, "\n\t(...tail calls...)");
    }
  }
  lauL_pushresult(&b);
}

/* }====================================================== */


/*
** {======================================================
** Error-report functions
** =======================================================
*/

LAULIB_API int lauL_argerror (lau_State *L, int arg, const char *extramsg) {
  lau_Debug ar;
  const char *argword;
  if (!lau_getstack(L, 0, &ar))  /* no stack frame? */
    return lauL_error(L, "bad argument #%d (%s)", arg, extramsg);
  lau_getinfo(L, "nt", &ar);
  if (arg <= ar.extraargs)  /* error in an extra argument? */
    argword =  "extra argument";
  else {
    arg -= ar.extraargs;  /* do not count extra arguments */
    if (strcmp(ar.namewhat, "method") == 0) {  /* colon syntax? */
      arg--;  /* do not count (extra) self argument */
      if (arg == 0)  /* error in self argument? */
        return lauL_error(L, "calling '%s' on bad self (%s)",
                               ar.name, extramsg);
      /* else go through; error in a regular argument */
    }
    argword = "argument";
  }
  if (ar.name == NULL)
    ar.name = (pushglobalfuncname(L, &ar)) ? lau_tostring(L, -1) : "?";
  return lauL_error(L, "bad %s #%d to '%s' (%s)",
                       argword, arg, ar.name, extramsg);
}


LAULIB_API int lauL_typeerror (lau_State *L, int arg, const char *tname) {
  const char *msg;
  const char *typearg;  /* name for the type of the actual argument */
  if (lauL_getmetafield(L, arg, "__name") == LAU_TSTRING)
    typearg = lau_tostring(L, -1);  /* use the given type name */
  else if (lau_type(L, arg) == LAU_TLIGHTUSERDATA)
    typearg = "light userdata";  /* special name for messages */
  else
    typearg = lauL_typename(L, arg);  /* standard name */
  msg = lau_pushfstring(L, "%s expected, got %s", tname, typearg);
  return lauL_argerror(L, arg, msg);
}


static void tag_error (lau_State *L, int arg, int tag) {
  lauL_typeerror(L, arg, lau_typename(L, tag));
}


/*
** The use of 'lau_pushfstring' ensures this function does not
** need reserved stack space when called.
*/
LAULIB_API void lauL_where (lau_State *L, int level) {
  lau_Debug ar;
  if (lau_getstack(L, level, &ar)) {  /* check function at level */
    lau_getinfo(L, "Sl", &ar);  /* get info about it */
    if (ar.currentline > 0) {  /* is there info? */
      lau_pushfstring(L, "%s:%d: ", ar.short_src, ar.currentline);
      return;
    }
  }
  lau_pushfstring(L, "");  /* else, no information available... */
}


/*
** Again, the use of 'lau_pushvfstring' ensures this function does
** not need reserved stack space when called. (At worst, it generates
** a memory error instead of the given message.)
*/
LAULIB_API int lauL_error (lau_State *L, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  lauL_where(L, 1);
  lau_pushvfstring(L, fmt, argp);
  va_end(argp);
  lau_concat(L, 2);
  return lau_error(L);
}


LAULIB_API int lauL_fileresult (lau_State *L, int stat, const char *fname) {
  int en = errno;  /* calls to Lau API may change this value */
  if (stat) {
    lau_pushboolean(L, 1);
    return 1;
  }
  else {
    const char *msg;
    lauL_pushfail(L);
    msg = (en != 0) ? strerror(en) : "(no extra info)";
    if (fname)
      lau_pushfstring(L, "%s: %s", fname, msg);
    else
      lau_pushstring(L, msg);
    lau_pushinteger(L, en);
    return 3;
  }
}


#if !defined(l_inspectstat)	/* { */

#if defined(LAU_USE_POSIX)

#include <sys/wait.h>

/*
** use appropriate macros to interpret 'pclose' return status
*/
#define l_inspectstat(stat,what)  \
   if (WIFEXITED(stat)) { stat = WEXITSTATUS(stat); } \
   else if (WIFSIGNALED(stat)) { stat = WTERMSIG(stat); what = "signal"; }

#else

#define l_inspectstat(stat,what)  /* no op */

#endif

#endif				/* } */


LAULIB_API int lauL_execresult (lau_State *L, int stat) {
  if (stat != 0 && errno != 0)  /* error with an 'errno'? */
    return lauL_fileresult(L, 0, NULL);
  else {
    const char *what = "exit";  /* type of termination */
    l_inspectstat(stat, what);  /* interpret result */
    if (*what == 'e' && stat == 0)  /* successful termination? */
      lau_pushboolean(L, 1);
    else
      lauL_pushfail(L);
    lau_pushstring(L, what);
    lau_pushinteger(L, stat);
    return 3;  /* return true/fail,what,code */
  }
}

/* }====================================================== */



/*
** {======================================================
** Userdata's metatable manipulation
** =======================================================
*/

LAULIB_API int lauL_newmetatable (lau_State *L, const char *tname) {
  if (lauL_getmetatable(L, tname) != LAU_TNIL)  /* name already in use? */
    return 0;  /* leave previous value on top, but return 0 */
  lau_pop(L, 1);
  lau_createtable(L, 0, 2);  /* create metatable */
  lau_pushstring(L, tname);
  lau_setfield(L, -2, "__name");  /* metatable.__name = tname */
  lau_pushvalue(L, -1);
  lau_setfield(L, LAU_REGISTRYINDEX, tname);  /* registry.name = metatable */
  return 1;
}


LAULIB_API void lauL_setmetatable (lau_State *L, const char *tname) {
  lauL_getmetatable(L, tname);
  lau_setmetatable(L, -2);
}


LAULIB_API void *lauL_testudata (lau_State *L, int ud, const char *tname) {
  void *p = lau_touserdata(L, ud);
  if (p != NULL) {  /* value is a userdata? */
    if (lau_getmetatable(L, ud)) {  /* does it have a metatable? */
      lauL_getmetatable(L, tname);  /* get correct metatable */
      if (!lau_rawequal(L, -1, -2))  /* not the same? */
        p = NULL;  /* value is a userdata with wrong metatable */
      lau_pop(L, 2);  /* remove both metatables */
      return p;
    }
  }
  return NULL;  /* value is not a userdata with a metatable */
}


LAULIB_API void *lauL_checkudata (lau_State *L, int ud, const char *tname) {
  void *p = lauL_testudata(L, ud, tname);
  lauL_argexpected(L, p != NULL, ud, tname);
  return p;
}

/* }====================================================== */


/*
** {======================================================
** Argument check functions
** =======================================================
*/

LAULIB_API int lauL_checkoption (lau_State *L, int arg, const char *def,
                                 const char *const lst[]) {
  const char *name = (def) ? lauL_optstring(L, arg, def) :
                             lauL_checkstring(L, arg);
  int i;
  for (i=0; lst[i]; i++)
    if (strcmp(lst[i], name) == 0)
      return i;
  return lauL_argerror(L, arg,
                       lau_pushfstring(L, "invalid option '%s'", name));
}


/*
** Ensures the stack has at least 'space' extra slots, raising an error
** if it cannot fulfill the request. (The error handling needs a few
** extra slots to format the error message. In case of an error without
** this extra space, Lau will generate the same 'stack overflow' error,
** but without 'msg'.)
*/
LAULIB_API void lauL_checkstack (lau_State *L, int space, const char *msg) {
  if (l_unlikely(!lau_checkstack(L, space))) {
    if (msg)
      lauL_error(L, "stack overflow (%s)", msg);
    else
      lauL_error(L, "stack overflow");
  }
}


LAULIB_API void lauL_checktype (lau_State *L, int arg, int t) {
  if (l_unlikely(lau_type(L, arg) != t))
    tag_error(L, arg, t);
}


LAULIB_API void lauL_checkany (lau_State *L, int arg) {
  if (l_unlikely(lau_type(L, arg) == LAU_TNONE))
    lauL_argerror(L, arg, "value expected");
}


LAULIB_API const char *lauL_checklstring (lau_State *L, int arg, size_t *len) {
  const char *s = lau_tolstring(L, arg, len);
  if (l_unlikely(!s)) tag_error(L, arg, LAU_TSTRING);
  return s;
}


LAULIB_API const char *lauL_optlstring (lau_State *L, int arg,
                                        const char *def, size_t *len) {
  if (lau_isnoneornil(L, arg)) {
    if (len)
      *len = (def ? strlen(def) : 0);
    return def;
  }
  else return lauL_checklstring(L, arg, len);
}


LAULIB_API lau_Number lauL_checknumber (lau_State *L, int arg) {
  int isnum;
  lau_Number d = lau_tonumberx(L, arg, &isnum);
  if (l_unlikely(!isnum))
    tag_error(L, arg, LAU_TNUMBER);
  return d;
}


LAULIB_API lau_Number lauL_optnumber (lau_State *L, int arg, lau_Number def) {
  return lauL_opt(L, lauL_checknumber, arg, def);
}


static void interror (lau_State *L, int arg) {
  if (lau_isnumber(L, arg))
    lauL_argerror(L, arg, "number has no integer representation");
  else
    tag_error(L, arg, LAU_TNUMBER);
}


LAULIB_API lau_Integer lauL_checkinteger (lau_State *L, int arg) {
  int isnum;
  lau_Integer d = lau_tointegerx(L, arg, &isnum);
  if (l_unlikely(!isnum)) {
    interror(L, arg);
  }
  return d;
}


LAULIB_API lau_Integer lauL_optinteger (lau_State *L, int arg,
                                                      lau_Integer def) {
  return lauL_opt(L, lauL_checkinteger, arg, def);
}

/* }====================================================== */


/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

/* userdata to box arbitrary data */
typedef struct UBox {
  void *box;
  size_t bsize;
} UBox;


/* Resize the buffer used by a box. Optimize for the common case of
** resizing to the old size. (For instance, __gc will resize the box
** to 0 even after it was closed. 'pushresult' may also resize it to a
** final size that is equal to the one set when the buffer was created.)
*/
static void *resizebox (lau_State *L, int idx, size_t newsize) {
  UBox *box = (UBox *)lau_touserdata(L, idx);
  if (box->bsize == newsize)  /* not changing size? */
    return box->box;  /* keep the buffer */
  else {
    void *ud;
    lau_Alloc allocf = lau_getallocf(L, &ud);
    void *temp = allocf(ud, box->box, box->bsize, newsize);
    if (l_unlikely(temp == NULL && newsize > 0)) {  /* allocation error? */
      lau_pushliteral(L, "not enough memory");
      lau_error(L);  /* raise a memory error */
    }
    box->box = temp;
    box->bsize = newsize;
    return temp;
  }
}


static int boxgc (lau_State *L) {
  resizebox(L, 1, 0);
  return 0;
}


static const lauL_Reg boxmt[] = {  /* box metamethods */
  {"__gc", boxgc},
  {"__close", boxgc},
  {NULL, NULL}
};


static void newbox (lau_State *L) {
  UBox *box = (UBox *)lau_newuserdatauv(L, sizeof(UBox), 0);
  box->box = NULL;
  box->bsize = 0;
  if (lauL_newmetatable(L, "_UBOX*"))  /* creating metatable? */
    lauL_setfuncs(L, boxmt, 0);  /* set its metamethods */
  lau_setmetatable(L, -2);
}


/*
** check whether buffer is using a userdata on the stack as a temporary
** buffer
*/
#define buffonstack(B)	((B)->b != (B)->init.b)


/*
** Whenever buffer is accessed, slot 'idx' must either be a box (which
** cannot be NULL) or it is a placeholder for the buffer.
*/
#define checkbufferlevel(B,idx)  \
  lau_assert(buffonstack(B) ? lau_touserdata(B->L, idx) != NULL  \
                            : lau_touserdata(B->L, idx) == (void*)B)


/*
** Compute new size for buffer 'B', enough to accommodate extra 'sz'
** bytes plus one for a terminating zero.
*/
static size_t newbuffsize (lauL_Buffer *B, size_t sz) {
  size_t newsize = B->size;
  if (l_unlikely(sz >= MAX_SIZE - B->n))
    return cast_sizet(lauL_error(B->L, "resulting string too large"));
  /* else  B->n + sz + 1 <= MAX_SIZE */
  if (newsize <= MAX_SIZE/3 * 2)  /* no overflow? */
    newsize += (newsize >> 1);  /* new size *= 1.5 */
  if (newsize < B->n + sz + 1)  /* not big enough? */
    newsize = B->n + sz + 1;
  return newsize;
}


/*
** Returns a pointer to a free area with at least 'sz' bytes in buffer
** 'B'. 'boxidx' is the relative position in the stack where is the
** buffer's box or its placeholder.
*/
static char *prepbuffsize (lauL_Buffer *B, size_t sz, int boxidx) {
  checkbufferlevel(B, boxidx);
  if (B->size - B->n >= sz)  /* enough space? */
    return B->b + B->n;
  else {
    lau_State *L = B->L;
    char *newbuff;
    size_t newsize = newbuffsize(B, sz);
    /* create larger buffer */
    if (buffonstack(B))  /* buffer already has a box? */
      newbuff = (char *)resizebox(L, boxidx, newsize);  /* resize it */
    else {  /* no box yet */
      lau_remove(L, boxidx);  /* remove placeholder */
      newbox(L);  /* create a new box */
      lau_insert(L, boxidx);  /* move box to its intended position */
      lau_toclose(L, boxidx);
      newbuff = (char *)resizebox(L, boxidx, newsize);
      memcpy(newbuff, B->b, B->n * sizeof(char));  /* copy original content */
    }
    B->b = newbuff;
    B->size = newsize;
    return newbuff + B->n;
  }
}

/*
** returns a pointer to a free area with at least 'sz' bytes
*/
LAULIB_API char *lauL_prepbuffsize (lauL_Buffer *B, size_t sz) {
  return prepbuffsize(B, sz, -1);
}


LAULIB_API void lauL_addlstring (lauL_Buffer *B, const char *s, size_t l) {
  if (l > 0) {  /* avoid 'memcpy' when 's' can be NULL */
    char *b = prepbuffsize(B, l, -1);
    memcpy(b, s, l * sizeof(char));
    lauL_addsize(B, l);
  }
}


LAULIB_API void lauL_addstring (lauL_Buffer *B, const char *s) {
  lauL_addlstring(B, s, strlen(s));
}


LAULIB_API void lauL_pushresult (lauL_Buffer *B) {
  lau_State *L = B->L;
  checkbufferlevel(B, -1);
  if (!buffonstack(B))  /* using static buffer? */
    lau_pushlstring(L, B->b, B->n);  /* save result as regular string */
  else {  /* reuse buffer already allocated */
    UBox *box = (UBox *)lau_touserdata(L, -1);
    void *ud;
    lau_Alloc allocf = lau_getallocf(L, &ud);  /* function to free buffer */
    size_t len = B->n;  /* final string length */
    char *s;
    resizebox(L, -1, len + 1);  /* adjust box size to content size */
    s = (char*)box->box;  /* final buffer address */
    s[len] = '\0';  /* add ending zero */
    /* clear box, as Lau will take control of the buffer */
    box->bsize = 0;  box->box = NULL;
    lau_pushexternalstring(L, s, len, allocf, ud);
    lau_closeslot(L, -2);  /* close the box */
    lau_gc(L, LAU_GCSTEP, len);
  }
  lau_remove(L, -2);  /* remove box or placeholder from the stack */
}


LAULIB_API void lauL_pushresultsize (lauL_Buffer *B, size_t sz) {
  lauL_addsize(B, sz);
  lauL_pushresult(B);
}


/*
** 'lauL_addvalue' is the only function in the Buffer system where the
** box (if existent) is not on the top of the stack. So, instead of
** calling 'lauL_addlstring', it replicates the code using -2 as the
** last argument to 'prepbuffsize', signaling that the box is (or will
** be) below the string being added to the buffer. (Box creation can
** trigger an emergency GC, so we should not remove the string from the
** stack before we have the space guaranteed.)
*/
LAULIB_API void lauL_addvalue (lauL_Buffer *B) {
  lau_State *L = B->L;
  size_t len;
  const char *s = lau_tolstring(L, -1, &len);
  char *b = prepbuffsize(B, len, -2);
  memcpy(b, s, len * sizeof(char));
  lauL_addsize(B, len);
  lau_pop(L, 1);  /* pop string */
}


LAULIB_API void lauL_buffinit (lau_State *L, lauL_Buffer *B) {
  B->L = L;
  B->b = B->init.b;
  B->n = 0;
  B->size = LAUL_BUFFERSIZE;
  lau_pushlightuserdata(L, (void*)B);  /* push placeholder */
}


LAULIB_API char *lauL_buffinitsize (lau_State *L, lauL_Buffer *B, size_t sz) {
  lauL_buffinit(L, B);
  return prepbuffsize(B, sz, -1);
}

/* }====================================================== */


/*
** {======================================================
** Reference system
** =======================================================
*/

/*
** The previously freed references form a linked list: t[1] is the index
** of a first free index, t[t[1]] is the index of the second element,
** etc. A zero signals the end of the list.
*/
LAULIB_API int lauL_ref (lau_State *L, int t) {
  int ref;
  if (lau_isnil(L, -1)) {
    lau_pop(L, 1);  /* remove from stack */
    return LAU_REFNIL;  /* 'nil' has a unique fixed reference */
  }
  t = lau_absindex(L, t);
  if (lau_rawgeti(L, t, 1) == LAU_TNUMBER)  /* already initialized? */
    ref = (int)lau_tointeger(L, -1);  /* ref = t[1] */
  else {  /* first access */
    lau_assert(!lau_toboolean(L, -1));  /* must be nil or false */
    ref = 0;  /* list is empty */
    lau_pushinteger(L, 0);  /* initialize as an empty list */
    lau_rawseti(L, t, 1);  /* ref = t[1] = 0 */
  }
  lau_pop(L, 1);  /* remove element from stack */
  if (ref != 0) {  /* any free element? */
    lau_rawgeti(L, t, ref);  /* remove it from list */
    lau_rawseti(L, t, 1);  /* (t[1] = t[ref]) */
  }
  else  /* no free elements */
    ref = (int)lau_rawlen(L, t) + 1;  /* get a new reference */
  lau_rawseti(L, t, ref);
  return ref;
}


LAULIB_API void lauL_unref (lau_State *L, int t, int ref) {
  if (ref >= 0) {
    t = lau_absindex(L, t);
    lau_rawgeti(L, t, 1);
    lau_assert(lau_isinteger(L, -1));
    lau_rawseti(L, t, ref);  /* t[ref] = t[1] */
    lau_pushinteger(L, ref);
    lau_rawseti(L, t, 1);  /* t[1] = ref */
  }
}

/* }====================================================== */


/*
** {======================================================
** Load functions
** =======================================================
*/

typedef struct LoadF {
  unsigned n;  /* number of pre-read characters */
  FILE *f;  /* file being read */
  char buff[BUFSIZ];  /* area for reading file */
} LoadF;


static const char *getF (lau_State *L, void *ud, size_t *size) {
  LoadF *lf = (LoadF *)ud;
  UNUSED(L);
  if (lf->n > 0) {  /* are there pre-read characters to be read? */
    *size = lf->n;  /* return them (chars already in buffer) */
    lf->n = 0;  /* no more pre-read characters */
  }
  else {  /* read a block from file */
    /* 'fread' can return > 0 *and* set the EOF flag. If next call to
       'getF' called 'fread', it might still wait for user input.
       The next check avoids this problem. */
    if (feof(lf->f)) return NULL;
    *size = fread(lf->buff, 1, sizeof(lf->buff), lf->f);  /* read block */
  }
  return lf->buff;
}


static int errfile (lau_State *L, const char *what, int fnameindex) {
  int err = errno;
  const char *filename = lau_tostring(L, fnameindex) + 1;
  if (err != 0)
    lau_pushfstring(L, "cannot %s %s: %s", what, filename, strerror(err));
  else
    lau_pushfstring(L, "cannot %s %s", what, filename);
  lau_remove(L, fnameindex);
  return LAU_ERRFILE;
}


/*
** Skip an optional BOM at the start of a stream. If there is an
** incomplete BOM (the first character is correct but the rest is
** not), returns the first character anyway to force an error
** (as no chunk can start with 0xEF).
*/
static int skipBOM (FILE *f) {
  int c = getc(f);  /* read first character */
  if (c == 0xEF && getc(f) == 0xBB && getc(f) == 0xBF)  /* correct BOM? */
    return getc(f);  /* ignore BOM and return next char */
  else  /* no (valid) BOM */
    return c;  /* return first character */
}


/*
** reads the first character of file 'f' and skips an optional BOM mark
** in its beginning plus its first line if it starts with '#'. Returns
** true if it skipped the first line.  In any case, '*cp' has the
** first "valid" character of the file (after the optional BOM and
** a first-line comment).
*/
static int skipcomment (FILE *f, int *cp) {
  int c = *cp = skipBOM(f);
  if (c == '#') {  /* first line is a comment (Unix exec. file)? */
    do {  /* skip first line */
      c = getc(f);
    } while (c != EOF && c != '\n');
    *cp = getc(f);  /* next character after comment, if present */
    return 1;  /* there was a comment */
  }
  else return 0;  /* no comment */
}


LAULIB_API int lauL_loadfilex (lau_State *L, const char *filename,
                                             const char *mode) {
  LoadF lf;
  int status, readstatus;
  int c;
  int fnameindex = lau_gettop(L) + 1;  /* index of filename on the stack */
  if (filename == NULL) {
    lau_pushliteral(L, "=stdin");
    lf.f = stdin;
  }
  else {
    lau_pushfstring(L, "@%s", filename);
    errno = 0;
    lf.f = fopen(filename, "r");
    if (lf.f == NULL) return errfile(L, "open", fnameindex);
  }
  lf.n = 0;
  if (skipcomment(lf.f, &c))  /* read initial portion */
    lf.buff[lf.n++] = '\n';  /* add newline to correct line numbers */
  if (c == LAU_SIGNATURE[0]) {  /* binary file? */
    lf.n = 0;  /* remove possible newline */
    if (filename) {  /* "real" file? */
      errno = 0;
      lf.f = freopen(filename, "rb", lf.f);  /* reopen in binary mode */
      if (lf.f == NULL) return errfile(L, "reopen", fnameindex);
      skipcomment(lf.f, &c);  /* re-read initial portion */
    }
  }
  if (c != EOF)
    lf.buff[lf.n++] = cast_char(c);  /* 'c' is the first character */
  status = lau_load(L, getF, &lf, lau_tostring(L, -1), mode);
  readstatus = ferror(lf.f);
  errno = 0;  /* no useful error number until here */
  if (filename) fclose(lf.f);  /* close file (even in case of errors) */
  if (readstatus) {
    lau_settop(L, fnameindex);  /* ignore results from 'lau_load' */
    return errfile(L, "read", fnameindex);
  }
  lau_remove(L, fnameindex);
  return status;
}


typedef struct LoadS {
  const char *s;
  size_t size;
} LoadS;


static const char *getS (lau_State *L, void *ud, size_t *size) {
  LoadS *ls = (LoadS *)ud;
  UNUSED(L);
  if (ls->size == 0) return NULL;
  *size = ls->size;
  ls->size = 0;
  return ls->s;
}


LAULIB_API int lauL_loadbufferx (lau_State *L, const char *buff, size_t size,
                                 const char *name, const char *mode) {
  LoadS ls;
  ls.s = buff;
  ls.size = size;
  return lau_load(L, getS, &ls, name, mode);
}


LAULIB_API int lauL_loadstring (lau_State *L, const char *s) {
  return lauL_loadbufferx(L, s, strlen(s), s, "t");
}

/* }====================================================== */



LAULIB_API int lauL_getmetafield (lau_State *L, int obj, const char *event) {
  if (!lau_getmetatable(L, obj))  /* no metatable? */
    return LAU_TNIL;
  else {
    int tt;
    lau_pushstring(L, event);
    tt = lau_rawget(L, -2);
    if (tt == LAU_TNIL)  /* is metafield nil? */
      lau_pop(L, 2);  /* remove metatable and metafield */
    else
      lau_remove(L, -2);  /* remove only metatable */
    return tt;  /* return metafield type */
  }
}


LAULIB_API int lauL_callmeta (lau_State *L, int obj, const char *event) {
  obj = lau_absindex(L, obj);
  if (lauL_getmetafield(L, obj, event) == LAU_TNIL)  /* no metafield? */
    return 0;
  lau_pushvalue(L, obj);
  lau_call(L, 1, 1);
  return 1;
}


LAULIB_API lau_Integer lauL_len (lau_State *L, int idx) {
  lau_Integer l;
  int isnum;
  lau_len(L, idx);
  l = lau_tointegerx(L, -1, &isnum);
  if (l_unlikely(!isnum))
    lauL_error(L, "object length is not an integer");
  lau_pop(L, 1);  /* remove object */
  return l;
}


LAULIB_API const char *lauL_tolstring (lau_State *L, int idx, size_t *len) {
  idx = lau_absindex(L,idx);
  if (lauL_callmeta(L, idx, "__tostring")) {  /* metafield? */
    if (!lau_isstring(L, -1))
      lauL_error(L, "'__tostring' must return a string");
  }
  else {
    switch (lau_type(L, idx)) {
      case LAU_TNUMBER: {
        char buff[LAU_N2SBUFFSZ];
        lau_numbertocstring(L, idx, buff);
        lau_pushstring(L, buff);
        break;
      }
      case LAU_TSTRING:
        lau_pushvalue(L, idx);
        break;
      case LAU_TBOOLEAN:
        lau_pushstring(L, (lau_toboolean(L, idx) ? "true" : "false"));
        break;
      case LAU_TNIL:
        lau_pushliteral(L, "nil");
        break;
      default: {
        int tt = lauL_getmetafield(L, idx, "__name");  /* try name */
        const char *kind = (tt == LAU_TSTRING) ? lau_tostring(L, -1) :
                                                 lauL_typename(L, idx);
        lau_pushfstring(L, "%s: %p", kind, lau_topointer(L, idx));
        if (tt != LAU_TNIL)
          lau_remove(L, -2);  /* remove '__name' */
        break;
      }
    }
  }
  return lau_tolstring(L, -1, len);
}


/*
** set functions from list 'l' into table at top - 'nup'; each
** function gets the 'nup' elements at the top as upvalues.
** Returns with only the table at the stack.
*/
LAULIB_API void lauL_setfuncs (lau_State *L, const lauL_Reg *l, int nup) {
  lauL_checkstack(L, nup, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    if (l->func == NULL)  /* placeholder? */
      lau_pushboolean(L, 0);
    else {
      int i;
      for (i = 0; i < nup; i++)  /* copy upvalues to the top */
        lau_pushvalue(L, -nup);
      lau_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    }
    lau_setfield(L, -(nup + 2), l->name);
  }
  lau_pop(L, nup);  /* remove upvalues */
}


/*
** ensure that stack[idx][fname] has a table and push that table
** into the stack
*/
LAULIB_API int lauL_getsubtable (lau_State *L, int idx, const char *fname) {
  if (lau_getfield(L, idx, fname) == LAU_TTABLE)
    return 1;  /* table already there */
  else {
    lau_pop(L, 1);  /* remove previous result */
    idx = lau_absindex(L, idx);
    lau_newtable(L);
    lau_pushvalue(L, -1);  /* copy to be left at top */
    lau_setfield(L, idx, fname);  /* assign new table to field */
    return 0;  /* false, because did not find table there */
  }
}


/*
** Stripped-down 'require': After checking "loaded" table, calls 'openf'
** to open a module, registers the result in 'package.loaded' table and,
** if 'glb' is true, also registers the result in the global table.
** Leaves resulting module on the top.
*/
LAULIB_API void lauL_requiref (lau_State *L, const char *modname,
                               lau_CFunction openf, int glb) {
  lauL_getsubtable(L, LAU_REGISTRYINDEX, LAU_LOADED_TABLE);
  lau_getfield(L, -1, modname);  /* LOADED[modname] */
  if (!lau_toboolean(L, -1)) {  /* package not already loaded? */
    lau_pop(L, 1);  /* remove field */
    lau_pushcfunction(L, openf);
    lau_pushstring(L, modname);  /* argument to open function */
    lau_call(L, 1, 1);  /* call 'openf' to open module */
    lau_pushvalue(L, -1);  /* make copy of module (call result) */
    lau_setfield(L, -3, modname);  /* LOADED[modname] = module */
  }
  lau_remove(L, -2);  /* remove LOADED table */
  if (glb) {
    lau_pushvalue(L, -1);  /* copy of module */
    lau_setglobal(L, modname);  /* _G[modname] = module */
  }
}


LAULIB_API void lauL_addgsub (lauL_Buffer *b, const char *s,
                                     const char *p, const char *r) {
  const char *wild;
  size_t l = strlen(p);
  while ((wild = strstr(s, p)) != NULL) {
    lauL_addlstring(b, s, ct_diff2sz(wild - s));  /* push prefix */
    lauL_addstring(b, r);  /* push replacement in place of pattern */
    s = wild + l;  /* continue after 'p' */
  }
  lauL_addstring(b, s);  /* push last suffix */
}


LAULIB_API const char *lauL_gsub (lau_State *L, const char *s,
                                  const char *p, const char *r) {
  lauL_Buffer b;
  lauL_buffinit(L, &b);
  lauL_addgsub(&b, s, p, r);
  lauL_pushresult(&b);
  return lau_tostring(L, -1);
}


void *lauL_alloc (void *ud, void *ptr, size_t osize, size_t nsize) {
  UNUSED(ud); UNUSED(osize);
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  else
    return realloc(ptr, nsize);
}


/*
** Standard panic function just prints an error message. The test
** with 'lau_type' avoids possible memory errors in 'lau_tostring'.
*/
static int panic (lau_State *L) {
  const char *msg = (lau_type(L, -1) == LAU_TSTRING)
                  ? lau_tostring(L, -1)
                  : "error object is not a string";
  lau_writestringerror("PANIC: unprotected error in call to Lau API (%s)\n",
                        msg);
  return 0;  /* return to Lau to abort */
}


/*
** Warning functions:
** warnfoff: warning system is off
** warnfon: ready to start a new message
** warnfcont: previous message is to be continued
*/
static void warnfoff (void *ud, const char *message, int tocont);
static void warnfon (void *ud, const char *message, int tocont);
static void warnfcont (void *ud, const char *message, int tocont);


/*
** Check whether message is a control message. If so, execute the
** control or ignore it if unknown.
*/
static int checkcontrol (lau_State *L, const char *message, int tocont) {
  if (tocont || *(message++) != '@')  /* not a control message? */
    return 0;
  else {
    if (strcmp(message, "off") == 0)
      lau_setwarnf(L, warnfoff, L);  /* turn warnings off */
    else if (strcmp(message, "on") == 0)
      lau_setwarnf(L, warnfon, L);   /* turn warnings on */
    return 1;  /* it was a control message */
  }
}


static void warnfoff (void *ud, const char *message, int tocont) {
  checkcontrol((lau_State *)ud, message, tocont);
}


/*
** Writes the message and handle 'tocont', finishing the message
** if needed and setting the next warn function.
*/
static void warnfcont (void *ud, const char *message, int tocont) {
  lau_State *L = (lau_State *)ud;
  lau_writestringerror("%s", message);  /* write message */
  if (tocont)  /* not the last part? */
    lau_setwarnf(L, warnfcont, L);  /* to be continued */
  else {  /* last part */
    lau_writestringerror("%s", "\n");  /* finish message with end-of-line */
    lau_setwarnf(L, warnfon, L);  /* next call is a new message */
  }
}


static void warnfon (void *ud, const char *message, int tocont) {
  if (checkcontrol((lau_State *)ud, message, tocont))  /* control message? */
    return;  /* nothing else to be done */
  lau_writestringerror("%s", "Lau warning: ");  /* start a new warning */
  warnfcont(ud, message, tocont);  /* finish processing */
}



/*
** A function to compute an unsigned int with some level of
** randomness. Rely on Address Space Layout Randomization (if present)
** and the current time.
*/
#if !defined(laui_makeseed)

#include <time.h>


/* Size for the buffer, in bytes */
#define BUFSEEDB	(sizeof(void*) + sizeof(time_t))

/* Size for the buffer in int's, rounded up */
#define BUFSEED		((BUFSEEDB + sizeof(int) - 1) / sizeof(int))

/*
** Copy the contents of variable 'v' into the buffer pointed by 'b'.
** (The '&b[0]' disguises 'b' to fix an absurd warning from clang.)
*/
#define addbuff(b,v)	(memcpy(&b[0], &(v), sizeof(v)), b += sizeof(v))


static unsigned int laui_makeseed (void) {
  unsigned int buff[BUFSEED];
  unsigned int res;
  unsigned int i;
  time_t t = time(NULL);
  char *b = (char*)buff;
  addbuff(b, b);  /* local variable's address */
  addbuff(b, t);  /* time */
  /* fill (rare but possible) remain of the buffer with zeros */
  memset(b, 0, sizeof(buff) - BUFSEEDB);
  res = buff[0];
  for (i = 1; i < BUFSEED; i++)
    res ^= (res >> 3) + (res << 7) + buff[i];
  return res;
}

#endif


LAULIB_API unsigned int lauL_makeseed (lau_State *L) {
  UNUSED(L);
  return laui_makeseed();
}


/*
** Use the name with parentheses so that headers can redefine it
** as a macro.
*/
LAULIB_API lau_State *(lauL_newstate) (void) {
  lau_State *L = lau_newstate(lauL_alloc, NULL, lauL_makeseed(NULL));
  if (l_likely(L)) {
    lau_atpanic(L, &panic);
    lau_setwarnf(L, warnfon, L);
  }
  return L;
}


LAULIB_API void lauL_checkversion_ (lau_State *L, lau_Number ver, size_t sz) {
  lau_Number v = lau_version(L);
  if (sz != LAUL_NUMSIZES)  /* check numeric types */
    lauL_error(L, "core and library have incompatible numeric types");
  else if (v != ver)
    lauL_error(L, "version mismatch: app. needs %f, Lau core provides %f",
                  (LAUI_UACNUMBER)ver, (LAUI_UACNUMBER)v);
}

