/*
** $Id: lstrlib.c $
** Standard library for string operations and pattern-matching
** See Copyright Notice in lau.h
*/

#define lstrlib_c
#define LAU_LIB

#include "lprefix.h"


#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lau.h"

#include "lauxlib.h"
#include "laulib.h"
#include "llimits.h"


/*
** maximum number of captures that a pattern can do during
** pattern-matching. This limit is arbitrary, but must fit in
** an unsigned char.
*/
#if !defined(LAU_MAXCAPTURES)
#define LAU_MAXCAPTURES		32
#endif


static int str_len (lau_State *L) {
  size_t l;
  lauL_checklstring(L, 1, &l);
  lau_pushinteger(L, (lau_Integer)l);
  return 1;
}


/*
** translate a relative initial string position
** (negative means back from end): clip result to [1, inf).
** The length of any string in Lau must fit in a lau_Integer,
** so there are no overflows in the casts.
** The inverted comparison avoids a possible overflow
** computing '-pos'.
*/
static size_t posrelatI (lau_Integer pos, size_t len) {
  if (pos > 0)
    return (size_t)pos;
  else if (pos == 0)
    return 1;
  else if (pos < -(lau_Integer)len)  /* inverted comparison */
    return 1;  /* clip to 1 */
  else return len + (size_t)pos + 1;
}


/*
** Gets an optional ending string position from argument 'arg',
** with default value 'def'.
** Negative means back from end: clip result to [0, len]
*/
static size_t getendpos (lau_State *L, int arg, lau_Integer def,
                         size_t len) {
  lau_Integer pos = lauL_optinteger(L, arg, def);
  if (pos > (lau_Integer)len)
    return len;
  else if (pos >= 0)
    return (size_t)pos;
  else if (pos < -(lau_Integer)len)
    return 0;
  else return len + (size_t)pos + 1;
}


static int str_sub (lau_State *L) {
  size_t l;
  const char *s = lauL_checklstring(L, 1, &l);
  size_t start = posrelatI(lauL_checkinteger(L, 2), l);
  size_t end = getendpos(L, 3, -1, l);
  if (start <= end)
    lau_pushlstring(L, s + start - 1, (end - start) + 1);
  else lau_pushliteral(L, "");
  return 1;
}


static int str_lower (lau_State *L) {
  size_t l;
  size_t i;
  lauL_Buffer b;
  const char *s = lauL_checklstring(L, 1, &l);
  char *p = lauL_buffinitsize(L, &b, l);
  for (i=0; i<l; i++)
    p[i] = cast_char(tolower(cast_uchar(s[i])));
  lauL_pushresultsize(&b, l);
  return 1;
}


static int str_upper (lau_State *L) {
  size_t l;
  size_t i;
  lauL_Buffer b;
  const char *s = lauL_checklstring(L, 1, &l);
  char *p = lauL_buffinitsize(L, &b, l);
  for (i=0; i<l; i++)
    p[i] = cast_char(toupper(cast_uchar(s[i])));
  lauL_pushresultsize(&b, l);
  return 1;
}



/*
** {======================================================
** METAMETHODS
** =======================================================
*/

#if defined(LAU_NOCVTS2N)	/* { */

/* no coercion from strings to numbers */

static const lauL_Reg stringmetamethods[] = {
  {"__index", NULL},  /* placeholder */
  {NULL, NULL}
};

#else		/* }{ */

static int tonum (lau_State *L, int arg) {
  if (lau_type(L, arg) == LAU_TNUMBER) {  /* already a number? */
    lau_pushvalue(L, arg);
    return 1;
  }
  else {  /* check whether it is a numerical string */
    size_t len;
    const char *s = lau_tolstring(L, arg, &len);
    return (s != NULL && lau_stringtonumber(L, s) == len + 1);
  }
}


