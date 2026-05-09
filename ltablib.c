/*
** $Id: ltablib.c $
** Library for Table Manipulation
** See Copyright Notice in lau.h
*/

#define ltablib_c
#define LAU_LIB

#include "lprefix.h"


#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "lau.h"

#include "lauxlib.h"
#include "laulib.h"
#include "llimits.h"


/*
** Operations that an object must define to mimic a table
** (some functions only need some of them)
*/
#define TAB_R	1			/* read */
#define TAB_W	2			/* write */
#define TAB_L	4			/* length */
#define TAB_RW	(TAB_R | TAB_W)		/* read/write */


#define aux_getn(L,n,w)	(checktab(L, n, (w) | TAB_L), lauL_len(L, n))


static int checkfield (lau_State *L, const char *key, int n) {
  lau_pushstring(L, key);
  return (lau_rawget(L, -n) != LAU_TNIL);
}


/*
** Check that 'arg' either is a table or can behave like one (that is,
** has a metatable with the required metamethods).
*/
static void checktab (lau_State *L, int arg, int what) {
  int tp = lau_type(L, arg);
  if (tp != LAU_TTABLE) {  /* is it not a table? */
    int n = 1;  /* number of elements to pop */
    if (lau_getmetatable(L, arg) &&  /* must have metatable */
        (!(what & TAB_R) || checkfield(L, "__index", ++n)) &&
        (!(what & TAB_W) || checkfield(L, "__newindex", ++n)) &&
        (!(what & TAB_L) ||  /* strings don't need '__len' to have a length */
             tp == LAU_TSTRING || checkfield(L, "__len", ++n))) {
      lau_pop(L, n);  /* pop metatable and tested metamethods */
    }
    else
      lauL_checktype(L, arg, LAU_TTABLE);  /* force an error */
  }
}


static int tcreate (lau_State *L) {
  lau_Unsigned sizeseq = (lau_Unsigned)lauL_checkinteger(L, 1);
  lau_Unsigned sizerest = (lau_Unsigned)lauL_optinteger(L, 2, 0);
  lauL_argcheck(L, sizeseq <= cast_uint(INT_MAX), 1, "out of range");
  lauL_argcheck(L, sizerest <= cast_uint(INT_MAX), 2, "out of range");
  lau_createtable(L, cast_int(sizeseq), cast_int(sizerest));
  return 1;
}


static int tinsert (lau_State *L) {
  lau_Integer pos;  /* where to insert new element */
  lau_Integer e = aux_getn(L, 1, TAB_RW);
  e = lauL_intop(+, e, 1);  /* first empty element */
  switch (lau_gettop(L)) {
    case 2: {  /* called with only 2 arguments */
      pos = e;  /* insert new element at the end */
      break;
    }
    case 3: {
      lau_Integer i;
      pos = lauL_checkinteger(L, 2);  /* 2nd argument is the position */
      /* check whether 'pos' is in [1, e] */
      lauL_argcheck(L, (lau_Unsigned)pos - 1u < (lau_Unsigned)e, 2,
                       "position out of bounds");
      for (i = e; i > pos; i--) {  /* move up elements */
        lau_geti(L, 1, i - 1);
        lau_seti(L, 1, i);  /* t[i] = t[i - 1] */
      }
      break;
    }
    default: {
      return lauL_error(L, "wrong number of arguments to 'insert'");
    }
  }
  lau_seti(L, 1, pos);  /* t[pos] = v */
  return 0;
}


static int tremove (lau_State *L) {
  lau_Integer size = aux_getn(L, 1, TAB_RW);
  lau_Integer pos = lauL_optinteger(L, 2, size);
  if (pos != size)  /* validate 'pos' if given */
    /* check whether 'pos' is in [1, size + 1] */
    lauL_argcheck(L, (lau_Unsigned)pos - 1u <= (lau_Unsigned)size, 2,
                     "position out of bounds");
  lau_geti(L, 1, pos);  /* result = t[pos] */
  for ( ; pos < size; pos++) {
    lau_geti(L, 1, pos + 1);
    lau_seti(L, 1, pos);  /* t[pos] = t[pos + 1] */
  }
  lau_pushnil(L);
  lau_seti(L, 1, pos);  /* remove entry t[pos] */
  return 1;
}


