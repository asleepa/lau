/*
** $Id: lstring.h $
** String table (keep all strings handled by Lau)
** See Copyright Notice in lau.h
*/

#ifndef lstring_h
#define lstring_h

#include "lgc.h"
#include "lobject.h"
#include "lstate.h"


/*
** Memory-allocation error message must be preallocated (it cannot
** be created after memory is exhausted)
*/
#define MEMERRMSG       "not enough memory"


/*
** Maximum length for short strings, that is, strings that are
** internalized. (Cannot be smaller than reserved words or tags for
** metamethods, as these strings must be internalized;
** #("function") = 8, #("__newindex") = 10.)
*/
#if !defined(LAUI_MAXSHORTLEN)
#define LAUI_MAXSHORTLEN	40
#endif


/*
** Size of a short TString: Size of the header plus space for the string
** itself (including final '\0').
*/
#define sizestrshr(l)  \
	(offsetof(TString, contents) + ((l) + 1) * sizeof(char))


#define lauS_newliteral(L, s)	(lauS_newlstr(L, "" s, \
                                 (sizeof(s)/sizeof(char))-1))


/*
** test whether a string is a reserved word
*/
#define isreserved(s)	(strisshr(s) && (s)->extra > 0)


/*
** equality for short strings, which are always internalized
*/
#define eqshrstr(a,b)	check_exp((a)->tt == LAU_VSHRSTR, (a) == (b))


LAUI_FUNC unsigned lauS_hashlongstr (TString *ts);
LAUI_FUNC int lauS_eqstr (TString *a, TString *b);
LAUI_FUNC void lauS_resize (lau_State *L, int newsize);
LAUI_FUNC void lauS_clearcache (global_State *g);
LAUI_FUNC void lauS_init (lau_State *L);
LAUI_FUNC void lauS_remove (lau_State *L, TString *ts);
LAUI_FUNC Udata *lauS_newudata (lau_State *L, size_t s,
                                              unsigned short nuvalue);
LAUI_FUNC TString *lauS_newlstr (lau_State *L, const char *str, size_t l);
LAUI_FUNC TString *lauS_new (lau_State *L, const char *str);
LAUI_FUNC TString *lauS_createlngstrobj (lau_State *L, size_t l);
LAUI_FUNC TString *lauS_newextlstr (lau_State *L,
		const char *s, size_t len, lau_Alloc falloc, void *ud);
LAUI_FUNC size_t lauS_sizelngstr (size_t len, int kind);
LAUI_FUNC TString *lauS_normstr (lau_State *L, TString *ts);

#endif