/*
** To be here, either the first operand was a string or the first
** operand didn't have a corresponding metamethod. (Otherwise, that
** other metamethod would have been called.) So, if this metamethod
** doesn't work, the only other option would be for the second
** operand to have a different metamethod.
*/
static void trymt (lau_State *L, const char *mtkey, const char *opname) {
  lau_settop(L, 2);  /* back to the original arguments */
  if (l_unlikely(lau_type(L, 2) == LAU_TSTRING ||
                 !lauL_getmetafield(L, 2, mtkey)))
    lauL_error(L, "attempt to %s a '%s' with a '%s'", opname,
                  lauL_typename(L, -2), lauL_typename(L, -1));
  lau_insert(L, -3);  /* put metamethod before arguments */
  lau_call(L, 2, 1);  /* call metamethod */
}


static int arith (lau_State *L, int op, const char *mtname) {
  if (tonum(L, 1) && tonum(L, 2))
    lau_arith(L, op);  /* result will be on the top */
  else
    trymt(L, mtname, mtname + 2);
  return 1;
}


static int arith_add (lau_State *L) {
  return arith(L, LAU_OPADD, "__add");
}

static int arith_sub (lau_State *L) {
  return arith(L, LAU_OPSUB, "__sub");
}

static int arith_mul (lau_State *L) {
  return arith(L, LAU_OPMUL, "__mul");
}

static int arith_mod (lau_State *L) {
  return arith(L, LAU_OPMOD, "__mod");
}

static int arith_pow (lau_State *L) {
  return arith(L, LAU_OPPOW, "__pow");
}

static int arith_div (lau_State *L) {
  return arith(L, LAU_OPDIV, "__div");
}

static int arith_unm (lau_State *L) {
  return arith(L, LAU_OPUNM, "__unm");
}


static const lauL_Reg stringmetamethods[] = {
  {"__add", arith_add},
  {"__sub", arith_sub},
  {"__mul", arith_mul},
  {"__mod", arith_mod},
  {"__pow", arith_pow},
  {"__div", arith_div},
  {"__unm", arith_unm},
  {"__index", NULL},  /* placeholder */
  {NULL, NULL}
};

#endif		/* } */

/* }====================================================== */

/*
** {======================================================
** PATTERN MATCHING
** =======================================================
*/


#define CAP_UNFINISHED	(-1)
#define CAP_POSITION	(-2)


typedef struct MatchState {
  const char *src_init;  /* init of source string */
  const char *src_end;  /* end ('\0') of source string */
  const char *p_end;  /* end ('\0') of pattern */
  lau_State *L;
  int matchdepth;  /* control for recursive depth (to avoid C stack overflow) */
  int level;  /* total number of captures (finished or unfinished) */
  struct {
    const char *init;
    ptrdiff_t len;  /* length or special value (CAP_*) */
  } capture[LAU_MAXCAPTURES];
} MatchState;

/* recursive function */
static const char *match (MatchState *ms, const char *s, const char *p);


/* maximum recursion depth for 'match' */
#if !defined(MAXCCALLS)
#define MAXCCALLS	200
#endif


#define L_ESC		'%'
#define SPECIALS	"^$*+?.([%-"


static int check_capture (MatchState *ms, int l) {
  l -= '1';
  if (l_unlikely(l < 0 || l >= ms->level ||
                 ms->capture[l].len == CAP_UNFINISHED))
    return lauL_error(ms->L, "invalid capture index %%%d", l + 1);
  return l;
}


static int capture_to_close (MatchState *ms) {
  int level = ms->level;
  for (level--; level>=0; level--)
    if (ms->capture[level].len == CAP_UNFINISHED) return level;
  return lauL_error(ms->L, "invalid pattern capture");
}


