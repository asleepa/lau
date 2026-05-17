/*
** $Id: lcode.c $
** Code generator for Lau
** See Copyright Notice in lau.h
*/

#define lcode_c
#define LAU_CORE

#include "lprefix.h"


#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

#include "lau.h"

#include "lcode.h"
#include "ldebug.h"
#include "ldo.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"
#include "lstring.h"
#include "ltable.h"
#include "lvm.h"

/* (note that expressions VJMP also have jumps.) */
#define hasjumps(e)	((e)->t != (e)->f)


static int codesJ (FuncState *fs, OpCode o, int sj, int k);



/* semantic error */
l_noret lauK_semerror (LexState *ls, const char *fmt, ...) {
  const char *msg;
  va_list argp;
  pushvfstring(ls->L, argp, fmt, msg);
  ls->t.token = 0;  /* remove "near <token>" from final message */
  ls->linenumber = ls->lastline;  /* back to line of last used token */
  lauX_syntaxerror(ls, msg);
}


/*
** If expression is a numeric constant, fills 'v' with its value
** and returns 1. Otherwise, returns 0.
*/
static int tonumeral (const expdesc *e, TValue *v) {
  if (hasjumps(e))
    return 0;  /* not a numeral */
  switch (e->k) {
    case VKINT:
      if (v) setivalue(v, e->u.ival);
      return 1;
    case VKFLT:
      if (v) setfltvalue(v, e->u.nval);
      return 1;
    default: return 0;
  }
}


/*
** Get the constant value from a constant expression
*/
static TValue *const2val (FuncState *fs, const expdesc *e) {
  lau_assert(e->k == VCONST);
  return &fs->ls->dyd->actvar.arr[e->u.info].k;
}


/*
** If expression is a constant, fills 'v' with its value
** and returns 1. Otherwise, returns 0.
*/
int lauK_exp2const (FuncState *fs, const expdesc *e, TValue *v) {
  if (hasjumps(e))
    return 0;  /* not a constant */
  switch (e->k) {
    case VFALSE:
      setbfvalue(v);
      return 1;
    case VTRUE:
      setbtvalue(v);
      return 1;
    case VNIL:
      setnilvalue(v);
      return 1;
    case VKSTR: {
      setsvalue(fs->ls->L, v, e->u.strval);
      return 1;
    }
    case VCONST: {
      setobj(fs->ls->L, v, const2val(fs, e));
      return 1;
    }
    default: return tonumeral(e, v);
  }
}


/*
** Return the previous instruction of the current code. If there
** may be a jump target between the current instruction and the
** previous one, return an invalid instruction (to avoid wrong
** optimizations).
*/
static Instruction *previousinstruction (FuncState *fs) {
  static const Instruction invalidinstruction = ~(Instruction)0;
  if (fs->pc > fs->lasttarget)
    return &fs->f->code[fs->pc - 1];  /* previous instruction */
  else
    return cast(Instruction*, &invalidinstruction);
}


/*
** Create a OP_LOADNIL instruction, but try to optimize: if the previous
** instruction is also OP_LOADNIL and ranges are compatible, adjust
** range of previous instruction instead of emitting a new one. (For
** instance, 'local a; local b' will generate a single opcode.)
*/
void lauK_nil (FuncState *fs, int from, int n) {
  int l = from + n - 1;  /* last register to set nil */
  Instruction *previous = previousinstruction(fs);
  if (GET_OPCODE(*previous) == OP_LOADNIL) {  /* previous is LOADNIL? */
    int pfrom = GETARG_A(*previous);  /* get previous range */
    int pl = pfrom + GETARG_B(*previous);
    if ((pfrom <= from && from <= pl + 1) ||
        (from <= pfrom && pfrom <= l + 1)) {  /* can connect both? */
      if (pfrom < from) from = pfrom;  /* from = min(from, pfrom) */
      if (pl > l) l = pl;  /* l = max(l, pl) */
      SETARG_A(*previous, from);
      SETARG_B(*previous, l - from);
      return;
    }  /* else go through */
  }
  lauK_codeABC(fs, OP_LOADNIL, from, n - 1, 0);  /* else no optimization */
}


/*
** Gets the destination address of a jump instruction. Used to traverse
** a list of jumps.
*/
static int getjump (FuncState *fs, int pc) {
  int offset = GETARG_sJ(fs->f->code[pc]);
  if (offset == NO_JUMP)  /* point to itself represents end of list */
    return NO_JUMP;  /* end of list */
  else
    return (pc+1)+offset;  /* turn offset into absolute position */
}


/*
** Fix jump instruction at position 'pc' to jump to 'dest'.
** (Jump addresses are relative in Lau)
*/
static void fixjump (FuncState *fs, int pc, int dest) {
  Instruction *jmp = &fs->f->code[pc];
  int offset = dest - (pc + 1);
  lau_assert(dest != NO_JUMP);
  if (!(-OFFSET_sJ <= offset && offset <= MAXARG_sJ - OFFSET_sJ))
    lauX_syntaxerror(fs->ls, "control structure too long");
  lau_assert(GET_OPCODE(*jmp) == OP_JMP);
  SETARG_sJ(*jmp, offset);
}


/*
** Concatenate jump-list 'l2' into jump-list 'l1'
*/
void lauK_concat (FuncState *fs, int *l1, int l2) {
  if (l2 == NO_JUMP) return;  /* nothing to concatenate? */
  else if (*l1 == NO_JUMP)  /* no original list? */
    *l1 = l2;  /* 'l1' points to 'l2' */
  else {
    int list = *l1;
    int next;
    while ((next = getjump(fs, list)) != NO_JUMP)  /* find last element */
      list = next;
    fixjump(fs, list, l2);  /* last element links to 'l2' */
  }
}


/*
** Create a jump instruction and return its position, so its destination
** can be fixed later (with 'fixjump').
*/
int lauK_jump (FuncState *fs) {
  return codesJ(fs, OP_JMP, NO_JUMP, 0);
}


/*
** Code a 'return' instruction
*/
void lauK_ret (FuncState *fs, int first, int nret) {
  OpCode op;
  switch (nret) {
    case 0: op = OP_RETURN0; break;
    case 1: op = OP_RETURN1; break;
    default: op = OP_RETURN; break;
  }
  lauY_checklimit(fs, nret + 1, MAXARG_B, "returns");
  lauK_codeABC(fs, op, first, nret + 1, 0);
}


/*
** Code a "conditional jump", that is, a test or comparison opcode
** followed by a jump. Return jump position.
*/
static int condjump (FuncState *fs, OpCode op, int A, int B, int C, int k) {
  lauK_codeABCk(fs, op, A, B, C, k);
  return lauK_jump(fs);
}


/*
** returns current 'pc' and marks it as a jump target (to avoid wrong
** optimizations with consecutive instructions not in the same basic block).
*/
int lauK_getlabel (FuncState *fs) {
  fs->lasttarget = fs->pc;
  return fs->pc;
}


/*
** Returns the position of the instruction "controlling" a given
** jump (that is, its condition), or the jump itself if it is
** unconditional.
*/
static Instruction *getjumpcontrol (FuncState *fs, int pc) {
  Instruction *pi = &fs->f->code[pc];
  if (pc >= 1 && testTMode(GET_OPCODE(*(pi-1))))
    return pi-1;
  else
    return pi;
}


/*
** Patch destination register for a TESTSET instruction.
** If instruction in position 'node' is not a TESTSET, return 0 ("fails").
** Otherwise, if 'reg' is not 'NO_REG', set it as the destination
** register. Otherwise, change instruction to a simple 'TEST' (produces
** no register value)
*/
static int patchtestreg (FuncState *fs, int node, int reg) {
  Instruction *i = getjumpcontrol(fs, node);
  if (GET_OPCODE(*i) != OP_TESTSET)
    return 0;  /* cannot patch other instructions */
  if (reg != NO_REG && reg != GETARG_B(*i))
    SETARG_A(*i, reg);
  else {
     /* no register to put value or register already has the value;
        change instruction to simple test */
    *i = CREATE_ABCk(OP_TEST, GETARG_B(*i), 0, 0, GETARG_k(*i));
  }
  return 1;
}


