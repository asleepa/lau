/*
** $Id: ltests.h $
** Internal Header for Debugging of the Lau Implementation
** See Copyright Notice in lau.h
*/

#ifndef ltests_h
#define ltests_h


#include <stdio.h>
#include <stdlib.h>

/* test Lau with compatibility code */
#define LAU_COMPAT_MATHLIB
#undef LAU_COMPAT_GLOBAL
#define LAU_COMPAT_GLOBAL	0


#define LAU_DEBUG


/* turn on assertions */
#define LAUI_ASSERT


/* to avoid warnings, and to make sure value is really unused */
#define UNUSED(x)       (x=0, (void)(x))


/* test for sizes in 'l_sprintf' (make sure whole buffer is available) */
#undef l_sprintf
#if !defined(LAU_USE_C89)
#define l_sprintf(s,sz,f,i)	(memset(s,0xAB,sz), snprintf(s,sz,f,i))
#else
#define l_sprintf(s,sz,f,i)	(memset(s,0xAB,sz), sprintf(s,f,i))
#endif


/* get a chance to test code without jump tables */
#define LAU_USE_JUMPTABLE	0


/* use 32-bit integers in random generator */
#define LAU_RAND32


/* test stack reallocation without strict address use */
#define LAUI_STRICT_ADDRESS	0


/* memory-allocator control variables */
typedef struct Memcontrol {
  int failnext;
  unsigned long numblocks;
  unsigned long total;
  unsigned long maxmem;
  unsigned long memlimit;
  unsigned long countlimit;
  unsigned long objcount[LAU_NUMTYPES];
} Memcontrol;

LAU_API Memcontrol l_memcontrol;


#define laui_tracegc(L,f)		laui_tracegctest(L, f)
extern void laui_tracegctest (lau_State *L, int first);


/*
** generic variable for debug tricks
*/
extern void *l_Trick;


/*
** Function to traverse and check all memory used by Lau
*/
extern int lau_checkmemory (lau_State *L);

/*
** Function to print an object GC-friendly
*/
struct GCObject;
extern void lau_printobj (lau_State *L, struct GCObject *o);


/*
** Function to print a value
*/
struct TValue;
extern void lau_printvalue (struct TValue *v);

/*
** Function to print the stack
*/
extern void lau_printstack (lau_State *L);
extern int lau_printallstack (lau_State *L);


/* test for lock/unlock */

struct L_EXTRA { int lock; int *plock; };
#undef LAU_EXTRASPACE
#define LAU_EXTRASPACE	sizeof(struct L_EXTRA)
#define getlock(l)	cast(struct L_EXTRA*, lau_getextraspace(l))
#define laui_userstateopen(l)  \
	(getlock(l)->lock = 0, getlock(l)->plock = &(getlock(l)->lock))
#define laui_userstateclose(l)  \
  lau_assert(getlock(l)->lock == 1 && getlock(l)->plock == &(getlock(l)->lock))
#define laui_userstatethread(l,l1) \
  lau_assert(getlock(l1)->plock == getlock(l)->plock)
#define laui_userstatefree(l,l1) \
  lau_assert(getlock(l)->plock == getlock(l1)->plock)
#define lau_lock(l)     lau_assert((*getlock(l)->plock)++ == 0)
#define lau_unlock(l)   lau_assert(--(*getlock(l)->plock) == 0)



LAU_API int lauB_opentests (lau_State *L);

LAU_API void *debug_realloc (void *ud, void *block,
                             size_t osize, size_t nsize);


#define lauL_newstate()  \
	lau_newstate(debug_realloc, &l_memcontrol, lauL_makeseed(NULL))
#define laui_openlibs(L)  \
  {  lauL_openlibs(L); \
     lauL_requiref(L, "T", lauB_opentests, 1); \
     lau_pop(L, 1); }




/* change some sizes to give some bugs a chance */

#undef LAUL_BUFFERSIZE
#define LAUL_BUFFERSIZE		23
#define MINSTRTABSIZE		2
#define MAXIWTHABS		3

#define STRCACHE_N	23
#define STRCACHE_M	5

#define MAXINDEXRK	1


/*
** Reduce maximum stack size to make stack-overflow tests run faster.
** (But value is still large enough to overflow smaller integers.)
*/
#define LAUI_MAXSTACK   68000


/* test mode uses more stack space */
#undef LAUI_MAXCCALLS
#define LAUI_MAXCCALLS	180


/* force Lau to use its own implementations */
#undef lau_strx2number
#undef lau_number2strx


#endif

