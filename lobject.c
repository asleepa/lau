/*
** $Id: lobject.c $
** Some generic functions over Lau objects
** See Copyright Notice in lau.h
*/

#define lobject_c
#define LAU_CORE

#include "lprefix.h"


#include <float.h>
#include <locale.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lau.h"

#include "lctype.h"
#include "ldebug.h"
#include "ldo.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "lvm.h"


/*
** Computes ceil(log2(x)), which is the smallest integer n such that
** x <= (1 << n).
*/
lu_byte lauO_ceillog2 (unsigned int x) {
  static const lu_byte log_2[256] = {  /* log_2[i - 1] = ceil(log2(i)) */
    0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8
  };
  int l = 0;
  x--;
  while (x >= 256) { l += 8; x >>= 8; }
  return cast_byte(l + log_2[x]);
}

/*
** Encodes 'p'% as a floating-point byte, represented as (eeeexxxx).
** The exponent is represented using excess-7. Mimicking IEEE 754, the
** representation normalizes the number when possible, assuming an extra
** 1 before the mantissa (xxxx) and adding one to the exponent (eeee)
** to signal that. So, the real value is (1xxxx) * 2^(eeee - 7 - 1) if
** eeee != 0, and (xxxx) * 2^-7 otherwise (subnormal numbers).
*/
lu_byte lauO_codeparam (unsigned int p) {
  if (p >= (cast(lu_mem, 0x1F) << (0xF - 7 - 1)) * 100u)  /* overflow? */
    return 0xFF;  /* return maximum value */
  else {
    p = (cast(l_uint32, p) * 128 + 99) / 100;  /* round up the division */
    if (p < 0x10) {  /* subnormal number? */
      /* exponent bits are already zero; nothing else to do */
      return cast_byte(p);
    }
    else {  /* p >= 0x10 implies ceil(log2(p + 1)) >= 5 */
      /* preserve 5 bits in 'p' */
      unsigned log = lauO_ceillog2(p + 1) - 5u;
      return cast_byte(((p >> log) - 0x10) | ((log + 1) << 4));
    }
  }
}


/*
** Computes 'p' times 'x', where 'p' is a floating-point byte. Roughly,
** we have to multiply 'x' by the mantissa and then shift accordingly to
** the exponent.  If the exponent is positive, both the multiplication
** and the shift increase 'x', so we have to care only about overflows.
** For negative exponents, however, multiplying before the shift keeps
** more significant bits, as long as the multiplication does not
** overflow, so we check which order is best.
*/
l_mem lauO_applyparam (lu_byte p, l_mem x) {
  int m = p & 0xF;  /* mantissa */
  int e = (p >> 4);  /* exponent */
  if (e > 0) {  /* normalized? */
    e--;  /* correct exponent */
    m += 0x10;  /* correct mantissa; maximum value is 0x1F */
  }
  e -= 7;  /* correct excess-7 */
  if (e >= 0) {
    if (x < (MAX_LMEM / 0x1F) >> e)  /* no overflow? */
      return (x * m) << e;  /* order doesn't matter here */
    else  /* real overflow */
      return MAX_LMEM;
  }
  else {  /* negative exponent */
    e = -e;
    if (x < MAX_LMEM / 0x1F)  /* multiplication cannot overflow? */
      return (x * m) >> e;  /* multiplying first gives more precision */
    else if ((x >> e) <  MAX_LMEM / 0x1F)  /* cannot overflow after shift? */
      return (x >> e) * m;
    else  /* real overflow */
      return MAX_LMEM;
  }
}