/*
** Traverse a list of tests ensuring no one produces a value
*/
static void removevalues (FuncState *fs, int list) {
  for (; list != NO_JUMP; list = getjump(fs, list))
      patchtestreg(fs, list, NO_REG);
}


/*
** Traverse a list of tests, patching their destination address and
** registers: tests producing values jump to 'vtarget' (and put their
** values in 'reg'), other tests jump to 'dtarget'.
*/
static void patchlistaux (FuncState *fs, int list, int vtarget, int reg,
                          int dtarget) {
  while (list != NO_JUMP) {
    int next = getjump(fs, list);
    if (patchtestreg(fs, list, reg))
      fixjump(fs, list, vtarget);
    else
      fixjump(fs, list, dtarget);  /* jump to default target */
    list = next;
  }
}


/*
** Path all jumps in 'list' to jump to 'target'.
** (The assert means that we cannot fix a jump to a forward address
** because we only know addresses once code is generated.)
*/
void lauK_patchlist (FuncState *fs, int list, int target) {
  lau_assert(target <= fs->pc);
  patchlistaux(fs, list, target, NO_REG, target);
}


void lauK_patchtohere (FuncState *fs, int list) {
  int hr = lauK_getlabel(fs);  /* mark "here" as a jump target */
  lauK_patchlist(fs, list, hr);
}


/* limit for difference between lines in relative line info. */
#define LIMLINEDIFF	0x80


/*
** Save line info for a new instruction. If difference from last line
** does not fit in a byte, of after that many instructions, save a new
** absolute line info; (in that case, the special value 'ABSLINEINFO'
** in 'lineinfo' signals the existence of this absolute information.)
** Otherwise, store the difference from last line in 'lineinfo'.
*/
static void savelineinfo (FuncState *fs, Proto *f, int line) {
  int linedif = line - fs->previousline;
  int pc = fs->pc - 1;  /* last instruction coded */
  if (abs(linedif) >= LIMLINEDIFF || fs->iwthabs++ >= MAXIWTHABS) {
    lauM_growvector(fs->ls->L, f->abslineinfo, fs->nabslineinfo,
                    f->sizeabslineinfo, AbsLineInfo, INT_MAX, "lines");
    f->abslineinfo[fs->nabslineinfo].pc = pc;
    f->abslineinfo[fs->nabslineinfo++].line = line;
    linedif = ABSLINEINFO;  /* signal that there is absolute information */
    fs->iwthabs = 1;  /* restart counter */
  }
  lauM_growvector(fs->ls->L, f->lineinfo, pc, f->sizelineinfo, ls_byte,
                  INT_MAX, "opcodes");
  f->lineinfo[pc] = cast(ls_byte, linedif);
  fs->previousline = line;  /* last line saved */
}


/*
** Remove line information from the last instruction.
** If line information for that instruction is absolute, set 'iwthabs'
** above its max to force the new (replacing) instruction to have
** absolute line info, too.
*/
static void removelastlineinfo (FuncState *fs) {
  Proto *f = fs->f;
  int pc = fs->pc - 1;  /* last instruction coded */
  if (f->lineinfo[pc] != ABSLINEINFO) {  /* relative line info? */
    fs->previousline -= f->lineinfo[pc];  /* correct last line saved */
    fs->iwthabs--;  /* undo previous increment */
  }
  else {  /* absolute line information */
    lau_assert(f->abslineinfo[fs->nabslineinfo - 1].pc == pc);
    fs->nabslineinfo--;  /* remove it */
    fs->iwthabs = MAXIWTHABS + 1;  /* force next line info to be absolute */
  }
}


/*
** Remove the last instruction created, correcting line information
** accordingly.
*/
static void removelastinstruction (FuncState *fs) {
  removelastlineinfo(fs);
  fs->pc--;
}


/*
** Emit instruction 'i', checking for array sizes and saving also its
** line information. Return 'i' position.
*/
int lauK_code (FuncState *fs, Instruction i) {
  Proto *f = fs->f;
  /* put new instruction in code array */
  lauM_growvector(fs->ls->L, f->code, fs->pc, f->sizecode, Instruction,
                  INT_MAX, "opcodes");
  f->code[fs->pc++] = i;
  savelineinfo(fs, f, fs->ls->lastline);
  return fs->pc - 1;  /* index of new instruction */
}


/*
** Format and emit an 'iABC' instruction. (Assertions check consistency
** of parameters versus opcode.)
*/
int lauK_codeABCk (FuncState *fs, OpCode o, int A, int B, int C, int k) {
  lau_assert(getOpMode(o) == iABC);
  lau_assert(A <= MAXARG_A && B <= MAXARG_B &&
             C <= MAXARG_C && (k & ~1) == 0);
  return lauK_code(fs, CREATE_ABCk(o, A, B, C, k));
}


int lauK_codevABCk (FuncState *fs, OpCode o, int A, int B, int C, int k) {
  lau_assert(getOpMode(o) == ivABC);
  lau_assert(A <= MAXARG_A && B <= MAXARG_vB &&
             C <= MAXARG_vC && (k & ~1) == 0);
  return lauK_code(fs, CREATE_vABCk(o, A, B, C, k));
}


/*
** Format and emit an 'iABx' instruction.
*/
int lauK_codeABx (FuncState *fs, OpCode o, int A, int Bc) {
  lau_assert(getOpMode(o) == iABx);
  lau_assert(A <= MAXARG_A && Bc <= MAXARG_Bx);
  return lauK_code(fs, CREATE_ABx(o, A, Bc));
}


/*
** Format and emit an 'iAsBx' instruction.
*/
static int codeAsBx (FuncState *fs, OpCode o, int A, int Bc) {
  int b = Bc + OFFSET_sBx;
  lau_assert(getOpMode(o) == iAsBx);
  lau_assert(A <= MAXARG_A && b <= MAXARG_Bx);
  return lauK_code(fs, CREATE_ABx(o, A, b));
}


/*
** Format and emit an 'isJ' instruction.
*/
static int codesJ (FuncState *fs, OpCode o, int sj, int k) {
  int j = sj + OFFSET_sJ;
  lau_assert(getOpMode(o) == isJ);
  lau_assert(j <= MAXARG_sJ && (k & ~1) == 0);
  return lauK_code(fs, CREATE_sJ(o, j, k));
}


/*
** Emit an "extra argument" instruction (format 'iAx')
*/
static int codeextraarg (FuncState *fs, int A) {
  lau_assert(A <= MAXARG_Ax);
  return lauK_code(fs, CREATE_Ax(OP_EXTRAARG, A));
}


/*
** Emit a "load constant" instruction, using either 'OP_LOADK'
** (if constant index 'k' fits in 18 bits) or an 'OP_LOADKX'
** instruction with "extra argument".
*/
static int lauK_codek (FuncState *fs, int reg, int k) {
  if (k <= MAXARG_Bx)
    return lauK_codeABx(fs, OP_LOADK, reg, k);
  else {
    int p = lauK_codeABx(fs, OP_LOADKX, reg, 0);
    codeextraarg(fs, k);
    return p;
  }
}


/*
** Check register-stack level, keeping track of its maximum size
** in field 'maxstacksize'
*/
void lauK_checkstack (FuncState *fs, int n) {
  int newstack = fs->freereg + n;
  if (newstack > fs->f->maxstacksize) {
    lauY_checklimit(fs, newstack, MAX_FSTACK, "registers");
    fs->f->maxstacksize = cast_byte(newstack);
  }
}


/*
** Reserve 'n' registers in register stack
*/
void lauK_reserveregs (FuncState *fs, int n) {
  lauK_checkstack(fs, n);
  fs->freereg =  cast_byte(fs->freereg + n);
}


/*
** Free register 'reg', if it is neither a constant index nor
** a local variable.
)
*/
static void freereg (FuncState *fs, int reg) {
  if (reg >= lauY_nvarstack(fs)) {
    fs->freereg--;
    lau_assert(reg == fs->freereg);
  }
}