static const char *classend (MatchState *ms, const char *p) {
  switch (*p++) {
    case L_ESC: {
      if (l_unlikely(p == ms->p_end))
        lauL_error(ms->L, "malformed pattern (ends with '%%')");
      return p+1;
    }
    case '[': {
      if (*p == '^') p++;
      do {  /* look for a ']' */
        if (l_unlikely(p == ms->p_end))
          lauL_error(ms->L, "malformed pattern (missing ']')");
        if (*(p++) == L_ESC && p < ms->p_end)
          p++;  /* skip escapes (e.g. '%]') */
      } while (*p != ']');
      return p+1;
    }
    default: {
      return p;
    }
  }
}

static int match_class (int c, int cl) {
  int res;
  switch (tolower(cl)) {
    case 'a' : res = isalpha(c); break;
    case 'c' : res = iscntrl(c); break;
    case 'd' : res = isdigit(c); break;
    case 'g' : res = isgraph(c); break;
    case 'l' : res = islower(c); break;
    case 'p' : res = ispunct(c); break;
    case 's' : res = isspace(c); break;
    case 'u' : res = isupper(c); break;
    case 'w' : res = isalnum(c); break;
    case 'x' : res = isxdigit(c); break;
    case 'z' : res = (c == 0); break;  /* deprecated option */
    default: return (cl == c);
  }
  return (islower(cl) ? res : !res);
}


static int matchbracketclass (int c, const char *p, const char *ec) {
  int sig = 1;
  if (*(p+1) == '^') {
    sig = 0;
    p++;  /* skip the '^' */
  }
  while (++p < ec) {
    if (*p == L_ESC) {
      p++;
      if (match_class(c, cast_uchar(*p)))
        return sig;
    }
    else if ((*(p+1) == '-') && (p+2 < ec)) {
      p+=2;
      if (cast_uchar(*(p-2)) <= c && c <= cast_uchar(*p))
        return sig;
    }
    else if (cast_uchar(*p) == c) return sig;
  }
  return !sig;
}


static int singlematch (MatchState *ms, const char *s, const char *p,
                        const char *ep) {
  if (s >= ms->src_end)
    return 0;
  else {
    int c = cast_uchar(*s);
    switch (*p) {
      case '.': return 1;  /* matches any char */
      case L_ESC: return match_class(c, cast_uchar(*(p+1)));
      case '[': return matchbracketclass(c, p, ep-1);
      default:  return (cast_uchar(*p) == c);
    }
  }
}


static const char *matchbalance (MatchState *ms, const char *s,
                                   const char *p) {
  if (l_unlikely(p >= ms->p_end - 1))
    lauL_error(ms->L, "malformed pattern (missing arguments to '%%b')");
  if (*s != *p) return NULL;
  else {
    int b = *p;
    int e = *(p+1);
    int cont = 1;
    while (++s < ms->src_end) {
      if (*s == e) {
        if (--cont == 0) return s+1;
      }
      else if (*s == b) cont++;
    }
  }
  return NULL;  /* string ends out of balance */
}


static const char *max_expand (MatchState *ms, const char *s,
                                 const char *p, const char *ep) {
  ptrdiff_t i = 0;  /* counts maximum expand for item */
  while (singlematch(ms, s + i, p, ep))
    i++;
  /* keeps trying to match with the maximum repetitions */
  while (i>=0) {
    const char *res = match(ms, (s+i), ep+1);
    if (res) return res;
    i--;  /* else didn't match; reduce 1 repetition to try again */
  }
  return NULL;
}


static const char *min_expand (MatchState *ms, const char *s,
                                 const char *p, const char *ep) {
  for (;;) {
    const char *res = match(ms, s, ep+1);
    if (res != NULL)
      return res;
    else if (singlematch(ms, s, p, ep))
      s++;  /* try with one more repetition */
    else return NULL;
  }
}


static const char *start_capture (MatchState *ms, const char *s,
                                    const char *p, int what) {
  const char *res;
  int level = ms->level;
  if (level >= LAU_MAXCAPTURES) lauL_error(ms->L, "too many captures");
  ms->capture[level].init = s;
  ms->capture[level].len = what;
  ms->level = level+1;
  if ((res=match(ms, s, p)) == NULL)  /* match failed? */
    ms->level--;  /* undo capture */
  return res;
}