/*
** Copy elements (1[f], ..., 1[e]) into (tt[t], tt[t+1], ...). Whenever
** possible, copy in increasing order, which is better for rehashing.
** "possible" means destination after original range, or smaller
** than origin, or copying to another table.
*/
static int tmove (lau_State *L) {
  lau_Integer f = lauL_checkinteger(L, 2);
  lau_Integer e = lauL_checkinteger(L, 3);
  lau_Integer t = lauL_checkinteger(L, 4);
  int tt = !lau_isnoneornil(L, 5) ? 5 : 1;  /* destination table */
  checktab(L, 1, TAB_R);
  checktab(L, tt, TAB_W);
  if (e >= f) {  /* otherwise, nothing to move */
    lau_Integer n, i;
    lauL_argcheck(L, f > 0 || e < LAU_MAXINTEGER + f, 3,
                  "too many elements to move");
    n = e - f + 1;  /* number of elements to move */
    lauL_argcheck(L, t <= LAU_MAXINTEGER - n + 1, 4,
                  "destination wrap around");
    if (t > e || t <= f || (tt != 1 && !lau_compare(L, 1, tt, LAU_OPEQ))) {
      for (i = 0; i < n; i++) {
        lau_geti(L, 1, f + i);
        lau_seti(L, tt, t + i);
      }
    }
    else {
      for (i = n - 1; i >= 0; i--) {
        lau_geti(L, 1, f + i);
        lau_seti(L, tt, t + i);
      }
    }
  }
  lau_pushvalue(L, tt);  /* return destination table */
  return 1;
}


static void addfield (lau_State *L, lauL_Buffer *b, lau_Integer i) {
  lau_geti(L, 1, i);
  if (l_unlikely(!lau_isstring(L, -1)))
    lauL_error(L, "invalid value (%s) at index %I in table for 'concat'",
                  lauL_typename(L, -1), (LAUI_UACINT)i);
  lauL_addvalue(b);
}


static int tconcat (lau_State *L) {
  lauL_Buffer b;
  lau_Integer last = aux_getn(L, 1, TAB_R);
  size_t lsep;
  const char *sep = lauL_optlstring(L, 2, "", &lsep);
  lau_Integer i = lauL_optinteger(L, 3, 1);
  last = lauL_optinteger(L, 4, last);
  lauL_buffinit(L, &b);
  for (; i < last; i++) {
    addfield(L, &b, i);
    lauL_addlstring(&b, sep, lsep);
  }
  if (i == last)  /* add last value (if interval was not empty) */
    addfield(L, &b, i);
  lauL_pushresult(&b);
  return 1;
}


/*
** {======================================================
** Pack/unpack
** =======================================================
*/

static int tpack (lau_State *L) {
  int i;
  int n = lau_gettop(L);  /* number of elements to pack */
  lau_createtable(L, n, 1);  /* create result table */
  lau_insert(L, 1);  /* put it at index 1 */
  for (i = n; i >= 1; i--)  /* assign elements */
    lau_seti(L, 1, i);
  lau_pushinteger(L, n);
  lau_setfield(L, 1, "n");  /* t.n = number of elements */
  return 1;  /* return table */
}


static int tunpack (lau_State *L) {
  lau_Unsigned n;
  lau_Integer len = aux_getn(L, 1, TAB_R);
  lau_Integer i = lauL_optinteger(L, 2, 1);
  lau_Integer e = lauL_opt(L, lauL_checkinteger, 3, len);
  if (i > e) return 0;  /* empty range */
  n = l_castS2U(e) - l_castS2U(i);  /* number of elements minus 1 */
  if (l_unlikely(n >= (unsigned int)INT_MAX  ||
                 !lau_checkstack(L, (int)(++n))))
    return lauL_error(L, "too many results to unpack");
  for (; i < e; i++) {  /* push arg[i..e - 1] (to avoid overflows) */
    lau_geti(L, 1, i);
  }
  lau_geti(L, 1, e);  /* push last element */
  return (int)n;
}