static lau_Integer intarith (lau_State *L, int op, lau_Integer v1,
                                                   lau_Integer v2) {
  switch (op) {
    case LAU_OPADD: return intop(+, v1, v2);
    case LAU_OPSUB:return intop(-, v1, v2);
    case LAU_OPMUL:return intop(*, v1, v2);
    case LAU_OPMOD: return lauV_mod(L, v1, v2);
    case LAU_OPIDIV: return lauV_idiv(L, v1, v2);
    case LAU_OPBAND: return intop(&, v1, v2);
    case LAU_OPBOR: return intop(|, v1, v2);
    case LAU_OPBXOR: return intop(^, v1, v2);
    case LAU_OPSHL: return lauV_shiftl(v1, v2);
    case LAU_OPSHR: return lauV_shiftr(v1, v2);
    case LAU_OPUNM: return intop(-, 0, v1);
    case LAU_OPBNOT: return intop(^, ~l_castS2U(0), v1);
    default: lau_assert(0); return 0;
  }
}


static lau_Number numarith (lau_State *L, int op, lau_Number v1,
                                                  lau_Number v2) {
  switch (op) {
    case LAU_OPADD: return laui_numadd(L, v1, v2);
    case LAU_OPSUB: return laui_numsub(L, v1, v2);
    case LAU_OPMUL: return laui_nummul(L, v1, v2);
    case LAU_OPDIV: return laui_numdiv(L, v1, v2);
    case LAU_OPPOW: return laui_numpow(L, v1, v2);
    case LAU_OPIDIV: return laui_numidiv(L, v1, v2);
    case LAU_OPUNM: return laui_numunm(L, v1);
    case LAU_OPMOD: return lauV_modf(L, v1, v2);
    default: lau_assert(0); return 0;
  }
}


int lauO_rawarith (lau_State *L, int op, const TValue *p1, const TValue *p2,
                   TValue *res) {
  switch (op) {
    case LAU_OPBAND: case LAU_OPBOR: case LAU_OPBXOR:
    case LAU_OPSHL: case LAU_OPSHR:
    case LAU_OPBNOT: {  /* operate only on integers */
      lau_Integer i1; lau_Integer i2;
      if (tointegerns(p1, &i1) && tointegerns(p2, &i2)) {
        setivalue(res, intarith(L, op, i1, i2));
        return 1;
      }
      else return 0;  /* fail */
    }
    case LAU_OPDIV: case LAU_OPPOW: {  /* operate only on floats */
      lau_Number n1; lau_Number n2;
      if (tonumberns(p1, n1) && tonumberns(p2, n2)) {
        setfltvalue(res, numarith(L, op, n1, n2));
        return 1;
      }
      else return 0;  /* fail */
    }
    default: {  /* other operations */
      lau_Number n1; lau_Number n2;
      if (ttisinteger(p1) && ttisinteger(p2)) {
        setivalue(res, intarith(L, op, ivalue(p1), ivalue(p2)));
        return 1;
      }
      else if (tonumberns(p1, n1) && tonumberns(p2, n2)) {
        setfltvalue(res, numarith(L, op, n1, n2));
        return 1;
      }
      else return 0;  /* fail */
    }
  }
}


void lauO_arith (lau_State *L, int op, const TValue *p1, const TValue *p2,
                 StkId res) {
  if (!lauO_rawarith(L, op, p1, p2, s2v(res))) {
    /* could not perform raw operation; try metamethod */
    lauT_trybinTM(L, p1, p2, res, cast(TMS, (op - LAU_OPADD) + TM_ADD));
  }
}


lu_byte lauO_hexavalue (int c) {
  lau_assert(lisxdigit(c));
  if (lisdigit(c)) return cast_byte(c - '0');
  else return cast_byte((ltolower(c) - 'a') + 10);
}


static int isneg (const char **s) {
  if (**s == '-') { (*s)++; return 1; }
  else if (**s == '+') (*s)++;
  return 0;
}



/*
** {==================================================================
** Lau's implementation for 'lau_strx2number'
** ===================================================================
*/

#if !defined(lau_strx2number)

/* maximum number of significant digits to read (to avoid overflows
   even with single floats) */
#define MAXSIGDIG	30