static const char *end_capture (MatchState *ms, const char *s,
                                  const char *p) {
  int l = capture_to_close(ms);
  const char *res;
  ms->capture[l].len = s - ms->capture[l].init;  /* close capture */
  if ((res = match(ms, s, p)) == NULL)  /* match failed? */
    ms->capture[l].len = CAP_UNFINISHED;  /* undo capture */
  return res;
}


static const char *match_capture (MatchState *ms, const char *s, int l) {
  size_t len;
  l = check_capture(ms, l);
  len = cast_sizet(ms->capture[l].len);
  if ((size_t)(ms->src_end-s) >= len &&
      memcmp(ms->capture[l].init, s, len) == 0)
    return s+len;
  else return NULL;
}


static const char *match (MatchState *ms, const char *s, const char *p) {
  if (l_unlikely(ms->matchdepth-- == 0))
    lauL_error(ms->L, "pattern too complex");
  init: /* using goto to optimize tail recursion */
  if (p != ms->p_end) {  /* end of pattern? */
    switch (*p) {
      case '(': {  /* start capture */
        if (*(p + 1) == ')')  /* position capture? */
          s = start_capture(ms, s, p + 2, CAP_POSITION);
        else
          s = start_capture(ms, s, p + 1, CAP_UNFINISHED);
        break;
      }
      case ')': {  /* end capture */
        s = end_capture(ms, s, p + 1);
        break;
      }
      case '$': {
        if ((p + 1) != ms->p_end)  /* is the '$' the last char in pattern? */
          goto dflt;  /* no; go to default */
        s = (s == ms->src_end) ? s : NULL;  /* check end of string */
        break;
      }
      case L_ESC: {  /* escaped sequences not in the format class[*+?-]? */
        switch (*(p + 1)) {
          case 'b': {  /* balanced string? */
            s = matchbalance(ms, s, p + 2);
            if (s != NULL) {
              p += 4; goto init;  /* return match(ms, s, p + 4); */
            }  /* else fail (s == NULL) */
            break;
          }
          case 'f': {  /* frontier? */
            const char *ep; char previous;
            p += 2;
            if (l_unlikely(*p != '['))
              lauL_error(ms->L, "missing '[' after '%%f' in pattern");
            ep = classend(ms, p);  /* points to what is next */
            previous = (s == ms->src_init) ? '\0' : *(s - 1);
            if (!matchbracketclass(cast_uchar(previous), p, ep - 1) &&
               matchbracketclass(cast_uchar(*s), p, ep - 1)) {
              p = ep; goto init;  /* return match(ms, s, ep); */
            }
            s = NULL;  /* match failed */
            break;
          }
          case '0': case '1': case '2': case '3':
          case '4': case '5': case '6': case '7':
          case '8': case '9': {  /* capture results (%0-%9)? */
            s = match_capture(ms, s, cast_uchar(*(p + 1)));
            if (s != NULL) {
              p += 2; goto init;  /* return match(ms, s, p + 2) */
            }
            break;
          }
          default: goto dflt;
        }
        break;
      }
      default: dflt: {  /* pattern class plus optional suffix */
        const char *ep = classend(ms, p);  /* points to optional suffix */
        /* does not match at least once? */
        if (!singlematch(ms, s, p, ep)) {
          if (*ep == '*' || *ep == '?' || *ep == '-') {  /* accept empty? */
            p = ep + 1; goto init;  /* return match(ms, s, ep + 1); */
          }
          else  /* '+' or no suffix */
            s = NULL;  /* fail */
        }
        else {  /* matched once */
          switch (*ep) {  /* handle optional suffix */
            case '?': {  /* optional */
              const char *res;
              if ((res = match(ms, s + 1, ep + 1)) != NULL)
                s = res;
              else {
                p = ep + 1; goto init;  /* else return match(ms, s, ep + 1); */
              }
              break;
            }
            case '+':  /* 1 or more repetitions */
              s++;  /* 1 match already done */
              /* FALLTHROUGH */
            case '*':  /* 0 or more repetitions */
              s = max_expand(ms, s, p, ep);
              break;
            case '-':  /* 0 or more repetitions (minimum) */
              s = min_expand(ms, s, p, ep);
              break;
            default:  /* no suffix */
              s++; p = ep; goto init;  /* return match(ms, s + 1, ep); */
          }
        }
        break;
      }
    }
  }
  ms->matchdepth++;
  return s;
}