/* }====================================================== */



/*
** {======================================================
** Quicksort
** (based on 'Algorithms in MODULA-3', Robert Sedgewick;
**  Addison-Wesley, 1993.)
** =======================================================
*/


/*
** Type for array indices. These indices are always limited by INT_MAX,
** so it is safe to cast them to lau_Integer even for Lau 32 bits.
*/
typedef unsigned int IdxT;


/* Versions of lau_seti/lau_geti specialized for IdxT */
#define geti(L,idt,idx)	lau_geti(L, idt, l_castU2S(idx))
#define seti(L,idt,idx)	lau_seti(L, idt, l_castU2S(idx))


/*
** Produce a "random" 'unsigned int' to randomize pivot choice. This
** macro is used only when 'sort' detects a big imbalance in the result
** of a partition. (If you don't want/need this "randomness", ~0 is a
** good choice.)
*/
#if !defined(l_randomizePivot)
#define l_randomizePivot(L)	lauL_makeseed(L)
#endif					/* } */


/* arrays larger than 'RANLIMIT' may use randomized pivots */
#define RANLIMIT	100u


static void set2 (lau_State *L, IdxT i, IdxT j) {
  seti(L, 1, i);
  seti(L, 1, j);
}


/*
** Return true iff value at stack index 'a' is less than the value at
** index 'b' (according to the order of the sort).
*/
static int sort_comp (lau_State *L, int a, int b) {
  if (lau_isnil(L, 2))  /* no function? */
    return lau_compare(L, a, b, LAU_OPLT);  /* a < b */
  else {  /* function */
    int res;
    lau_pushvalue(L, 2);    /* push function */
    lau_pushvalue(L, a-1);  /* -1 to compensate function */
    lau_pushvalue(L, b-2);  /* -2 to compensate function and 'a' */
    lau_call(L, 2, 1);      /* call function */
    res = lau_toboolean(L, -1);  /* get result */
    lau_pop(L, 1);          /* pop result */
    return res;
  }
}


/*
** Does the partition: Pivot P is at the top of the stack.
** precondition: a[lo] <= P == a[up-1] <= a[up],
** so it only needs to do the partition from lo + 1 to up - 2.
** Pos-condition: a[lo .. i - 1] <= a[i] == P <= a[i + 1 .. up]
** returns 'i'.
*/
static IdxT partition (lau_State *L, IdxT lo, IdxT up) {
  IdxT i = lo;  /* will be incremented before first use */
  IdxT j = up - 1;  /* will be decremented before first use */
  /* loop invariant: a[lo .. i] <= P <= a[j .. up] */
  for (;;) {
    /* next loop: repeat ++i while a[i] < P */
    while ((void)geti(L, 1, ++i), sort_comp(L, -1, -2)) {
      if (l_unlikely(i == up - 1))  /* a[up - 1] < P == a[up - 1] */
        lauL_error(L, "invalid order function for sorting");
      lau_pop(L, 1);  /* remove a[i] */
    }
    /* after the loop, a[i] >= P and a[lo .. i - 1] < P  (a) */
    /* next loop: repeat --j while P < a[j] */
    while ((void)geti(L, 1, --j), sort_comp(L, -3, -1)) {
      if (l_unlikely(j < i))  /* j <= i - 1 and a[j] > P, contradicts (a) */
        lauL_error(L, "invalid order function for sorting");
      lau_pop(L, 1);  /* remove a[j] */
    }
    /* after the loop, a[j] <= P and a[j + 1 .. up] >= P */
    if (j < i) {  /* no elements out of place? */
      /* a[lo .. i - 1] <= P <= a[j + 1 .. i .. up] */
      lau_pop(L, 1);  /* pop a[j] */
      /* swap pivot (a[up - 1]) with a[i] to satisfy pos-condition */
      set2(L, up - 1, i);
      return i;
    }
    /* otherwise, swap a[i] - a[j] to restore invariant and repeat */
    set2(L, i, j);
  }
}