/*
** Free two registers in proper order
*/
static void freeregs (FuncState *fs, int r1, int r2) {
  if (r1 > r2) {
    freereg(fs, r1);
    freereg(fs, r2);
  }
  else {
    freereg(fs, r2);
    freereg(fs, r1);
  }
}


/*
** Free register used by expression 'e' (if any)
*/
static void freeexp (FuncState *fs, expdesc *e) {
  if (e->k == VNONRELOC)
    freereg(fs, e->u.info);
}


/*
** Free registers used by expressions 'e1' and 'e2' (if any) in proper
** order.
*/
static void freeexps (FuncState *fs, expdesc *e1, expdesc *e2) {
  int r1 = (e1->k == VNONRELOC) ? e1->u.info : -1;
  int r2 = (e2->k == VNONRELOC) ? e2->u.info : -1;
  freeregs(fs, r1, r2);
}


/*
** Add constant 'v' to prototype's list of constants (field 'k').
*/
static int addk (FuncState *fs, Proto *f, TValue *v) {
  lau_State *L = fs->ls->L;
  int oldsize = f->sizek;
  int k = fs->nk;
  lauM_growvector(L, f->k, k, f->sizek, TValue, MAXARG_Ax, "constants");
  while (oldsize < f->sizek)
    setnilvalue(&f->k[oldsize++]);
  setobj(L, &f->k[k], v);
  fs->nk++;
  lauC_barrier(L, f, v);
  return k;
}


/*
** Use scanner's table to cache position of constants in constant list
** and try to reuse constants. Because some values should not be used
** as keys (nil cannot be a key, integer keys can collapse with float
** keys), the caller must provide a useful 'key' for indexing the cache.
*/
static int k2proto (FuncState *fs, TValue *key, TValue *v) {
  TValue val;
  Proto *f = fs->f;
  int tag = lauH_get(fs->kcache, key, &val);  /* query scanner table */
  if (!tagisempty(tag)) {  /* is there an index there? */
    int k = cast_int(ivalue(&val));
    /* collisions can happen only for float keys */
    lau_assert(ttisfloat(key) || lauV_rawequalobj(&f->k[k], v));
    return k;  /* reuse index */
  }
  else {  /* constant not found; create a new entry */
    int k = addk(fs, f, v);
    /* cache it for reuse; numerical value does not need GC barrier;
       table is not a metatable, so it does not need to invalidate cache */
    setivalue(&val, k);
    lauH_set(fs->ls->L, fs->kcache, key, &val);
    return k;
  }
}


/*
** Add a string to list of constants and return its index.
*/
static int stringK (FuncState *fs, TString *s) {
  TValue o;
  setsvalue(fs->ls->L, &o, s);
  return k2proto(fs, &o, &o);  /* use string itself as key */
}


/*
** Add an integer to list of constants and return its index.
*/
static int lauK_intK (FuncState *fs, lau_Integer n) {
  TValue o;
  setivalue(&o, n);
  return k2proto(fs, &o, &o);  /* use integer itself as key */
}

/*
** Add a float to list of constants and return its index. Floats
** with integral values need a different key, to avoid collision
** with actual integers. To that end, we add to the number its smaller
** power-of-two fraction that is still significant in its scale.
** (For doubles, the fraction would be 2^-52).
** This method is not bulletproof: different numbers may generate the
** same key (e.g., very large numbers will overflow to 'inf') and for
** floats larger than 2^53 the result is still an integer. For those
** cases, just generate a new entry. At worst, this only wastes an entry
** with a duplicate.
*/
static int lauK_numberK (FuncState *fs, lau_Number r) {
  TValue o, kv;
  setfltvalue(&o, r);  /* value as a TValue */
  if (r == 0) {  /* handle zero as a special case */
    setpvalue(&kv, fs);  /* use FuncState as index */
    return k2proto(fs, &kv, &o);  /* cannot collide */
  }
  else {
    const int nbm = l_floatatt(MANT_DIG);
    const lau_Number q = l_mathop(ldexp)(l_mathop(1.0), -nbm + 1);
    const lau_Number k =  r * (1 + q);  /* key */
    lau_Integer ik;
    setfltvalue(&kv, k);  /* key as a TValue */
    if (!lauV_flttointeger(k, &ik, F2Ieq)) {  /* not an integer value? */
      int n = k2proto(fs, &kv, &o);  /* use key */
      if (lauV_rawequalobj(&fs->f->k[n], &o))  /* correct value? */
        return n;
    }
    /* else, either key is still an integer or there was a collision;
       anyway, do not try to reuse constant; instead, create a new one */
    return addk(fs, fs->f, &o);
  }
}


/*
** Add a false to list of constants and return its index.
*/
static int boolF (FuncState *fs) {
  TValue o;
  setbfvalue(&o);
  return k2proto(fs, &o, &o);  /* use boolean itself as key */
}


/*
** Add a true to list of constants and return its index.
*/
static int boolT (FuncState *fs) {
  TValue o;
  setbtvalue(&o);
  return k2proto(fs, &o, &o);  /* use boolean itself as key */
}


/*
** Add nil to list of constants and return its index.
*/
static int nilK (FuncState *fs) {
  lau_State *L = fs->ls->L;
  TValue k;
  /* cannot use nil as key; instead use table itself */
  sethvalue(L, &k, fs->kcache);
  return k2proto(fs, &k, &G(L)->nilvalue);
}


/*
** Check whether 'i' can be stored in an 'sC' operand. Equivalent to
** (0 <= int2sC(i) && int2sC(i) <= MAXARG_C) but without risk of
** overflows in the hidden addition inside 'int2sC'.
*/
static int fitsC (lau_Integer i) {
  return (l_castS2U(i) + OFFSET_sC <= cast_uint(MAXARG_C));
}


/*
** Check whether 'i' can be stored in an 'sBx' operand.
*/
static int fitsBx (lau_Integer i) {
  return (-OFFSET_sBx <= i && i <= MAXARG_Bx - OFFSET_sBx);
}


void lauK_int (FuncState *fs, int reg, lau_Integer i) {
  if (fitsBx(i))
    codeAsBx(fs, OP_LOADI, reg, cast_int(i));
  else
    lauK_codek(fs, reg, lauK_intK(fs, i));
}


static void lauK_float (FuncState *fs, int reg, lau_Number f) {
  lau_Integer fi;
  if (lauV_flttointeger(f, &fi, F2Ieq) && fitsBx(fi))
    codeAsBx(fs, OP_LOADF, reg, cast_int(fi));
  else
    lauK_codek(fs, reg, lauK_numberK(fs, f));
}


/*
** Convert a constant in 'v' into an expression description 'e'
*/
static void const2exp (TValue *v, expdesc *e) {
  switch (ttypetag(v)) {
    case LAU_VNUMINT:
      e->k = VKINT; e->u.ival = ivalue(v);
      break;
    case LAU_VNUMFLT:
      e->k = VKFLT; e->u.nval = fltvalue(v);
      break;
    case LAU_VFALSE:
      e->k = VFALSE;
      break;
    case LAU_VTRUE:
      e->k = VTRUE;
      break;
    case LAU_VNIL:
      e->k = VNIL;
      break;
    case LAU_VSHRSTR:  case LAU_VLNGSTR:
      e->k = VKSTR; e->u.strval = tsvalue(v);
      break;
    default: lau_assert(0);
  }
}


/*
** Fix an expression to return the number of results 'nresults'.
** 'e' must be a multi-ret expression (function call or vararg).
*/
void lauK_setreturns (FuncState *fs, expdesc *e, int nresults) {
  Instruction *pc = &getinstruction(fs, e);
  lauY_checklimit(fs, nresults + 1, MAXARG_C, "multiple results");
  if (e->k == VCALL)  /* expression is an open function call? */
    SETARG_C(*pc, nresults + 1);
  else {
    lau_assert(e->k == VVARARG);
    SETARG_C(*pc, nresults + 1);
    SETARG_A(*pc, fs->freereg);
    lauK_reserveregs(fs, 1);
  }
}