static const char *lmemfind (const char *s1, size_t l1,
                               const char *s2, size_t l2) {
  if (l2 == 0) return s1;  /* empty strings are everywhere */
  else if (l2 > l1) return NULL;  /* avoids a negative 'l1' */
  else {
    const char *init;  /* to search for a '*s2' inside 's1' */
    l2--;  /* 1st char will be checked by 'memchr' */
    l1 = l1-l2;  /* 's2' cannot be found after that */
    while (l1 > 0 && (init = (const char *)memchr(s1, *s2, l1)) != NULL) {
      init++;   /* 1st char is already checked */
      if (memcmp(init, s2+1, l2) == 0)
        return init-1;
      else {  /* correct 'l1' and 's1' to try again */
        l1 -= ct_diff2sz(init - s1);
        s1 = init;
      }
    }
    return NULL;  /* not found */
  }
}


/*
** get information about the i-th capture. If there are no captures
** and 'i==0', return information about the whole match, which
** is the range 's'..'e'. If the capture is a string, return
** its length and put its address in '*cap'. If it is an integer
** (a position), push it on the stack and return CAP_POSITION.
*/
static ptrdiff_t get_onecapture (MatchState *ms, int i, const char *s,
                              const char *e, const char **cap) {
  if (i >= ms->level) {
    if (l_unlikely(i != 0))
      lauL_error(ms->L, "invalid capture index %%%d", i + 1);
    *cap = s;
    return (e - s);
  }
  else {
    ptrdiff_t capl = ms->capture[i].len;
    *cap = ms->capture[i].init;
    if (l_unlikely(capl == CAP_UNFINISHED))
      lauL_error(ms->L, "unfinished capture");
    else if (capl == CAP_POSITION)
      lau_pushinteger(ms->L,
          ct_diff2S(ms->capture[i].init - ms->src_init) + 1);
    return capl;
  }
}


/*
** Push the i-th capture on the stack.
*/
static void push_onecapture (MatchState *ms, int i, const char *s,
                                                    const char *e) {
  const char *cap;
  ptrdiff_t l = get_onecapture(ms, i, s, e, &cap);
  if (l != CAP_POSITION)
    lau_pushlstring(ms->L, cap, cast_sizet(l));
  /* else position was already pushed */
}


static int push_captures (MatchState *ms, const char *s, const char *e) {
  int i;
  int nlevels = (ms->level == 0 && s) ? 1 : ms->level;
  lauL_checkstack(ms->L, nlevels, "too many captures");
  for (i = 0; i < nlevels; i++)
    push_onecapture(ms, i, s, e);
  return nlevels;  /* number of strings pushed */
}


/* check whether pattern has no special characters */
static int nospecials (const char *p, size_t l) {
  size_t upto = 0;
  do {
    if (strpbrk(p + upto, SPECIALS))
      return 0;  /* pattern has a special character */
    upto += strlen(p + upto) + 1;  /* may have more after \0 */
  } while (upto <= l);
  return 1;  /* no special chars found */
}


/*
** Prepare state for matches. These fields are not affected by each match.
*/
static void prepstate (MatchState *ms, lau_State *L,
                       const char *s, size_t ls, const char *p, size_t lp) {
  ms->L = L;
  ms->src_init = s;
  ms->src_end = s + ls;
  ms->p_end = p + lp;
}


