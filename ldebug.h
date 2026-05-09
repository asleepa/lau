/*
** $Id: ldebug.h $
** Auxiliary functions from Debug Interface module
** See Copyright Notice in lau.h
*/

#ifndef ldebug_h
#define ldebug_h


#include "lstate.h"


#define pcRel(pc, p)	(cast_int((pc) - (p)->code) - 1)


/* Active Lau function (given call info) */
#define ci_func(ci)		(clLvalue(s2v((ci)->func.p)))


#define resethookcount(L)	(L->hookcount = L->basehookcount)

/*
** mark for entries in 'lineinfo' array that has absolute information in
** 'abslineinfo' array
*/
#define ABSLINEINFO	(-0x80)


/*
** MAXimum number of successive Instructions WiTHout ABSolute line
** information. (A power of two allows fast divisions.)
*/
#if !defined(MAXIWTHABS)
#define MAXIWTHABS	128
#endif


LAUI_FUNC int lauG_getfuncline (const Proto *f, int pc);
LAUI_FUNC const char *lauG_findlocal (lau_State *L, CallInfo *ci, int n,
                                                    StkId *pos);
LAUI_FUNC l_noret lauG_typeerror (lau_State *L, const TValue *o,
                                                const char *opname);
LAUI_FUNC l_noret lauG_callerror (lau_State *L, const TValue *o);
LAUI_FUNC l_noret lauG_forerror (lau_State *L, const TValue *o,
                                               const char *what);
LAUI_FUNC l_noret lauG_concaterror (lau_State *L, const TValue *p1,
                                                  const TValue *p2);
LAUI_FUNC l_noret lauG_opinterror (lau_State *L, const TValue *p1,
                                                 const TValue *p2,
                                                 const char *msg);
LAUI_FUNC l_noret lauG_tointerror (lau_State *L, const TValue *p1,
                                                 const TValue *p2);
LAUI_FUNC l_noret lauG_ordererror (lau_State *L, const TValue *p1,
                                                 const TValue *p2);
LAUI_FUNC l_noret lauG_errnnil (lau_State *L, LClosure *cl, int k);
LAUI_FUNC l_noret lauG_runerror (lau_State *L, const char *fmt, ...);
LAUI_FUNC const char *lauG_addinfo (lau_State *L, const char *msg,
                                                  TString *src, int line);
LAUI_FUNC l_noret lauG_errormsg (lau_State *L);
LAUI_FUNC int lauG_traceexec (lau_State *L, const Instruction *pc);
LAUI_FUNC int lauG_tracecall (lau_State *L);


#endif