/*
** Convert a VKSTR to a VK
*/
static int str2K (FuncState *fs, expdesc *e) {
  lau_assert(e->k == VKSTR);
  e->u.info = stringK(fs, e->u.strval);
  e->k = VK;
  return e->u.info;
}


/*
** Fix an expression to return one result.
** If expression is not a multi-ret expression (function call or
** vararg), it already returns one result, so nothing needs to be done.
** Function calls become VNONRELOC expressions (as its result comes
** fixed in the base register of the call), while vararg expressions
** become VRELOC (as OP_VARARG puts its results where it wants).
** (Calls are created returning one result, so that does not need
** to be fixed.)
*/
void lauK_setoneret (FuncState *fs, expdesc *e) {
  if (e->k == VCALL) {  /* expression is an open function call? */
    /* already returns 1 value */
    lau_assert(GETARG_C(getinstruction(fs, e)) == 2);
    e->k = VNONRELOC;  /* result has fixed position */
    e->u.info = GETARG_A(getinstruction(fs, e));
  }
  else if (e->k == VVARARG) {
    SETARG_C(getinstruction(fs, e), 2);
    e->k = VRELOC;  /* can relocate its simple result */
  }
}

/*
** Change a vararg parameter into a regular local variable
*/
void lauK_vapar2local (FuncState *fs, expdesc *var) {
  needvatab(fs->f);  /* function will need a vararg table */
  /* now a vararg parameter is equivalent to a regular local variable */
  var->k = VLOCAL;
}


/*
** Ensure that expression 'e' is not a variable (nor a <const>).
** (Expression still may have jump lists.)
*/
void lauK_dischargevars (FuncState *fs, expdesc *e) {
  switch (e->k) {
    case VCONST: {
      const2exp(const2val(fs, e), e);
      break;
    }
    case VVARGVAR: {
      lauK_vapar2local(fs, e);  /* turn it into a local variable */
    }  /* FALLTHROUGH */
    case VLOCAL: {  /* already in a register */
      int temp = e->u.var.ridx;
      e->u.info = temp;  /* (avoid a direct assignment; values overlap) */
      e->k = VNONRELOC;  /* becomes a non-relocatable value */
      break;
    }
    case VUPVAL: {  /* move value to some (pending) register */
      e->u.info = lauK_codeABC(fs, OP_GETUPVAL, 0, e->u.info, 0);
      e->k = VRELOC;
      break;
    }
    case VINDEXUP: {
      e->u.info = lauK_codeABC(fs, OP_GETTABUP, 0, e->u.ind.t, e->u.ind.idx);
      e->k = VRELOC;
      break;
    }
    case VINDEXI: {
      freereg(fs, e->u.ind.t);
      e->u.info = lauK_codeABC(fs, OP_GETI, 0, e->u.ind.t, e->u.ind.idx);
      e->k = VRELOC;
      break;
    }
    case VINDEXSTR: {
      freereg(fs, e->u.ind.t);
      e->u.info = lauK_codeABC(fs, OP_GETFIELD, 0, e->u.ind.t, e->u.ind.idx);
      e->k = VRELOC;
      break;
    }
    case VINDEXED: {
      freeregs(fs, e->u.ind.t, e->u.ind.idx);
      e->u.info = lauK_codeABC(fs, OP_GETTABLE, 0, e->u.ind.t, e->u.ind.idx);
      e->k = VRELOC;
      break;
    }
    case VVARGIND: {
      freeregs(fs, e->u.ind.t, e->u.ind.idx);
      e->u.info = lauK_codeABC(fs, OP_GETVARG, 0, e->u.ind.t, e->u.ind.idx);
      e->k = VRELOC;
      break;
    }
    case VVARARG: case VCALL: {
      lauK_setoneret(fs, e);
      break;
    }
    default: break;  /* there is one value available (somewhere) */
  }
}


/*
** Ensure expression value is in register 'reg', making 'e' a
** non-relocatable expression.
** (Expression still may have jump lists.)
*/
static void discharge2reg (FuncState *fs, expdesc *e, int reg) {
  lauK_dischargevars(fs, e);
  switch (e->k) {
    case VNIL: {
      lauK_nil(fs, reg, 1);
      break;
    }
    case VFALSE: {
      lauK_codeABC(fs, OP_LOADFALSE, reg, 0, 0);
      break;
    }
    case VTRUE: {
      lauK_codeABC(fs, OP_LOADTRUE, reg, 0, 0);
      break;
    }
    case VKSTR: {
      str2K(fs, e);
    }  /* FALLTHROUGH */
    case VK: {
      lauK_codek(fs, reg, e->u.info);
      break;
    }
    case VKFLT: {
      lauK_float(fs, reg, e->u.nval);
      break;
    }
    case VKINT: {
      lauK_int(fs, reg, e->u.ival);
      break;
    }
    case VRELOC: {
      Instruction *pc = &getinstruction(fs, e);
      SETARG_A(*pc, reg);  /* instruction will put result in 'reg' */
      break;
    }
    case VNONRELOC: {
      if (reg != e->u.info)
        lauK_codeABC(fs, OP_MOVE, reg, e->u.info, 0);
      break;
    }
    default: {
      lau_assert(e->k == VJMP);
      return;  /* nothing to do... */
    }
  }
  e->u.info = reg;
  e->k = VNONRELOC;
}


/*
** Ensure expression value is in a register, making 'e' a
** non-relocatable expression.
** (Expression still may have jump lists.)
*/
static void discharge2anyreg (FuncState *fs, expdesc *e) {
  if (e->k != VNONRELOC) {  /* no fixed register yet? */
    lauK_reserveregs(fs, 1);  /* get a register */
    discharge2reg(fs, e, fs->freereg-1);  /* put value there */
  }
}


static int code_loadbool (FuncState *fs, int A, OpCode op) {
  lauK_getlabel(fs);  /* those instructions may be jump targets */
  return lauK_codeABC(fs, op, A, 0, 0);
}


/*
** check whether list has any jump that do not produce a value
** or produce an inverted value
*/
static int need_value (FuncState *fs, int list) {
  for (; list != NO_JUMP; list = getjump(fs, list)) {
    Instruction i = *getjumpcontrol(fs, list);
    if (GET_OPCODE(i) != OP_TESTSET) return 1;
  }
  return 0;  /* not found */
}


/*
** Ensures final expression result (which includes results from its
** jump lists) is in register 'reg'.
** If expression has jumps, need to patch these jumps either to
** its final position or to "load" instructions (for those tests
** that do not produce values).
*/
static void exp2reg (FuncState *fs, expdesc *e, int reg) {
  discharge2reg(fs, e, reg);
  if (e->k == VJMP)  /* expression itself is a test? */
    lauK_concat(fs, &e->t, e->u.info);  /* put this jump in 't' list */
  if (hasjumps(e)) {
    int final;  /* position after whole expression */
    int p_f = NO_JUMP;  /* position of an eventual LOAD false */
    int p_t = NO_JUMP;  /* position of an eventual LOAD true */
    if (need_value(fs, e->t) || need_value(fs, e->f)) {
      int fj = (e->k == VJMP) ? NO_JUMP : lauK_jump(fs);
      p_f = code_loadbool(fs, reg, OP_LFALSESKIP);  /* skip next inst. */
      p_t = code_loadbool(fs, reg, OP_LOADTRUE);
      /* jump around these booleans if 'e' is not a test */
      lauK_patchtohere(fs, fj);
    }
    final = lauK_getlabel(fs);
    patchlistaux(fs, e->f, final, reg, p_f);
    patchlistaux(fs, e->t, final, reg, p_t);
  }
  e->f = e->t = NO_JUMP;
  e->u.info = reg;
  e->k = VNONRELOC;
}


/*
** Ensures final expression result is in next available register.
*/
void lauK_exp2nextreg (FuncState *fs, expdesc *e) {
  lauK_dischargevars(fs, e);
  freeexp(fs, e);
  lauK_reserveregs(fs, 1);
  exp2reg(fs, e, fs->freereg - 1);
}