/*
** (Re)prepare state for a match, setting fields that change during
** each match.
*/
static void reprepstate (MatchState *ms) {
  ms->matchdepth = MAXCCALLS;
  ms->level = 0;
}


static int str_find_aux (lau_State *L, int find) {
  size_t ls, lp;
  const char *s = lauL_checklstring(L, 1, &ls);
  const char *p = lauL_checklstring(L, 2, &lp);
  size_t init = posrelatI(lauL_optinteger(L, 3, 1), ls) - 1;
  if (init > ls) {  /* start after string's end? */
    lauL_pushfail(L);  /* cannot find anything */
    return 1;
  }
  /* explicit request or no special characters? */
  if (find && (lau_toboolean(L, 4) || nospecials(p, lp))) {
    /* do a plain search */
    const char *s2 = lmemfind(s + init, ls - init, p, lp);
    if (s2) {
      lau_pushinteger(L, ct_diff2S(s2 - s) + 1);
      lau_pushinteger(L, cast_st2S(ct_diff2sz(s2 - s) + lp));
      return 2;
    }
  }
  else {
    MatchState ms;
    const char *s1 = s + init;
    int anchor = (*p == '^');
    if (anchor) {
      p++; lp--;  /* skip anchor character */
    }
    prepstate(&ms, L, s, ls, p, lp);
    do {
      const char *res;
      reprepstate(&ms);
      if ((res=match(&ms, s1, p)) != NULL) {
        if (find) {
          lau_pushinteger(L, ct_diff2S(s1 - s) + 1);  /* start */
          lau_pushinteger(L, ct_diff2S(res - s));   /* end */
          return push_captures(&ms, NULL, 0) + 2;
        }
        else
          return push_captures(&ms, s1, res);
      }
    } while (s1++ < ms.src_end && !anchor);
  }
  lauL_pushfail(L);  /* not found */
  return 1;
}


static int str_find (lau_State *L) {
  return str_find_aux(L, 1);
}

static int str_match (lau_State *L) {
  return str_find_aux(L, 0);
}

static void add_s (MatchState *ms, lauL_Buffer *b, const char *s,
                                                   const char *e) {
  size_t l;
  lau_State *L = ms->L;
  const char *news = lau_tolstring(L, 3, &l);
  const char *p;
  while ((p = (char *)memchr(news, L_ESC, l)) != NULL) {
    lauL_addlstring(b, news, ct_diff2sz(p - news));
    p++;  /* skip ESC */
    if (*p == L_ESC)  /* '%%' */
      lauL_addchar(b, *p);
    else if (*p == '0')  /* '%0' */
        lauL_addlstring(b, s, ct_diff2sz(e - s));
    else if (isdigit(cast_uchar(*p))) {  /* '%n' */
      const char *cap;
      ptrdiff_t resl = get_onecapture(ms, *p - '1', s, e, &cap);
      if (resl == CAP_POSITION)
        lauL_addvalue(b);  /* add position to accumulated result */
      else
        lauL_addlstring(b, cap, cast_sizet(resl));
    }
    else
      lauL_error(L, "invalid use of '%c' in replacement string", L_ESC);
    l -= ct_diff2sz(p + 1 - news);
    news = p + 1;
  }
  lauL_addlstring(b, news, l);
}


