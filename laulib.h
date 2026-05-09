/*
** $Id: laulib.h $
** Lau standard libraries
** See Copyright Notice in lau.h
*/


#ifndef laulib_h
#define laulib_h

#include "lau.h"


/* version suffix for environment variable names */
#define LAU_VERSUFFIX          "_" LAU_VERSION_MAJOR "_" LAU_VERSION_MINOR

#define LAU_GLIBK		1
LAUMOD_API int (lauopen_base) (lau_State *L);

#define LAU_LOADLIBNAME	"package"
#define LAU_LOADLIBK	(LAU_GLIBK << 1)
LAUMOD_API int (lauopen_package) (lau_State *L);


#define LAU_COLIBNAME	"coroutine"
#define LAU_COLIBK	(LAU_LOADLIBK << 1)
LAUMOD_API int (lauopen_coroutine) (lau_State *L);

#define LAU_DBLIBNAME	"debug"
#define LAU_DBLIBK	(LAU_COLIBK << 1)
LAUMOD_API int (lauopen_debug) (lau_State *L);

#define LAU_IOLIBNAME	"io"
#define LAU_IOLIBK	(LAU_DBLIBK << 1)
LAUMOD_API int (lauopen_io) (lau_State *L);

#define LAU_MATHLIBNAME	"math"
#define LAU_MATHLIBK	(LAU_IOLIBK << 1)
LAUMOD_API int (lauopen_math) (lau_State *L);

#define LAU_OSLIBNAME	"os"
#define LAU_OSLIBK	(LAU_MATHLIBK << 1)
LAUMOD_API int (lauopen_os) (lau_State *L);

#define LAU_STRLIBNAME	"string"
#define LAU_STRLIBK	(LAU_OSLIBK << 1)
LAUMOD_API int (lauopen_string) (lau_State *L);

#define LAU_TABLIBNAME	"table"
#define LAU_TABLIBK	(LAU_STRLIBK << 1)
LAUMOD_API int (lauopen_table) (lau_State *L);

#define LAU_UTF8LIBNAME	"utf8"
#define LAU_UTF8LIBK	(LAU_TABLIBK << 1)
LAUMOD_API int (lauopen_utf8) (lau_State *L);


/* open selected libraries */
LAULIB_API void (lauL_openselectedlibs) (lau_State *L, int load, int preload);

/* open all libraries */
#define lauL_openlibs(L)	lauL_openselectedlibs(L, ~0, 0)


#endif