/*
** convert a hexadecimal numeric string to a number, following
** C99 specification for 'strtod'
*/
static lau_Number lau_strx2number (const char *s, char **endptr) {
  int dot = lau_getlocaledecpoint();
  lau_Number r = l_mathop(0.0);  /* result (accumulator) */
  int sigdig = 0;  /* number of significant digits */
  int nosigdig = 0;  /* number of non-significant digits */
  int e = 0;  /* exponent correction */
  int neg;  /* 1 if number is negative */
  int hasdot = 0;  /* true after seen a dot */
  *endptr = cast_charp(s);  /* nothing is valid yet */
  while (lisspace(cast_uchar(*s))) s++;  /* skip initial spaces */
  neg = isneg(&s);  /* check sign */
  if (!(*s == '0' && (*(s + 1) == 'x' || *(s + 1) == 'X')))  /* check '0x' */
    return l_mathop(0.0);  /* invalid format (no '0x') */
  for (s += 2; ; s++) {  /* skip '0x' and read numeral */
    if (*s == dot) {
      if (hasdot) break;  /* second dot? stop loop */
      else hasdot = 1;
    }
    else if (lisxdigit(cast_uchar(*s))) {
      if (sigdig == 0 && *s == '0')  /* non-significant digit (zero)? */
        nosigdig++;
      else if (++sigdig <= MAXSIGDIG)  /* can read it without overflow? */
          r = (r * l_mathop(16.0)) + lauO_hexavalue(*s);
      else e++;  /* too many digits; ignore, but still count for exponent */
      if (hasdot) e--;  /* decimal digit? correct exponent */
    }
    else break;  /* neither a dot nor a digit */
  }
  if (nosigdig + sigdig == 0)  /* no digits? */
    return l_mathop(0.0);  /* invalid format */
  *endptr = cast_charp(s);  /* valid up to here */
  e *= 4;  /* each digit multiplies/divides value by 2^4 */
  if (*s == 'p' || *s == 'P') {  /* exponent part? */
    int exp1 = 0;  /* exponent value */
    int neg1;  /* exponent sign */
    s++;  /* skip 'p' */
    neg1 = isneg(&s);  /* sign */
    if (!lisdigit(cast_uchar(*s)))
      return l_mathop(0.0);  /* invalid; must have at least one digit */
    while (lisdigit(cast_uchar(*s)))  /* read exponent */
      exp1 = exp1 * 10 + *(s++) - '0';
    if (neg1) exp1 = -exp1;
    e += exp1;
    *endptr = cast_charp(s);  /* valid up to here */
  }
  if (neg) r = -r;
  return l_mathop(ldexp)(r, e);
}

#endif
/* }====================================================== */


/* maximum length of a numeral to be converted to a number */
#if !defined (L_MAXLENNUM)
#define L_MAXLENNUM	200
#endif

/*
** Convert string 's' to a Lau number (put in 'result'). Return NULL on
** fail or the address of the ending '\0' on success. ('mode' == 'x')
** means a hexadecimal numeral.
*/
static const char *l_str2dloc (const char *s, lau_Number *result, int mode) {
  char *endptr;
  *result = (mode == 'x') ? lau_strx2number(s, &endptr)  /* try to convert */
                          : lau_str2number(s, &endptr);
  if (endptr == s) return NULL;  /* nothing recognized? */
  while (lisspace(cast_uchar(*endptr))) endptr++;  /* skip trailing spaces */
  return (*endptr == '\0') ? endptr : NULL;  /* OK iff no trailing chars */
}