/*
** Ensures final expression result is in some (any) register
** and return that register.
*/
int lauK_exp2anyreg (FuncState *fs, expdesc *e) {
  lauK_dischargevars(fs, e);
  if (e->k == VNONRELOC) {  /* expression already has a register? */
    if (!hasjumps(e))  /* no jumps? */
      return e->u.info;  /* result is already in a register */
    if (e->u.info >= lauY_nvarstack(fs)) {  /* reg. is not a local? */
      exp2reg(fs, e, e->u.info);  /* put final result in it */
      return e->u.info;
    }
    /* else expression has jumps and cannot change its register
       to hold the jump values, because it is a local variable.
       Go through to the default case. */
  }
  lauK_exp2nextreg(fs, e);  /* default: use next available register */
  return e->u.info;
}


/*
** Ensures final expression result is either in a register,
** in an upvalue, or it is the vararg parameter.
*/
void lauK_exp2anyregup (FuncState *fs, expdesc *e) {
  if ((e->k != VUPVAL && e->k != VVARGVAR) || hasjumps(e))
    lauK_exp2anyreg(fs, e);
}


/*
** Ensures final expression result is either in a register
** or it is a constant.
*/
void lauK_exp2val (FuncState *fs, expdesc *e) {
  if (e->k == VJMP || hasjumps(e))
    lauK_exp2anyreg(fs, e);
  else
    lauK_dischargevars(fs, e);
}


/*
** Try to make 'e' a K expression with an index in the range of R/K
** indices. Return true iff succeeded.
*/
static int lauK_exp2K (FuncState *fs, expdesc *e) {
  if (!hasjumps(e)) {
    int info;
    switch (e->k) {  /* move constants to 'k' */
      case VTRUE: info = boolT(fs); break;
      case VFALSE: info = boolF(fs); break;
      case VNIL: info = nilK(fs); break;
      case VKINT: info = lauK_intK(fs, e->u.ival); break;
      case VKFLT: info = lauK_numberK(fs, e->u.nval); break;
      case VKSTR: info = stringK(fs, e->u.strval); break;
      case VK: info = e->u.info; break;
      default: return 0;  /* not a constant */
    }
    if (info <= MAXINDEXRK) {  /* does constant fit in 'argC'? */
      e->k = VK;  /* make expression a 'K' expression */
      e->u.info = info;
      return 1;
    }
  }
  /* else, expression doesn't fit; leave it unchanged */
  return 0;
}


/*
** Ensures final expression result is in a valid R/K index
** (that is, it is either in a register or in 'k' with an index
** in the range of R/K indices).
** Returns 1 iff expression is K.
*/
static int exp2RK (FuncState *fs, expdesc *e) {
  if (lauK_exp2K(fs, e))
    return 1;
  else {  /* not a constant in the right range: put it in a register */
    lauK_exp2anyreg(fs, e);
    return 0;
  }
}


static void codeABRK (FuncState *fs, OpCode o, int A, int B,
                      expdesc *ec) {
  int k = exp2RK(fs, ec);
  lauK_codeABCk(fs, o, A, B, ec->u.info, k);
}


/*
** Generate code to store result of expression 'ex' into variable 'var'.
*/
void lauK_storevar (FuncState *fs, expdesc *var, expdesc *ex) {
  switch (var->k) {
    case VLOCAL: {
      freeexp(fs, ex);
      exp2reg(fs, ex, var->u.var.ridx);  /* compute 'ex' into proper place */
      return;
    }
    case VUPVAL: {
      int e = lauK_exp2anyreg(fs, ex);
      lauK_codeABC(fs, OP_SETUPVAL, e, var->u.info, 0);
      break;
    }
    case VINDEXUP: {
      codeABRK(fs, OP_SETTABUP, var->u.ind.t, var->u.ind.idx, ex);
      break;
    }
    case VINDEXI: {
      codeABRK(fs, OP_SETI, var->u.ind.t, var->u.ind.idx, ex);
      break;
    }
    case VINDEXSTR: {
      codeABRK(fs, OP_SETFIELD, var->u.ind.t, var->u.ind.idx, ex);
      break;
    }
    case VVARGIND: {
      needvatab(fs->f);  /* function will need a vararg table */
      /* now, assignment is to a regular table */
    }  /* FALLTHROUGH */
    case VINDEXED: {
      codeABRK(fs, OP_SETTABLE, var->u.ind.t, var->u.ind.idx, ex);
      break;
    }
    default: lau_assert(0);  /* invalid var kind to store */
  }
  freeexp(fs, ex);
}


/*
** Negate condition 'e' (where 'e' is a comparison).
*/
static void negatecondition (FuncState *fs, expdesc *e) {
  Instruction *pc = getjumpcontrol(fs, e->u.info);
  lau_assert(testTMode(GET_OPCODE(*pc)) && GET_OPCODE(*pc) != OP_TESTSET &&
                                           GET_OPCODE(*pc) != OP_TEST);
  SETARG_k(*pc, (GETARG_k(*pc) ^ 1));
}


/*
** Emit instruction to jump if 'e' is 'cond' (that is, if 'cond'
** is true, code will jump if 'e' is true.) Return jump position.
** Optimize when 'e' is 'not' something, inverting the condition
** and removing the 'not'.
*/
static int jumponcond (FuncState *fs, expdesc *e, int cond) {
  if (e->k == VRELOC) {
    Instruction ie = getinstruction(fs, e);
    if (GET_OPCODE(ie) == OP_NOT) {
      removelastinstruction(fs);  /* remove previous OP_NOT */
      return condjump(fs, OP_TEST, GETARG_B(ie), 0, 0, !cond);
    }
    /* else go through */
  }
  discharge2anyreg(fs, e);
  freeexp(fs, e);
  return condjump(fs, OP_TESTSET, NO_REG, e->u.info, 0, cond);
}


/*
** Emit code to go through if 'e' is true, jump otherwise.
*/
void lauK_goiftrue (FuncState *fs, expdesc *e) {
  int pc;  /* pc of new jump */
  lauK_dischargevars(fs, e);
  switch (e->k) {
    case VJMP: {  /* condition? */
      negatecondition(fs, e);  /* jump when it is false */
      pc = e->u.info;  /* save jump position */
      break;
    }
    case VK: case VKFLT: case VKINT: case VKSTR: case VTRUE: {
      pc = NO_JUMP;  /* always true; do nothing */
      break;
    }
    default: {
      pc = jumponcond(fs, e, 0);  /* jump when false */
      break;
    }
  }
  lauK_concat(fs, &e->f, pc);  /* insert new jump in false list */
  lauK_patchtohere(fs, e->t);  /* true list jumps to here (to go through) */
  e->t = NO_JUMP;
}


/*
** Emit code to go through if 'e' is false, jump otherwise.
*/
static void lauK_goiffalse (FuncState *fs, expdesc *e) {
  int pc;  /* pc of new jump */
  lauK_dischargevars(fs, e);
  switch (e->k) {
    case VJMP: {
      pc = e->u.info;  /* already jump if true */
      break;
    }
    case VNIL: case VFALSE: {
      pc = NO_JUMP;  /* always false; do nothing */
      break;
    }
    default: {
      pc = jumponcond(fs, e, 1);  /* jump if true */
      break;
    }
  }
  lauK_concat(fs, &e->t, pc);  /* insert new jump in 't' list */
  lauK_patchtohere(fs, e->f);  /* false list jumps to here (to go through) */
  e->f = NO_JUMP;
}


/*
** Code 'not e', doing constant folding.
*/
static void codenot (FuncState *fs, expdesc *e) {
  switch (e->k) {
    case VNIL: case VFALSE: {
      e->k = VTRUE;  /* true == not nil == not false */
      break;
    }
    case VK: case VKFLT: case VKINT: case VKSTR: case VTRUE: {
      e->k = VFALSE;  /* false == not "x" == not 0.5 == not 1 == not true */
      break;
    }
    case VJMP: {
      negatecondition(fs, e);
      break;
    }
    case VRELOC:
    case VNONRELOC: {
      discharge2anyreg(fs, e);
      freeexp(fs, e);
      e->u.info = lauK_codeABC(fs, OP_NOT, 0, e->u.info, 0);
      e->k = VRELOC;
      break;
    }
    default: lau_assert(0);  /* cannot happen */
  }
  /* interchange true and false lists */
  { int temp = e->f; e->f = e->t; e->t = temp; }
  removevalues(fs, e->f);  /* values are useless when negated */
  removevalues(fs, e->t);
}