/*
** Choose an element in the middle (2nd-3th quarters) of [lo,up]
** "randomized" by 'rnd'
*/
static IdxT choosePivot (IdxT lo, IdxT up, unsigned int rnd) {
  IdxT r4 = (up - lo) / 4;  /* range/4 */
  IdxT p = (rnd ^ lo ^ up) % (r4 * 2) + (lo + r4);
  lau_assert(lo + r4 <= p && p <= up - r4);
  return p;
}


/*
** Quicksort algorithm (recursive function)
*/
static void auxsort (lau_State *L, IdxT lo, IdxT up, unsigned rnd) {
  while (lo < up) {  /* loop for tail recursion */
    IdxT p;  /* Pivot index */
    IdxT n;  /* to be used later */
    /* sort elements 'lo', 'p', and 'up' */
    geti(L, 1, lo);
    geti(L, 1, up);
    if (sort_comp(L, -1, -2))  /* a[up] < a[lo]? */
      set2(L, lo, up);  /* swap a[lo] - a[up] */
    else
      lau_pop(L, 2);  /* remove both values */
    if (up - lo == 1)  /* only 2 elements? */
      return;  /* already sorted */
    if (up - lo < RANLIMIT || rnd == 0)  /* small interval or no randomize? */
      p = (lo + up)/2;  /* middle element is a good pivot */
    else  /* for larger intervals, it is worth a random pivot */
      p = choosePivot(lo, up, rnd);
    geti(L, 1, p);
    geti(L, 1, lo);
    if (sort_comp(L, -2, -1))  /* a[p] < a[lo]? */
      set2(L, p, lo);  /* swap a[p] - a[lo] */
    else {
      lau_pop(L, 1);  /* remove a[lo] */
      geti(L, 1, up);
      if (sort_comp(L, -1, -2))  /* a[up] < a[p]? */
        set2(L, p, up);  /* swap a[up] - a[p] */
      else
        lau_pop(L, 2);
    }
    if (up - lo == 2)  /* only 3 elements? */
      return;  /* already sorted */
    geti(L, 1, p);  /* get middle element (Pivot) */
    lau_pushvalue(L, -1);  /* push Pivot */
    geti(L, 1, up - 1);  /* push a[up - 1] */
    set2(L, p, up - 1);  /* swap Pivot (a[p]) with a[up - 1] */
    p = partition(L, lo, up);
    /* a[lo .. p - 1] <= a[p] == P <= a[p + 1 .. up] */
    if (p - lo < up - p) {  /* lower interval is smaller? */
      auxsort(L, lo, p - 1, rnd);  /* call recursively for lower interval */
      n = p - lo;  /* size of smaller interval */
      lo = p + 1;  /* tail call for [p + 1 .. up] (upper interval) */
    }
    else {
      auxsort(L, p + 1, up, rnd);  /* call recursively for upper interval */
      n = up - p;  /* size of smaller interval */
      up = p - 1;  /* tail call for [lo .. p - 1]  (lower interval) */
    }
    if ((up - lo) / 128 > n) /* partition too imbalanced? */
      rnd = l_randomizePivot(L);  /* try a new randomization */
  }  /* tail call auxsort(L, lo, up, rnd) */
}


static int sort (lau_State *L) {
  lau_Integer n = aux_getn(L, 1, TAB_RW);
  if (n > 1) {  /* non-trivial interval? */
    lauL_argcheck(L, n < INT_MAX, 1, "array too big");
    if (!lau_isnoneornil(L, 2))  /* is there a 2nd argument? */
      lauL_checktype(L, 2, LAU_TFUNCTION);  /* must be a function */
    lau_settop(L, 2);  /* make sure there are two arguments */
    auxsort(L, 1, (IdxT)n, 0);
  }
  return 0;
}

/* }====================================================== */


static const lauL_Reg tab_funcs[] = {
  {"concat", tconcat},
  {"create", tcreate},
  {"insert", tinsert},
  {"pack", tpack},
  {"unpack", tunpack},
  {"remove", tremove},
  {"move", tmove},
  {"sort", sort},
  {NULL, NULL}
};


LAUMOD_API int lauopen_table (lau_State *L) {
  lauL_newlib(L, tab_funcs);
  return 1;
}