/*
** Convert string 's' to a Lau number (put in 'result') handling the
** current locale.
** This function accepts both the current locale or a dot as the radix
** mark. If the conversion fails, it may mean number has a dot but
** locale accepts something else. In that case, the code copies 's'
** to a buffer (because 's' is read-only), changes the dot to the
** current locale radix mark, and tries to convert again.
** The variable 'mode' checks for special characters in the string:
** - 'n' means 'inf' or 'nan' (which should be rejected)
** - 'x' means a hexadecimal numeral
** - '.' just optimizes the search for the common case (no special chars)
*/
static const char *l_str2d (const char *s, lau_Number *result) {
  const char *endptr;
  const char *pmode = strpbrk(s, ".xXnN");  /* look for special chars */
  int mode = pmode ? ltolower(cast_uchar(*pmode)) : 0;
  if (mode == 'n')  /* reject 'inf' and 'nan' */
    return NULL;
  endptr = l_str2dloc(s, result, mode);  /* try to convert */
  if (endptr == NULL) {  /* failed? may be a different locale */
    char buff[L_MAXLENNUM + 1];
    const char *pdot = strchr(s, '.');
    if (pdot == NULL || strlen(s) > L_MAXLENNUM)
      return NULL;  /* string too long or no dot; fail */
    strcpy(buff, s);  /* copy string to buffer */
    buff[pdot - s] = lau_getlocaledecpoint();  /* correct decimal point */
    endptr = l_str2dloc(buff, result, mode);  /* try again */
    if (endptr != NULL)
      endptr = s + (endptr - buff);  /* make relative to 's' */
  }
  return endptr;
}


#define MAXBY10		cast(lau_Unsigned, LAU_MAXINTEGER / 10)
#define MAXLASTD	cast_int(LAU_MAXINTEGER % 10)

static const char *l_str2int (const char *s, lau_Integer *result) {
  lau_Unsigned a = 0;
  int empty = 1;
  int neg;
  while (lisspace(cast_uchar(*s))) s++;  /* skip initial spaces */
  neg = isneg(&s);
  if (s[0] == '0' &&
      (s[1] == 'x' || s[1] == 'X')) {  /* hex? */
    s += 2;  /* skip '0x' */
    for (; lisxdigit(cast_uchar(*s)); s++) {
      a = a * 16 + lauO_hexavalue(*s);
      empty = 0;
    }
  }
  else {  /* decimal */
    for (; lisdigit(cast_uchar(*s)); s++) {
      int d = *s - '0';
      if (a >= MAXBY10 && (a > MAXBY10 || d > MAXLASTD + neg))  /* overflow? */
        return NULL;  /* do not accept it (as integer) */
      a = a * 10 + cast_uint(d);
      empty = 0;
    }
  }
  while (lisspace(cast_uchar(*s))) s++;  /* skip trailing spaces */
  if (empty || *s != '\0') return NULL;  /* something wrong in the numeral */
  else {
    *result = l_castU2S((neg) ? 0u - a : a);
    return s;
  }
}


size_t lauO_str2num (const char *s, TValue *o) {
  lau_Integer i; lau_Number n;
  const char *e;
  if ((e = l_str2int(s, &i)) != NULL) {  /* try as an integer */
    setivalue(o, i);
  }
  else if ((e = l_str2d(s, &n)) != NULL) {  /* else try as a float */
    setfltvalue(o, n);
  }
  else
    return 0;  /* conversion failed */
  return ct_diff2sz(e - s) + 1;  /* success; return string size */
}


int lauO_utf8esc (char *buff, l_uint32 x) {
  int n = 1;  /* number of bytes put in buffer (backwards) */
  lau_assert(x <= 0x7FFFFFFFu);
  if (x < 0x80)  /* ASCII? */
    buff[UTF8BUFFSZ - 1] = cast_char(x);
  else {  /* need continuation bytes */
    unsigned int mfb = 0x3f;  /* maximum that fits in first byte */
    do {  /* add continuation bytes */
      buff[UTF8BUFFSZ - (n++)] = cast_char(0x80 | (x & 0x3f));
      x >>= 6;  /* remove added bits */
      mfb >>= 1;  /* now there is one less bit available in first byte */
    } while (x > mfb);  /* still needs continuation byte? */
    buff[UTF8BUFFSZ - n] = cast_char((~mfb << 1) | x);  /* add first byte */
  }
  return n;
}


