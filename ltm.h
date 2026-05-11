/*
** $Id: ltm.h $
** Tag methods
** See Copyright Notice in lau.h
*/

#ifndef ltm_h
#define ltm_h


#include "lobject.h"


/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER TM" and "ORDER OP"
*/
typedef enum {
  TM_INDEX,
  TM_NEWINDEX,
  TM_GC,
  TM_MODE,
  TM_LEN,
  TM_EQ,  /* last tag method with fast access */
  TM_ADD,
  TM_SUB,
  TM_MUL,
  TM_MOD,
  TM_POW,
  TM_DIV,
  TM_LT,
  TM_LE,
  TM_CONCAT,
  TM_CALL,
  TM_CLOSE,
  TM_N		/* number of elements in the enum */
} TMS;


/*
** Mask with 1 in all fast-access methods. A 1 in any of these bits
** in the flag of a (meta)table means the metatable does not have the
** corresponding metamethod field. (Bit 6 of the flag indicates that
** the table is using the dummy node.)
*/
#define maskflags	cast_byte(~(~0u << (TM_EQ + 1)))


/*
** Test whether there is no tagmethod.
** (Because tagmethods use raw accesses, the result may be an "empty" nil.)
*/
#define notm(tm)	ttisnil(tm)

#define checknoTM(mt,e)	((mt) == NULL || (mt)->flags & (1u<<(e)))

#define gfasttm(g,mt,e)  \
  (checknoTM(mt, e) ? NULL : lauT_gettm(mt, e, (g)->tmname[e]))

#define fasttm(l,mt,e)	gfasttm(G(l), mt, e)

#define ttypename(x)	lauT_typenames_[(x) + 1]

LAUI_DDEC(const char *const lauT_typenames_[LAU_TOTALTYPES];)


LAUI_FUNC const char *lauT_objtypename (lau_State *L, const TValue *o);

LAUI_FUNC const TValue *lauT_gettm (Table *events, TMS event, TString *ename);
LAUI_FUNC const TValue *lauT_gettmbyobj (lau_State *L, const TValue *o,
                                                       TMS event);
LAUI_FUNC void lauT_init (lau_State *L);

LAUI_FUNC void lauT_callTM (lau_State *L, const TValue *f, const TValue *p1,
                            const TValue *p2, const TValue *p3);
LAUI_FUNC lu_byte lauT_callTMres (lau_State *L, const TValue *f,
                               const TValue *p1, const TValue *p2, StkId p3);
LAUI_FUNC void lauT_trybinTM (lau_State *L, const TValue *p1, const TValue *p2,
                              StkId res, TMS event);
LAUI_FUNC void lauT_tryconcatTM (lau_State *L);
LAUI_FUNC void lauT_trybinassocTM (lau_State *L, const TValue *p1,
       const TValue *p2, int inv, StkId res, TMS event);
LAUI_FUNC void lauT_trybiniTM (lau_State *L, const TValue *p1, lau_Integer i2,
                               int inv, StkId res, TMS event);
LAUI_FUNC int lauT_callorderTM (lau_State *L, const TValue *p1,
                                const TValue *p2, TMS event);
LAUI_FUNC int lauT_callorderiTM (lau_State *L, const TValue *p1, int v2,
                                 int inv, int isfloat, TMS event);

LAUI_FUNC void lauT_adjustvarargs (lau_State *L, struct CallInfo *ci,
                                                 const Proto *p);
LAUI_FUNC void lauT_getvararg (CallInfo *ci, StkId ra, TValue *rc);
LAUI_FUNC void lauT_getvarargs (lau_State *L, struct CallInfo *ci, StkId where,
                                              int wanted, int vatab);


#endif