/*
** Check whether expression 'e' is a short literal string
*/
static int isKstr (FuncState *fs, expdesc *e) {
  return (e->k == VK && !hasjumps(e) && e->u.info <= MAXINDEXRK &&
          ttisshrstring(&fs->f->k[e->u.info]));
}

/*
** Check whether expression 'e' is a literal integer.
*/
static int isKint (expdesc *e) {
  return (e->k == VKINT && !hasjumps(e));
}


/*
** Check whether expression 'e' is a literal integer in
** proper range to fit in register C
*/
static int isCint (expdesc *e) {
  return isKint(e) && (l_castS2U(e->u.ival) <= l_castS2U(MAXARG_C));
}


/*
** Check whether expression 'e' is a literal integer in
** proper range to fit in register sC
*/
static int isSCint (expdesc *e) {
  return isKint(e) && fitsC(e->u.ival);
}


/*
** Check whether expression 'e' is a literal integer or float in
** proper range to fit in a register (sB or sC).
*/
static int isSCnumber (expdesc *e, int *pi, int *isfloat) {
  lau_Integer i;
  if (e->k == VKINT)
    i = e->u.ival;
  else if (e->k == VKFLT && lauV_flttointeger(e->u.nval, &i, F2Ieq))
    *isfloat = 1;
  else
    return 0;  /* not a number */
  if (!hasjumps(e) && fitsC(i)) {
    *pi = int2sC(cast_int(i));
    return 1;
  }
  else
    return 0;
}


/*
** Emit SELF instruction or equivalent: the code will convert
** expression 'e' into 'e.key(e,'.
*/
void lauK_self (FuncState *fs, expdesc *e, expdesc *key) {
  int ereg, base;
  lauK_exp2anyreg(fs, e);
  ereg = e->u.info;  /* register where 'e' (the receiver) was placed */
  freeexp(fs, e);
  base = e->u.info = fs->freereg;  /* base register for op_self */
  e->k = VNONRELOC;  /* self expression has a fixed register */
  lauK_reserveregs(fs, 2);  /* method and 'self' produced by op_self */
  lau_assert(key->k == VKSTR);
  /* is method name a short string in a valid K index? */
  if (strisshr(key->u.strval) && lauK_exp2K(fs, key)) {
    /* can use 'self' opcode */
    lauK_codeABCk(fs, OP_SELF, base, ereg, key->u.info, 0);
  }
  else {  /* cannot use 'self' opcode; use move+gettable */
    lauK_exp2anyreg(fs, key);  /* put method name in a register */
    lauK_codeABC(fs, OP_MOVE, base + 1, ereg, 0);  /* copy self to base+1 */
    lauK_codeABC(fs, OP_GETTABLE, base, ereg, key->u.info);  /* get method */
  }
  freeexp(fs, key);
}


/* auxiliary function to define indexing expressions */
static void fillidxk (expdesc *t, int idx, expkind k) {
  t->u.ind.idx = cast_byte(idx);
  t->k = k;
}


/*
** Create expression 't[k]'. 't' must have its final result already in a
** register or upvalue. Upvalues can only be indexed by literal strings.
** Keys can be literal strings in the constant table or arbitrary
** values in registers.
*/
void lauK_indexed (FuncState *fs, expdesc *t, expdesc *k) {
  int keystr = -1;
  if (k->k == VKSTR)
    keystr = str2K(fs, k);
  lau_assert(!hasjumps(t) &&
             (t->k == VLOCAL || t->k == VVARGVAR ||
              t->k == VNONRELOC || t->k == VUPVAL));
  if (t->k == VUPVAL && !isKstr(fs, k))  /* upvalue indexed by non 'Kstr'? */
    lauK_exp2anyreg(fs, t);  /* put it in a register */
  if (t->k == VUPVAL) {
    lu_byte temp = cast_byte(t->u.info);  /* upvalue index */
    t->u.ind.t = temp;  /* (avoid a direct assignment; values overlap) */
    lau_assert(isKstr(fs, k));
    fillidxk(t, k->u.info, VINDEXUP);  /* literal short string */
  }
  else if (t->k == VVARGVAR) {  /* indexing the vararg parameter? */
    int kreg = lauK_exp2anyreg(fs, k);  /* put key in some register */
    lu_byte vreg = cast_byte(t->u.var.ridx);  /* register with vararg param. */
    lau_assert(vreg == fs->f->numparams);
    t->u.ind.t = vreg;  /* (avoid a direct assignment; values may overlap?) */
    fillidxk(t, kreg, VVARGIND);  /* 't' represents 'vararg[k]' */
  }
  else {
    /* register index of the table */
    lu_byte temp = cast_byte((t->k == VLOCAL) ? t->u.var.ridx: t->u.info);
    t->u.ind.t = temp;  /* (avoid a direct assignment; values may overlap?) */
    if (isKstr(fs, k))
      fillidxk(t, k->u.info, VINDEXSTR);  /* literal short string */
    else if (isCint(k))  /* int. constant in proper range? */
      fillidxk(t, cast_int(k->u.ival), VINDEXI);
    else
      fillidxk(t, lauK_exp2anyreg(fs, k), VINDEXED);  /* register */
  }
  t->u.ind.keystr = keystr;  /* string index in 'k' */
  t->u.ind.ro = 0;  /* by default, not read-only */
}


/*
** Return false if folding can raise an error.
** Bitwise operations need operands convertible to integers; division
** operations cannot have 0 as divisor.
*/
static int validop (int op, TValue *v2) {
  switch (op) {
    case LAU_OPDIV: case LAU_OPMOD:  /* division by 0 */
      return (nvalue(v2) != 0);
    default: return 1;  /* everything else is valid */
  }
}


/*
** Try to "constant-fold" an operation; return 1 iff successful.
** (In this case, 'e1' has the final result.)
*/
static int constfolding (FuncState *fs, int op, expdesc *e1,
                                        const expdesc *e2) {
  TValue v1, v2, res;
  if (!tonumeral(e1, &v1) || !tonumeral(e2, &v2) || !validop(op, &v2))
    return 0;  /* non-numeric operands or not safe to fold */
  lauO_rawarith(fs->ls->L, op, &v1, &v2, &res);  /* does operation */
  if (ttisinteger(&res)) {
    e1->k = VKINT;
    e1->u.ival = ivalue(&res);
  }
  else {  /* folds neither NaN nor 0.0 (to avoid problems with -0.0) */
    lau_Number n = fltvalue(&res);
    if (laui_numisnan(n) || n == 0)
      return 0;
    e1->k = VKFLT;
    e1->u.nval = n;
  }
  return 1;
}


/*
** Convert a BinOpr to an OpCode  (ORDER OPR - ORDER OP)
*/
l_sinline OpCode binopr2op (BinOpr opr, BinOpr baser, OpCode base) {
  lau_assert(baser <= opr &&
            ((baser == OPR_ADD && opr <= OPR_DIV) ||
             (baser == OPR_LT && opr <= OPR_LE)));
  return cast(OpCode, (cast_int(opr) - cast_int(baser)) + cast_int(base));
}


/*
** Convert a UnOpr to an OpCode  (ORDER OPR - ORDER OP)
*/
l_sinline OpCode unopr2op (UnOpr opr) {
  return cast(OpCode, (cast_int(opr) - cast_int(OPR_MINUS)) +
                                       cast_int(OP_UNM));
}


/*
** Convert a BinOpr to a tag method  (ORDER OPR - ORDER TM)
*/
l_sinline TMS binopr2TM (BinOpr opr) {
  lau_assert(OPR_ADD <= opr && opr <= OPR_DIV);
  return cast(TMS, (cast_int(opr) - cast_int(OPR_ADD)) + cast_int(TM_ADD));
}