/*
** The size of the buffer for the conversion of a number to a string
** 'LAU_N2SBUFFSZ' must be enough to accommodate both LAU_INTEGER_FMT
** and LAU_NUMBER_FMT.  For a long long int, this is 19 digits plus a
** sign and a final '\0', adding to 21. For a long double, it can go to
** a sign, the dot, an exponent letter, an exponent sign, 4 exponent
** digits, the final '\0', plus the significant digits, which are
** approximately the *_DIG attribute.
*/
#if LAU_N2SBUFFSZ < (20 + l_floatatt(DIG))
#error "invalid value for LAU_N2SBUFFSZ"
#endif


/*
** Convert a float to a string, adding it to a buffer. First try with
** a not too large number of digits, to avoid noise (for instance,
** 1.1 going to "1.1000000000000001"). If that lose precision, so
** that reading the result back gives a different number, then do the
** conversion again with extra precision. Moreover, if the numeral looks
** like an integer (without a decimal point or an exponent), add ".0" to
** its end.
*/
static int tostringbuffFloat (lau_Number n, char *buff) {
  /* first conversion */
  int len = l_sprintf(buff, LAU_N2SBUFFSZ, LAU_NUMBER_FMT,
                            (LAUI_UACNUMBER)n);
  lau_Number check = lau_str2number(buff, NULL);  /* read it back */
  if (check != n) {  /* not enough precision? */
    /* convert again with more precision */
    len = l_sprintf(buff, LAU_N2SBUFFSZ, LAU_NUMBER_FMT_N,
                          (LAUI_UACNUMBER)n);
  }
  /* looks like an integer? */
  if (buff[strspn(buff, "-0123456789")] == '\0') {
    buff[len++] = lau_getlocaledecpoint();
    buff[len++] = '0';  /* adds '.0' to result */
  }
  return len;
}


/*
** Convert a number object to a string, adding it to a buffer.
*/
unsigned lauO_tostringbuff (const TValue *obj, char *buff) {
  int len;
  lau_assert(ttisnumber(obj));
  if (ttisinteger(obj))
    len = lau_integer2str(buff, LAU_N2SBUFFSZ, ivalue(obj));
  else
    len = tostringbuffFloat(fltvalue(obj), buff);
  lau_assert(len < LAU_N2SBUFFSZ);
  return cast_uint(len);
}


/*
** Convert a number object to a Lau string, replacing the value at 'obj'
*/
void lauO_tostring (lau_State *L, TValue *obj) {
  char buff[LAU_N2SBUFFSZ];
  unsigned len = lauO_tostringbuff(obj, buff);
  setsvalue(L, obj, lauS_newlstr(L, buff, len));
}




/*
** {==================================================================
** 'lauO_pushvfstring'
** ===================================================================
*/

/*
** Size for buffer space used by 'lauO_pushvfstring'. It should be
** (LAU_IDSIZE + LAU_N2SBUFFSZ) + a minimal space for basic messages,
** so that 'lauG_addinfo' can work directly on the static buffer.
*/
#define BUFVFS		cast_uint(LAU_IDSIZE + LAU_N2SBUFFSZ + 95)

/*
** Buffer used by 'lauO_pushvfstring'. 'err' signals an error while
** building result (memory error [1] or buffer overflow [2]).
*/
typedef struct BuffFS {
  lau_State *L;
  char *b;
  size_t buffsize;
  size_t blen;  /* length of string in 'buff' */
  int err;
  char space[BUFVFS];  /* initial buffer */
} BuffFS;


static void initbuff (lau_State *L, BuffFS *buff) {
  buff->L = L;
  buff->b = buff->space;
  buff->buffsize = sizeof(buff->space);
  buff->blen = 0;
  buff->err = 0;
}


