/*
** $Id: lundump.h $
** load precompiled Lau chunks
** See Copyright Notice in lau.h
*/

#ifndef lundump_h
#define lundump_h

#include <limits.h>

#include "llimits.h"
#include "lobject.h"
#include "lzio.h"


/* data to catch conversion errors */
#define LAUC_DATA	"\x19\x93\r\n\x1a\n"

#define LAUC_INT	-0x5678
#define LAUC_INST	0x12345678
#define LAUC_NUM	cast_num(-370.5)

/*
** Encode major-minor version in one byte, one nibble for each
*/
#define LAUC_VERSION	(LAU_VERSION_MAJOR_N*16+LAU_VERSION_MINOR_N)

#define LAUC_FORMAT	0	/* this is the official format */


/* load one chunk; from lundump.c */
LAUI_FUNC LClosure* lauU_undump (lau_State* L, ZIO* Z, Table *anchor,
                                 const char* name, int fixed);

/* dump one chunk; from ldump.c */
LAUI_FUNC int lauU_dump (lau_State* L, const Proto* f, lau_Writer w,
                         void* data, int strip);

#endif