/*
** Emit code for unary expressions that "produce values"
** (everything but 'not').
** Expression to produce final result will be encoded in 'e'.
*/
static void codeunexpval (FuncState *fs, OpCode op, expdesc *e, int line) {
  int r = lauK_exp2anyreg(fs, e);  /* opcodes operate only on registers */
  freeexp(fs, e);
  e->u.info = lauK_codeABC(fs, op, 0, r, 0);  /* generate opcode */
  e->k = VRELOC;  /* all those operations are relocatable */
  lauK_fixline(fs, line);
}


/*
** Emit code for binary expressions that "produce values"
** (everything but logical operators 'and'/'or' and comparison
** operators).
** Expression to produce final result will be encoded in 'e1'.
*/
static void finishbinexpval (FuncState *fs, expdesc *e1, expdesc *e2,
                             OpCode op, int v2, int flip, int line,
                             OpCode mmop, TMS event) {
  int v1 = lauK_exp2anyreg(fs, e1);
  int pc = lauK_codeABCk(fs, op, 0, v1, v2, 0);
  freeexps(fs, e1, e2);
  e1->u.info = pc;
  e1->k = VRELOC;  /* all those operations are relocatable */
  lauK_fixline(fs, line);
  lauK_codeABCk(fs, mmop, v1, v2, cast_int(event), flip);  /* metamethod */
  lauK_fixline(fs, line);
}


/*
** Emit code for binary expressions that "produce values" over
** two registers.
*/
static void codebinexpval (FuncState *fs, BinOpr opr,
                           expdesc *e1, expdesc *e2, int line) {
  OpCode op = binopr2op(opr, OPR_ADD, OP_ADD);
  int v2 = lauK_exp2anyreg(fs, e2);  /* make sure 'e2' is in a register */
  /* 'e1' must be already in a register or it is a constant */
  lau_assert((VNIL <= e1->k && e1->k <= VKSTR) ||
             e1->k == VNONRELOC || e1->k == VRELOC);
  lau_assert(OP_ADD <= op && op <= OP_DIV);
  finishbinexpval(fs, e1, e2, op, v2, 0, line, OP_MMBIN, binopr2TM(opr));
}


/*
** Code binary operators with immediate operands.
*/
static void codebini (FuncState *fs, OpCode op,
                       expdesc *e1, expdesc *e2, int flip, int line,
                       TMS event) {
  int v2 = int2sC(cast_int(e2->u.ival));  /* immediate operand */
  lau_assert(e2->k == VKINT);
  finishbinexpval(fs, e1, e2, op, v2, flip, line, OP_MMBINI, event);
}


/*
** Code binary operators with K operand.
*/
static void codebinK (FuncState *fs, BinOpr opr,
                      expdesc *e1, expdesc *e2, int flip, int line) {
  TMS event = binopr2TM(opr);
  int v2 = e2->u.info;  /* K index */
  OpCode op = binopr2op(opr, OPR_ADD, OP_ADDK);
  finishbinexpval(fs, e1, e2, op, v2, flip, line, OP_MMBINK, event);
}


/* Try to code a binary operator negating its second operand.
** For the metamethod, 2nd operand must keep its original value.
*/
static int finishbinexpneg (FuncState *fs, expdesc *e1, expdesc *e2,
                             OpCode op, int line, TMS event) {
  if (!isKint(e2))
    return 0;  /* not an integer constant */
  else {
    lau_Integer i2 = e2->u.ival;
    if (!(fitsC(i2) && fitsC(-i2)))
      return 0;  /* not in the proper range */
    else {  /* operating a small integer constant */
      int v2 = cast_int(i2);
      finishbinexpval(fs, e1, e2, op, int2sC(-v2), 0, line, OP_MMBINI, event);
      /* correct metamethod argument */
      SETARG_B(fs->f->code[fs->pc - 1], int2sC(v2));
      return 1;  /* successfully coded */
    }
  }
}


static void swapexps (expdesc *e1, expdesc *e2) {
  expdesc temp = *e1; *e1 = *e2; *e2 = temp;  /* swap 'e1' and 'e2' */
}


/*
** Code binary operators with no constant operand.
*/
static void codebinNoK (FuncState *fs, BinOpr opr,
                        expdesc *e1, expdesc *e2, int flip, int line) {
  if (flip)
    swapexps(e1, e2);  /* back to original order */
  codebinexpval(fs, opr, e1, e2, line);  /* use standard operators */
}


/*
** Code arithmetic operators ('+', '-', ...). If second operand is a
** constant in the proper range, use variant opcodes with K operands.
*/
static void codearith (FuncState *fs, BinOpr opr,
                       expdesc *e1, expdesc *e2, int flip, int line) {
  if (tonumeral(e2, NULL) && lauK_exp2K(fs, e2))  /* K operand? */
    codebinK(fs, opr, e1, e2, flip, line);
  else  /* 'e2' is neither an immediate nor a K operand */
    codebinNoK(fs, opr, e1, e2, flip, line);
}


/*
** Code commutative operators ('+', '*'). If first operand is a
** numeric constant, change order of operands to try to use an
** immediate or K operator.
*/
static void codecommutative (FuncState *fs, BinOpr op,
                             expdesc *e1, expdesc *e2, int line) {
  if (op == OPR_ADD && isSCint(e2))  /* immediate operand? */
    codebini(fs, OP_ADDI, e1, e2, 0, line, TM_ADD);
  else
    codearith(fs, op, e1, e2, 0, line);
}


/*
** Emit code for order comparisons. When using an immediate operand,
** 'isfloat' tells whether the original value was a float.
*/
static void codeorder (FuncState *fs, BinOpr opr, expdesc *e1, expdesc *e2) {
  int r1, r2;
  int im;
  int isfloat = 0;
  OpCode op;
  if (isSCnumber(e2, &im, &isfloat)) {
    /* use immediate operand */
    r1 = lauK_exp2anyreg(fs, e1);
    r2 = im;
    op = binopr2op(opr, OPR_LT, OP_LTI);
  }
  else if (isSCnumber(e1, &im, &isfloat)) {
    /* transform (A < B) to (B > A) and (A <= B) to (B >= A) */
    r1 = lauK_exp2anyreg(fs, e2);
    r2 = im;
    op = binopr2op(opr, OPR_LT, OP_GTI);
  }
  else {  /* regular case, compare two registers */
    r1 = lauK_exp2anyreg(fs, e1);
    r2 = lauK_exp2anyreg(fs, e2);
    op = binopr2op(opr, OPR_LT, OP_LT);
  }
  freeexps(fs, e1, e2);
  e1->u.info = condjump(fs, op, r1, r2, isfloat, 1);
  e1->k = VJMP;
}


/*
** Emit code for equality comparisons ('==', '~=').
** 'e1' was already put as RK by 'lauK_infix'.
*/
static void codeeq (FuncState *fs, BinOpr opr, expdesc *e1, expdesc *e2) {
  int r1, r2;
  int im;
  int isfloat = 0;  /* not needed here, but kept for symmetry */
  OpCode op;
  if (e1->k != VNONRELOC) {
    lau_assert(e1->k == VK || e1->k == VKINT || e1->k == VKFLT);
    swapexps(e1, e2);
  }
  r1 = lauK_exp2anyreg(fs, e1);  /* 1st expression must be in register */
  if (isSCnumber(e2, &im, &isfloat)) {
    op = OP_EQI;
    r2 = im;  /* immediate operand */
  }
  else if (exp2RK(fs, e2)) {  /* 2nd expression is constant? */
    op = OP_EQK;
    r2 = e2->u.info;  /* constant index */
  }
  else {
    op = OP_EQ;  /* will compare two registers */
    r2 = lauK_exp2anyreg(fs, e2);
  }
  freeexps(fs, e1, e2);
  e1->u.info = condjump(fs, op, r1, r2, isfloat, (opr == OPR_EQ));
  e1->k = VJMP;
}


