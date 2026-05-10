/*
** $Id: lcode.h $
** Code generator for Lau
** See Copyright Notice in lau.h
*/

#ifndef lcode_h
#define lcode_h

#include "llex.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"


/*
** Marks the end of a patch list. It is an invalid value both as an absolute
** address, and as a list link (would link an element to itself).
*/
#define NO_JUMP (-1)


/*
** grep "ORDER OPR" if you change these enums  (ORDER OP)
*/
typedef enum BinOpr {
  /* arithmetic operators */
  OPR_ADD, OPR_SUB, OPR_MUL, OPR_MOD, OPR_POW,
  OPR_DIV, OPR_IDIV,
  /* bitwise operators */
  OPR_BAND, OPR_BOR, OPR_BXOR,
  OPR_SHL, OPR_SHR,
  /* string operator */
  OPR_CONCAT,
  /* comparison operators */
  OPR_EQ, OPR_LT, OPR_LE,
  OPR_NE, OPR_GT, OPR_GE,
  /* logical operators */
  OPR_AND, OPR_OR,
  OPR_NOBINOPR
} BinOpr;


/* true if operation is foldable (that is, it is arithmetic or bitwise) */
#define foldbinop(op)	((op) <= OPR_SHR)


#define lauK_codeABC(fs,o,a,b,c)	lauK_codeABCk(fs,o,a,b,c,0)


typedef enum UnOpr { OPR_MINUS, OPR_BNOT, OPR_NOT, OPR_LEN, OPR_NOUNOPR } UnOpr;


/* get (pointer to) instruction of given 'expdesc' */
#define getinstruction(fs,e)	((fs)->f->code[(e)->u.info])


#define lauK_setmultret(fs,e)	lauK_setreturns(fs, e, LAU_MULTRET)

#define lauK_jumpto(fs,t)	lauK_patchlist(fs, lauK_jump(fs), t)

LAUI_FUNC int lauK_code (FuncState *fs, Instruction i);
LAUI_FUNC int lauK_codeABx (FuncState *fs, OpCode o, int A, int Bx);
LAUI_FUNC int lauK_codeABCk (FuncState *fs, OpCode o, int A, int B, int C,
                                            int k);
LAUI_FUNC int lauK_codevABCk (FuncState *fs, OpCode o, int A, int B, int C,
                                             int k);
LAUI_FUNC int lauK_exp2const (FuncState *fs, const expdesc *e, TValue *v);
LAUI_FUNC void lauK_fixline (FuncState *fs, int line);
LAUI_FUNC void lauK_nil (FuncState *fs, int from, int n);
LAUI_FUNC void lauK_reserveregs (FuncState *fs, int n);
LAUI_FUNC void lauK_checkstack (FuncState *fs, int n);
LAUI_FUNC void lauK_int (FuncState *fs, int reg, lau_Integer n);
LAUI_FUNC void lauK_vapar2local (FuncState *fs, expdesc *var);
LAUI_FUNC void lauK_dischargevars (FuncState *fs, expdesc *e);
LAUI_FUNC int lauK_exp2anyreg (FuncState *fs, expdesc *e);
LAUI_FUNC void lauK_exp2anyregup (FuncState *fs, expdesc *e);
LAUI_FUNC void lauK_exp2nextreg (FuncState *fs, expdesc *e);
LAUI_FUNC void lauK_exp2val (FuncState *fs, expdesc *e);
LAUI_FUNC void lauK_self (FuncState *fs, expdesc *e, expdesc *key);
LAUI_FUNC void lauK_indexed (FuncState *fs, expdesc *t, expdesc *k);
LAUI_FUNC void lauK_goiftrue (FuncState *fs, expdesc *e);
LAUI_FUNC void lauK_storevar (FuncState *fs, expdesc *var, expdesc *e);
LAUI_FUNC void lauK_setreturns (FuncState *fs, expdesc *e, int nresults);
LAUI_FUNC void lauK_setoneret (FuncState *fs, expdesc *e);
LAUI_FUNC int lauK_jump (FuncState *fs);
LAUI_FUNC void lauK_ret (FuncState *fs, int first, int nret);
LAUI_FUNC void lauK_patchlist (FuncState *fs, int list, int target);
LAUI_FUNC void lauK_patchtohere (FuncState *fs, int list);
LAUI_FUNC void lauK_concat (FuncState *fs, int *l1, int l2);
LAUI_FUNC int lauK_getlabel (FuncState *fs);
LAUI_FUNC void lauK_prefix (FuncState *fs, UnOpr op, expdesc *v, int line);
LAUI_FUNC void lauK_infix (FuncState *fs, BinOpr op, expdesc *v);
LAUI_FUNC void lauK_posfix (FuncState *fs, BinOpr op, expdesc *v1,
                            expdesc *v2, int line);
LAUI_FUNC void lauK_settablesize (FuncState *fs, int pc,
                                  int ra, int asize, int hsize);
LAUI_FUNC void lauK_setlist (FuncState *fs, int base, int nelems, int tostore);
LAUI_FUNC void lauK_finish (FuncState *fs);
LAUI_FUNC l_noret lauK_semerror (LexState *ls, const char *fmt, ...);


#endif
