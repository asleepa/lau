/*
** Lau core, libraries, and interpreter in a single file.
** Compiling just this file generates a complete Lau stand-alone
** program:
**
** $ gcc -O2 -std=c99 -o lau onelau.c -lm
**
** or (for C89)
**
** $ gcc -O2 -std=c89 -DLAU_USE_C89 -o lau onelau.c -lm
**
** or (for Linux)
**
** gcc -O2 -o lau -DLAU_USE_LINUX -Wl,-E onelau.c -lm -ldl
**
*/

/* default is to build the full interpreter */
#ifndef MAKE_LIB
#ifndef MAKE_LAUC
#ifndef MAKE_LAU
#define MAKE_LAU
#endif
#endif
#endif


/*
** Choose suitable platform-specific features. Default is no
** platform-specific features. Some of these options may need extra
** libraries such as -ldl -lreadline -lncurses
*/
#if 0
#define LAU_USE_LINUX
#define LAU_USE_MACOSX
#define LAU_USE_POSIX
#endif


/*
** Other specific features
*/
#if 0
#define LAU_32BITS
#define LAU_USE_C89
#endif


/* no need to change anything below this line ----------------------------- */

#include "lprefix.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* setup for lauconf.h */
#define LAU_CORE
#define LAU_LIB

#include "lauconf.h"

/* do not export internal symbols */
#undef LAUI_FUNC
#undef LAUI_DDEC
#undef LAUI_DDEF
#define LAUI_FUNC	static
#define LAUI_DDEC(def)	/* empty */
#define LAUI_DDEF	static

/* core -- used by all */
#include "lzio.c"
#include "lctype.c"
#include "lopcodes.c"
#include "lmem.c"
#include "lundump.c"
#include "ldump.c"
#include "lstate.c"
#include "lgc.c"
#include "llex.c"
#include "lcode.c"
#include "lparser.c"
#include "ldebug.c"
#include "lfunc.c"
#include "lobject.c"
#include "ltm.c"
#include "lstring.c"
#include "ltable.c"
#include "ldo.c"
#include "lvm.c"
#include "lapi.c"

/* auxiliary library -- used by all */
#include "lauxlib.c"

/* standard library  -- not used by lauc */
#ifndef MAKE_LAUC
#include "lbaselib.c"
#include "liolib.c"
#include "lmathlib.c"
#include "loadlib.c"
#include "loslib.c"
#include "lstrlib.c"
#include "ltablib.c"
#include "linit.c"
#endif

/* test library -- used only for internal development */
#if defined(LAU_DEBUG)
#include "ltests.c"
#endif

/* lau */
#ifdef MAKE_LAU
#include "lau.c"
#endif

/* lauc */
#ifdef MAKE_LAUC
#include "lauc.c"
#endif