/*
** Apply prefix operation 'op' to expression 'e'.
*/
void lauK_prefix (FuncState *fs, UnOpr opr, expdesc *e, int line) {
  static const expdesc ef = {VKINT, {0}, NO_JUMP, NO_JUMP};
  lauK_dischargevars(fs, e);
  switch (opr) {
    case OPR_MINUS:  /* use 'ef' as fake 2nd operand */
      if (constfolding(fs, cast_int(opr + LAU_OPDIV), e, &ef))
        break;
      /* else */ /* FALLTHROUGH */
    case OPR_LEN:
      codeunexpval(fs, unopr2op(opr), e, line);
      break;
    case OPR_NOT: codenot(fs, e); break;
    default: lau_assert(0);
  }
}


/*
** Process 1st operand 'v' of binary operation 'op' before reading
** 2nd operand.
*/
void lauK_infix (FuncState *fs, BinOpr op, expdesc *v) {
  lauK_dischargevars(fs, v);
  switch (op) {
    case OPR_AND: {
      lauK_goiftrue(fs, v);  /* go ahead only if 'v' is true */
      break;
    }
    case OPR_OR: {
      lauK_goiffalse(fs, v);  /* go ahead only if 'v' is false */
      break;
    }
    case OPR_ADD: case OPR_SUB:
    case OPR_MUL: case OPR_DIV:
    case OPR_MOD: case OPR_POW:
    {
      if (!tonumeral(v, NULL))
        lauK_exp2anyreg(fs, v);
      /* else keep numeral, which may be folded or used as an immediate
         operand */
      break;
    }
    case OPR_EQ: case OPR_NE: {
      if (!tonumeral(v, NULL))
        exp2RK(fs, v);
      /* else keep numeral, which may be an immediate operand */
      break;
    }
    case OPR_LT: case OPR_LE:
    case OPR_GT: case OPR_GE: {
      int dummy, dummy2;
      if (!isSCnumber(v, &dummy, &dummy2))
        lauK_exp2anyreg(fs, v);
      /* else keep numeral, which may be an immediate operand */
      break;
    }
    default: lau_assert(0);
  }
}


/*
** Finalize code for binary operation, after reading 2nd operand.
*/
void lauK_posfix (FuncState *fs, BinOpr opr,
                  expdesc *e1, expdesc *e2, int line) {
  lauK_dischargevars(fs, e2);
  if (foldbinop(opr) && constfolding(fs, cast_int(opr + LAU_OPADD), e1, e2))
    return;  /* done by folding */
  switch (opr) {
    case OPR_AND: {
      lau_assert(e1->t == NO_JUMP);  /* list closed by 'lauK_infix' */
      lauK_concat(fs, &e2->f, e1->f);
      *e1 = *e2;
      break;
    }
    case OPR_OR: {
      lau_assert(e1->f == NO_JUMP);  /* list closed by 'lauK_infix' */
      lauK_concat(fs, &e2->t, e1->t);
      *e1 = *e2;
      break;
    }
    case OPR_ADD: case OPR_MUL: {
      codecommutative(fs, opr, e1, e2, line);
      break;
    }
    case OPR_SUB: {
      if (finishbinexpneg(fs, e1, e2, OP_ADDI, line, TM_SUB))
        break; /* coded as (r1 + -I) */
      /* ELSE */
    }  /* FALLTHROUGH */
    case OPR_DIV: case OPR_MOD: case OPR_POW: {
      codearith(fs, opr, e1, e2, 0, line);
      break;
    }
    case OPR_EQ: case OPR_NE: {
      codeeq(fs, opr, e1, e2);
      break;
    }
    case OPR_GT: case OPR_GE: {
      /* '(a > b)' <=> '(b < a)';  '(a >= b)' <=> '(b <= a)' */
      swapexps(e1, e2);
      opr = cast(BinOpr, (opr - OPR_GT) + OPR_LT);
    }  /* FALLTHROUGH */
    case OPR_LT: case OPR_LE: {
      codeorder(fs, opr, e1, e2);
      break;
    }
    default: lau_assert(0);
  }
}


/*
** Change line information associated with current position, by removing
** previous info and adding it again with new line.
*/
void lauK_fixline (FuncState *fs, int line) {
  removelastlineinfo(fs);
  savelineinfo(fs, fs->f, line);
}


void lauK_settablesize (FuncState *fs, int pc, int ra, int asize, int hsize) {
  Instruction *inst = &fs->f->code[pc];
  int extra = asize / (MAXARG_vC + 1);  /* higher bits of array size */
  int rc = asize % (MAXARG_vC + 1);  /* lower bits of array size */
  int k = (extra > 0);  /* true iff needs extra argument */
  hsize = (hsize != 0) ? lauO_ceillog2(cast_uint(hsize)) + 1 : 0;
  *inst = CREATE_vABCk(OP_NEWTABLE, ra, hsize, rc, k);
  *(inst + 1) = CREATE_Ax(OP_EXTRAARG, extra);
}


/*
** Emit a SETLIST instruction.
** 'base' is register that keeps table;
** 'nelems' is #table plus those to be stored now;
** 'tostore' is number of values (in registers 'base + 1',...) to add to
** table (or LAU_MULTRET to add up to stack top).
*/
void lauK_setlist (FuncState *fs, int base, int nelems, int tostore) {
  lau_assert(tostore != 0);
  if (tostore == LAU_MULTRET)
    tostore = 0;
  if (nelems <= MAXARG_vC)
    lauK_codevABCk(fs, OP_SETLIST, base, tostore, nelems, 0);
  else {
    int extra = nelems / (MAXARG_vC + 1);
    nelems %= (MAXARG_vC + 1);
    lauK_codevABCk(fs, OP_SETLIST, base, tostore, nelems, 1);
    codeextraarg(fs, extra);
  }
  fs->freereg = cast_byte(base + 1);  /* free registers with list values */
}


/*
** return the final target of a jump (skipping jumps to jumps)
*/
static int finaltarget (Instruction *code, int i) {
  int count;
  for (count = 0; count < 100; count++) {  /* avoid infinite loops */
    Instruction pc = code[i];
    if (GET_OPCODE(pc) != OP_JMP)
      break;
    else
      i += GETARG_sJ(pc) + 1;
  }
  return i;
}


/*
** Do a final pass over the code of a function, doing small peephole
** optimizations and adjustments.
*/
#include "lopnames.h"
void lauK_finish (FuncState *fs) {
  int i;
  Proto *p = fs->f;
  if (p->flag & PF_VATAB)  /* will it use a vararg table? */
    p->flag &= cast_byte(~PF_VAHID);  /* then it will not use hidden args. */
  for (i = 0; i < fs->pc; i++) {
    Instruction *pc = &p->code[i];
    /* avoid "not used" warnings when assert is off (for 'onelau.c') */
    (void)lauP_isOT; (void)lauP_isIT;
    lau_assert(i == 0 || lauP_isOT(*(pc - 1)) == lauP_isIT(*pc));
    switch (GET_OPCODE(*pc)) {
      case OP_RETURN0: case OP_RETURN1: {
        if (!(fs->needclose || (p->flag & PF_VAHID)))
          break;  /* no extra work */
        /* else use OP_RETURN to do the extra work */
        SET_OPCODE(*pc, OP_RETURN);
      }  /* FALLTHROUGH */
      case OP_RETURN: case OP_TAILCALL: {
        if (fs->needclose)
          SETARG_k(*pc, 1);  /* signal that it needs to close */
        if (p->flag & PF_VAHID)  /* does it use hidden arguments? */
          SETARG_C(*pc, p->numparams + 1);  /* signal that */
        break;
      }
      case OP_GETVARG: {
        if (p->flag & PF_VATAB)  /* function has a vararg table? */
          SET_OPCODE(*pc, OP_GETTABLE);  /* must get vararg there */
        break;
      }
      case OP_VARARG: {
        if (p->flag & PF_VATAB)  /* function has a vararg table? */
          SETARG_k(*pc, 1);  /* must get vararg there */
        break;
      }
      case OP_JMP: {  /* to optimize jumps to jumps */
        int target = finaltarget(p->code, i);
        fixjump(fs, i, target);  /* jump directly to final target */
        break;
      }
      default: break;
    }
  }
}