/*
** Push final result from 'lauO_pushvfstring'. This function may raise
** errors explicitly or through memory errors, so it must run protected.
*/
static void pushbuff (lau_State *L, void *ud) {
  BuffFS *buff = cast(BuffFS*, ud);
  switch (buff->err) {
    case 1:  /* memory error */
      lauD_throw(L, LAU_ERRMEM);
      break;
    case 2:  /* length overflow: Add "..." at the end of result */
      if (buff->buffsize - buff->blen < 3)
        strcpy(buff->b + buff->blen - 3, "...");  /* 'blen' must be > 3 */
      else {  /* there is enough space left for the "..." */
        strcpy(buff->b + buff->blen, "...");
        buff->blen += 3;
      }
      /* FALLTHROUGH */
    default: {  /* no errors, but it can raise one creating the new string */
      TString *ts = lauS_newlstr(L, buff->b, buff->blen);
      setsvalue2s(L, L->top.p, ts);
      L->top.p++;
    }
  }
}


static const char *clearbuff (BuffFS *buff) {
  lau_State *L = buff->L;
  const char *res;
  if (lauD_rawrunprotected(L, pushbuff, buff) != LAU_OK)  /* errors? */
    res = NULL;  /* error message is on the top of the stack */
  else
    res = getstr(tsvalue(s2v(L->top.p - 1)));
  if (buff->b != buff->space)  /* using dynamic buffer? */
    lauM_freearray(L, buff->b, buff->buffsize);  /* free it */
  return res;
}


static void addstr2buff (BuffFS *buff, const char *str, size_t slen) {
  size_t left = buff->buffsize - buff->blen;  /* space left in the buffer */
  if (buff->err)  /* do nothing else after an error */
    return;
  if (slen > left) {  /* new string doesn't fit into current buffer? */
    if (slen > ((MAX_SIZE/2) - buff->blen)) {  /* overflow? */
      memcpy(buff->b + buff->blen, str, left);  /* copy what it can */
      buff->blen = buff->buffsize;
      buff->err = 2;  /* doesn't add anything else */
      return;
    }
    else {
      size_t newsize = buff->buffsize + slen;  /* limited to MAX_SIZE/2 */
      char *newb =
        (buff->b == buff->space)  /* still using static space? */
        ? lauM_reallocvector(buff->L, NULL, 0, newsize, char)
        : lauM_reallocvector(buff->L, buff->b, buff->buffsize, newsize,
                                                               char);
      if (newb == NULL) {  /* allocation error? */
        buff->err = 1;  /* signal a memory error */
        return;
      }
      if (buff->b == buff->space)  /* new buffer (not reallocated)? */
        memcpy(newb, buff->b, buff->blen);  /* copy previous content */
      buff->b = newb;  /* set new (larger) buffer... */
      buff->buffsize = newsize;  /* ...and its new size */
    }
  }
  memcpy(buff->b + buff->blen, str, slen);  /* copy new content */
  buff->blen += slen;
}


/*
** Add a numeral to the buffer.
*/
static void addnum2buff (BuffFS *buff, TValue *num) {
  char numbuff[LAU_N2SBUFFSZ];
  unsigned len = lauO_tostringbuff(num, numbuff);
  addstr2buff(buff, numbuff, len);
}