/*
** Add the replacement value to the string buffer 'b'.
** Return true if the original string was changed. (Function calls and
** table indexing resulting in nil or false do not change the subject.)
*/
static int add_value (MatchState *ms, lauL_Buffer *b, const char *s,
                                      const char *e, int tr) {
  lau_State *L = ms->L;
  switch (tr) {
    case LAU_TFUNCTION: {  /* call the function */
      int n;
      lau_pushvalue(L, 3);  /* push the function */
      n = push_captures(ms, s, e);  /* all captures as arguments */
      lau_call(L, n, 1);  /* call it */
      break;
    }
    case LAU_TTABLE: {  /* index the table */
      push_onecapture(ms, 0, s, e);  /* first capture is the index */
      lau_gettable(L, 3);
      break;
    }
    default: {  /* LAU_TNUMBER or LAU_TSTRING */
      add_s(ms, b, s, e);  /* add value to the buffer */
      return 1;  /* something changed */
    }
  }
  if (!lau_toboolean(L, -1)) {  /* nil or false? */
    lau_pop(L, 1);  /* remove value */
    lauL_addlstring(b, s, ct_diff2sz(e - s));  /* keep original text */
    return 0;  /* no changes */
  }
  else if (l_unlikely(!lau_isstring(L, -1)))
    return lauL_error(L, "invalid replacement value (a %s)",
                         lauL_typename(L, -1));
  else {
    lauL_addvalue(b);  /* add result to accumulator */
    return 1;  /* something changed */
  }
}


static int str_gsub (lau_State *L) {
  size_t srcl, lp;
  const char *src = lauL_checklstring(L, 1, &srcl);  /* subject */
  const char *p = lauL_checklstring(L, 2, &lp);  /* pattern */
  const char *lastmatch = NULL;  /* end of last match */
  int tr = lau_type(L, 3);  /* replacement type */
  /* max replacements */
  lau_Integer max_s = lauL_optinteger(L, 4, cast_st2S(srcl) + 1);
  int anchor = (*p == '^');
  lau_Integer n = 0;  /* replacement count */
  int changed = 0;  /* change flag */
  MatchState ms;
  lauL_Buffer b;
  lauL_argexpected(L, tr == LAU_TNUMBER || tr == LAU_TSTRING ||
                   tr == LAU_TFUNCTION || tr == LAU_TTABLE, 3,
                      "string/function/table");
  lauL_buffinit(L, &b);
  if (anchor) {
    p++; lp--;  /* skip anchor character */
  }
  prepstate(&ms, L, src, srcl, p, lp);
  while (n < max_s) {
    const char *e;
    reprepstate(&ms);  /* (re)prepare state for new match */
    if ((e = match(&ms, src, p)) != NULL && e != lastmatch) {  /* match? */
      n++;
      changed = add_value(&ms, &b, src, e, tr) || changed;
      src = lastmatch = e;
    }
    else if (src < ms.src_end)  /* otherwise, skip one character */
      lauL_addchar(&b, *src++);
    else break;  /* end of subject */
    if (anchor) break;
  }
  if (!changed)  /* no changes? */
    lau_pushvalue(L, 1);  /* return original string */
  else {  /* something changed */
    lauL_addlstring(&b, src, ct_diff2sz(ms.src_end - src));
    lauL_pushresult(&b);  /* create and return new string */
  }
  lau_pushinteger(L, n);  /* number of substitutions */
  return 2;
}

/* }====================================================== */


static const lauL_Reg strlib[] = {
  {"find", str_find},
  {"match", str_match},
  {"len", str_len},
  {"lower", str_lower},
  {"sub", str_sub},
  {"gsub", str_gsub},
  {"upper", str_upper},
  {NULL, NULL}
};


static void createmetatable (lau_State *L) {
  /* table to be metatable for strings */
  lauL_newlibtable(L, stringmetamethods);
  lauL_setfuncs(L, stringmetamethods, 0);
  lau_pushliteral(L, "");  /* dummy string */
  lau_pushvalue(L, -2);  /* copy table */
  lau_setmetatable(L, -2);  /* set table as metatable for strings */
  lau_pop(L, 1);  /* pop dummy string */
  lau_pushvalue(L, -2);  /* get string library */
  lau_setfield(L, -2, "__index");  /* metatable.__index = string */
  lau_pop(L, 1);  /* pop metatable */
}


/*
** Open string library
*/
LAUMOD_API int lauopen_string (lau_State *L) {
  lauL_newlib(L, strlib);
  createmetatable(L);
  return 1;
}