/*
** this function handles only '%d', '%c', '%f', '%p', '%s', and '%%'
   conventional formats, plus Lau-specific '%I' and '%U'
*/
const char *lauO_pushvfstring (lau_State *L, const char *fmt, va_list argp) {
  BuffFS buff;  /* holds last part of the result */
  const char *e;  /* points to next '%' */
  initbuff(L, &buff);
  while ((e = strchr(fmt, '%')) != NULL) {
    addstr2buff(&buff, fmt, ct_diff2sz(e - fmt));  /* add 'fmt' up to '%' */
    switch (*(e + 1)) {  /* conversion specifier */
      case 's': {  /* zero-terminated string */
        const char *s = va_arg(argp, char *);
        if (s == NULL) s = "(null)";
        addstr2buff(&buff, s, strlen(s));
        break;
      }
      case 'c': {  /* an 'int' as a character */
        char c = cast_char(va_arg(argp, int));
        addstr2buff(&buff, &c, sizeof(char));
        break;
      }
      case 'd': {  /* an 'int' */
        TValue num;
        setivalue(&num, va_arg(argp, int));
        addnum2buff(&buff, &num);
        break;
      }
      case 'I': {  /* a 'lau_Integer' */
        TValue num;
        setivalue(&num, cast_Integer(va_arg(argp, l_uacInt)));
        addnum2buff(&buff, &num);
        break;
      }
      case 'f': {  /* a 'lau_Number' */
        TValue num;
        setfltvalue(&num, cast_num(va_arg(argp, l_uacNumber)));
        addnum2buff(&buff, &num);
        break;
      }
      case 'p': {  /* a pointer */
        char bf[LAU_N2SBUFFSZ];  /* enough space for '%p' */
        void *p = va_arg(argp, void *);
        int len = lau_pointer2str(bf, LAU_N2SBUFFSZ, p);
        addstr2buff(&buff, bf, cast_uint(len));
        break;
      }
      case 'U': {  /* an 'unsigned long' as a UTF-8 sequence */
        char bf[UTF8BUFFSZ];
        unsigned long arg = va_arg(argp, unsigned long);
        int len = lauO_utf8esc(bf, cast(l_uint32, arg));
        addstr2buff(&buff, bf + UTF8BUFFSZ - len, cast_uint(len));
        break;
      }
      case '%': {
        addstr2buff(&buff, "%", 1);
        break;
      }
      default: {
        addstr2buff(&buff, e, 2);  /* keep unknown format in the result */
        break;
      }
    }
    fmt = e + 2;  /* skip '%' and the specifier */
  }
  addstr2buff(&buff, fmt, strlen(fmt));  /* rest of 'fmt' */
  return clearbuff(&buff);  /* empty buffer into a new string */
}


const char *lauO_pushfstring (lau_State *L, const char *fmt, ...) {
  const char *msg;
  va_list argp;
  va_start(argp, fmt);
  msg = lauO_pushvfstring(L, fmt, argp);
  va_end(argp);
  if (msg == NULL)  /* error? */
    lauD_throw(L, LAU_ERRMEM);
  return msg;
}

/* }================================================================== */


#define RETS	"..."
#define PRE	"[string \""
#define POS	"\"]"

#define addstr(a,b,l)	( memcpy(a,b,(l) * sizeof(char)), a += (l) )

void lauO_chunkid (char *out, const char *source, size_t srclen) {
  size_t bufflen = LAU_IDSIZE;  /* free space in buffer */
  if (*source == '=') {  /* 'literal' source */
    if (srclen <= bufflen)  /* small enough? */
      memcpy(out, source + 1, srclen * sizeof(char));
    else {  /* truncate it */
      addstr(out, source + 1, bufflen - 1);
      *out = '\0';
    }
  }
  else if (*source == '@') {  /* file name */
    if (srclen <= bufflen)  /* small enough? */
      memcpy(out, source + 1, srclen * sizeof(char));
    else {  /* add '...' before rest of name */
      addstr(out, RETS, LL(RETS));
      bufflen -= LL(RETS);
      memcpy(out, source + 1 + srclen - bufflen, bufflen * sizeof(char));
    }
  }
  else {  /* string; format as [string "source"] */
    const char *nl = strchr(source, '\n');  /* find first new line (if any) */
    addstr(out, PRE, LL(PRE));  /* add prefix */
    bufflen -= LL(PRE RETS POS) + 1;  /* save space for prefix+suffix+'\0' */
    if (srclen < bufflen && nl == NULL) {  /* small one-line source? */
      addstr(out, source, srclen);  /* keep it */
    }
    else {
      if (nl != NULL)
        srclen = ct_diff2sz(nl - source);  /* stop at first newline */
      if (srclen > bufflen) srclen = bufflen;
      addstr(out, source, srclen);
      addstr(out, RETS, LL(RETS));
    }
    memcpy(out, POS, (LL(POS) + 1) * sizeof(char));
  }
}

