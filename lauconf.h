/*
** $Id: lauconf.h $
** Configuration file for Lau
** See Copyright Notice in lau.h
*/


#ifndef lauconf_h
#define lauconf_h

#include <limits.h>
#include <stddef.h>


/*
** ===================================================================
** General Configuration File for Lau
**
** Some definitions here can be changed externally, through the compiler
** (e.g., with '-D' options): They are commented out or protected
** by '#if !defined' guards. However, several other definitions
** should be changed directly here, either because they affect the
** Lau ABI (by making the changes here, you ensure that all software
** connected to Lau, such as C libraries, will be compiled with the same
** configuration); or because they are seldom changed.
**
** Search for "@@" to find all configurable definitions.
** ===================================================================
*/


/*
** {====================================================================
** System Configuration: macros to adapt (if needed) Lau to some
** particular platform, for instance restricting it to C89.
** =====================================================================
*/

/*
@@ LAU_USE_C89 controls the use of non-ISO-C89 features.
** Define it if you want Lau to avoid the use of a few C99 features
** or Windows-specific features on Windows.
*/
/* #define LAU_USE_C89 */


/*
** By default, Lau on Windows use (some) specific Windows features
*/
#if !defined(LAU_USE_C89) && defined(_WIN32) && !defined(_WIN32_WCE)
#define LAU_USE_WINDOWS  /* enable goodies for regular Windows */
#endif


#if defined(LAU_USE_WINDOWS)
#define LAU_DL_DLL	/* enable support for DLL */
#define LAU_USE_C89	/* broadly, Windows is C89 */
#endif


/*
** When POSIX DLL ('LAU_USE_DLOPEN') is enabled, the Lau stand-alone
** application will try to dynamically link a 'readline' facility
** for its REPL.  In that case, LAU_READLINELIB is the name of the
** library it will look for those facilities.  If lau.c cannot open
** the specified library, it will generate a warning and then run
** without 'readline'.  If that macro is not defined, lau.c will not
** use 'readline'.
*/
#if defined(LAU_USE_LINUX)
#define LAU_USE_POSIX
#define LAU_USE_DLOPEN		/* needs an extra library: -ldl */
#if !defined(LAU_READLINELIB)
#define LAU_READLINELIB		"libreadline.so"
#endif
#endif


#if defined(LAU_USE_MACOSX)
#define LAU_USE_POSIX
#define LAU_USE_DLOPEN		/* macOS does not need -ldl */
#if !defined(LAU_READLINELIB)
#define LAU_READLINELIB		"libedit.dylib"
#endif
#endif


#if defined(LAU_USE_IOS)
#define LAU_USE_POSIX
#define LAU_USE_DLOPEN
#endif


#if defined(LAU_USE_C89) && defined(LAU_USE_POSIX)
#error "POSIX is not compatible with C89"
#endif


/*
@@ LAUI_IS32INT is true iff 'int' has (at least) 32 bits.
*/
#define LAUI_IS32INT	((UINT_MAX >> 30) >= 3)

/* }================================================================== */



/*
** {==================================================================
** Configuration for Number types. These options should not be
** set externally, because any other code connected to Lau must
** use the same configuration.
** ===================================================================
*/

/*
@@ LAU_INT_TYPE defines the type for Lau integers.
@@ LAU_FLOAT_TYPE defines the type for Lau floats.
** Lau should work fine with any mix of these options supported
** by your C compiler. The usual configurations are 64-bit integers
** and 'double' (the default), 32-bit integers and 'float' (for
** restricted platforms), and 'long'/'double' (for C compilers not
** compliant with C99, which may not have support for 'long long').
*/

/* predefined options for LAU_INT_TYPE */
#define LAU_INT_INT		1
#define LAU_INT_LONG		2
#define LAU_INT_LONGLONG	3

/* predefined options for LAU_FLOAT_TYPE */
#define LAU_FLOAT_FLOAT		1
#define LAU_FLOAT_DOUBLE	2
#define LAU_FLOAT_LONGDOUBLE	3


/* Default configuration ('long long' and 'double', for 64-bit Lau) */
#define LAU_INT_DEFAULT		LAU_INT_LONGLONG
#define LAU_FLOAT_DEFAULT	LAU_FLOAT_DOUBLE


/*
@@ LAU_32BITS enables Lau with 32-bit integers and 32-bit floats.
*/
/* #define LAU_32BITS */


/*
@@ LAU_C89_NUMBERS ensures that Lau uses the largest types available for
** C89 ('long' and 'double'); Windows always has '__int64', so it does
** not need to use this case.
*/
#if defined(LAU_USE_C89) && !defined(LAU_USE_WINDOWS)
#define LAU_C89_NUMBERS		1
#else
#define LAU_C89_NUMBERS		0
#endif


#if defined(LAU_32BITS)	/* { */
/*
** 32-bit integers and 'float'
*/
#if LAUI_IS32INT  /* use 'int' if big enough */
#define LAU_INT_TYPE	LAU_INT_INT
#else  /* otherwise use 'long' */
#define LAU_INT_TYPE	LAU_INT_LONG
#endif
#define LAU_FLOAT_TYPE	LAU_FLOAT_FLOAT

#elif LAU_C89_NUMBERS	/* }{ */
/*
** largest types available for C89 ('long' and 'double')
*/
#define LAU_INT_TYPE	LAU_INT_LONG
#define LAU_FLOAT_TYPE	LAU_FLOAT_DOUBLE

#else		/* }{ */
/* use defaults */

#define LAU_INT_TYPE	LAU_INT_DEFAULT
#define LAU_FLOAT_TYPE	LAU_FLOAT_DEFAULT

#endif				/* } */


/* }================================================================== */



/*
** {==================================================================
** Configuration for Paths.
** ===================================================================
*/

/*
** LAU_PATH_SEP is the character that separates templates in a path.
** LAU_PATH_MARK is the string that marks the substitution points in a
** template.
** LAU_EXEC_DIR in a Windows path is replaced by the executable's
** directory.
*/
#define LAU_PATH_SEP            ";"
#define LAU_PATH_MARK           "?"
#define LAU_EXEC_DIR            "!"


/*
@@ LAU_PATH_DEFAULT is the default path that Lau uses to look for
** Lau libraries.
@@ LAU_CPATH_DEFAULT is the default path that Lau uses to look for
** C libraries.
** CHANGE them if your machine has a non-conventional directory
** hierarchy or if you want to install your libraries in
** non-conventional directories.
*/

#define LAU_VDIR	LAU_VERSION_MAJOR "." LAU_VERSION_MINOR
#if defined(_WIN32)	/* { */
/*
** In Windows, any exclamation mark ('!') in the path is replaced by the
** path of the directory of the executable file of the current process.
*/
#define LAU_LDIR	"!\\lau\\"
#define LAU_CDIR	"!\\"
#define LAU_SHRDIR	"!\\..\\share\\lau\\" LAU_VDIR "\\"

#if !defined(LAU_PATH_DEFAULT)
#define LAU_PATH_DEFAULT  \
		LAU_LDIR "?.laum;"  LAU_LDIR "?\\init.laum;" \
		LAU_CDIR "?.laum;"  LAU_CDIR "?\\init.laum;" \
		LAU_SHRDIR "?.laum;"  LAU_SHRDIR "?\\init.laum;" \
		".\\?.laum;" ".\\?\\init.laum"
#endif

#if !defined(LAU_CPATH_DEFAULT)
#define LAU_CPATH_DEFAULT \
		LAU_CDIR "?.dll;" \
		LAU_CDIR "..\\lib\\lau\\"  LAU_VDIR "\\?.dll;" \
		LAU_CDIR "loadall.dll;" ".\\?.dll"
#endif

#else			/* }{ */

#define LAU_ROOT	"/usr/local/"
#define LAU_LDIR	LAU_ROOT "share/lau/" LAU_VDIR "/"
#define LAU_CDIR	LAU_ROOT "lib/lau/" LAU_VDIR "/"

#if !defined(LAU_PATH_DEFAULT)
#define LAU_PATH_DEFAULT  \
		LAU_LDIR "?.lau;"  LAU_LDIR "?/init.lau;" \
		LAU_CDIR "?.lau;"  LAU_CDIR "?/init.lau;" \
		"./?.lau;" "./?/init.lau"
#endif

#if !defined(LAU_CPATH_DEFAULT)
#define LAU_CPATH_DEFAULT \
		LAU_CDIR "?.so;" LAU_CDIR "loadall.so;" "./?.so"
#endif

#endif			/* } */


/*
@@ LAU_DIRSEP is the directory separator (for submodules).
** CHANGE it if your machine does not use "/" as the directory separator
** and is not Windows. (On Windows Lau automatically uses "\".)
*/
#if !defined(LAU_DIRSEP)

#if defined(_WIN32)
#define LAU_DIRSEP	"\\"
#else
#define LAU_DIRSEP	"/"
#endif

#endif


/*
** LAU_IGMARK is a mark to ignore all after it when building the
** module name (e.g., used to build the lauopen_ function name).
** Typically, the suffix after the mark is the module version,
** as in "mod-v1.2.so".
*/
#define LAU_IGMARK		"-"

/* }================================================================== */


/*
** {==================================================================
** Marks for exported symbols in the C code
** ===================================================================
*/

/*
@@ LAU_API is a mark for all core API functions.
@@ LAULIB_API is a mark for all auxiliary library functions.
@@ LAUMOD_API is a mark for all standard library opening functions.
** CHANGE them if you need to define those functions in some special way.
** For instance, if you want to create one Windows DLL with the core and
** the libraries, you may want to use the following definition (define
** LAU_BUILD_AS_DLL to get it).
*/
#if defined(LAU_BUILD_AS_DLL)	/* { */

#if defined(LAU_CORE) || defined(LAU_LIB)	/* { */
#define LAU_API __declspec(dllexport)
#else						/* }{ */
#define LAU_API __declspec(dllimport)
#endif						/* } */

#else				/* }{ */

#define LAU_API		extern

#endif				/* } */


/*
** More often than not the libs go together with the core.
*/
#define LAULIB_API	LAU_API

#if defined(__cplusplus)
/* Lau uses the "C name" when calling open functions */
#define LAUMOD_API	extern "C"
#else
#define LAUMOD_API	LAU_API
#endif

/* }================================================================== */


/*
** {==================================================================
** Compatibility with previous versions
** ===================================================================
*/

/*
@@ LAU_COMPAT_GLOBAL avoids 'global' being a reserved word
*/
#if !defined(LAU_COMPAT_GLOBAL)
#define LAU_COMPAT_GLOBAL	1
#endif


/*
@@ LAU_COMPAT_LOOPVAR makes for-loop control variables not read-only,
** as they were in previous versions.
*/
#if !defined(LAU_COMPAT_LOOPVAR)
#define LAU_COMPAT_LOOPVAR	0
#endif


/*
@@ The following macros supply trivial compatibility for some
** changes in the API. The macros themselves document how to
** change your code to avoid using them.
** (Once more, these macros were officially removed in 5.3, but they are
** still available here.)
*/
#define lau_strlen(L,i)		lau_rawlen(L, (i))

#define lau_objlen(L,i)		lau_rawlen(L, (i))

#define lau_equal(L,idx1,idx2)		lau_compare(L,(idx1),(idx2),LAU_OPEQ)
#define lau_lessthan(L,idx1,idx2)	lau_compare(L,(idx1),(idx2),LAU_OPLT)

/* }================================================================== */



/*
** {==================================================================
** Configuration for Numbers (low-level part).
** Change these definitions if no predefined LAU_FLOAT_* / LAU_INT_*
** satisfy your needs.
** ===================================================================
*/

/*
@@ LAUI_UACNUMBER is the result of a 'default argument promotion'
@@ over a floating number.
@@ l_floatatt(x) corrects float attribute 'x' to the proper float type
** by prefixing it with one of FLT/DBL/LDBL.
@@ LAU_NUMBER_FRMLEN is the length modifier for writing floats.
@@ LAU_NUMBER_FMT is the format for writing floats with the maximum
** number of digits that respects tostring(tonumber(numeral)) == numeral.
** (That would be floor(log10(2^n)), where n is the number of bits in
** the float mantissa.)
@@ LAU_NUMBER_FMT_N is the format for writing floats with the minimum
** number of digits that ensures tonumber(tostring(number)) == number.
** (That would be LAU_NUMBER_FMT+2.)
@@ l_mathop allows the addition of an 'l' or 'f' to all math operations.
@@ l_floor takes the floor of a float.
@@ lau_str2number converts a decimal numeral to a number.
*/


/* The following definition is good for most cases here */

#define l_floor(x)		(l_mathop(floor)(x))


/* now the variable definitions */

#if LAU_FLOAT_TYPE == LAU_FLOAT_FLOAT		/* { single float */

#define LAU_NUMBER	float

#define l_floatatt(n)		(FLT_##n)

#define LAUI_UACNUMBER	double

#define LAU_NUMBER_FRMLEN	""
#define LAU_NUMBER_FMT		"%.7g"
#define LAU_NUMBER_FMT_N	"%.9g"

#define l_mathop(op)		op##f

#define lau_str2number(s,p)	strtof((s), (p))


#elif LAU_FLOAT_TYPE == LAU_FLOAT_LONGDOUBLE	/* }{ long double */

#define LAU_NUMBER	long double

#define l_floatatt(n)		(LDBL_##n)

#define LAUI_UACNUMBER	long double

#define LAU_NUMBER_FRMLEN	"L"
#define LAU_NUMBER_FMT		"%.19Lg"
#define LAU_NUMBER_FMT_N	"%.21Lg"

#define l_mathop(op)		op##l

#define lau_str2number(s,p)	strtold((s), (p))

#elif LAU_FLOAT_TYPE == LAU_FLOAT_DOUBLE	/* }{ double */

#define LAU_NUMBER	double

#define l_floatatt(n)		(DBL_##n)

#define LAUI_UACNUMBER	double

#define LAU_NUMBER_FRMLEN	""
#define LAU_NUMBER_FMT		"%.15g"
#define LAU_NUMBER_FMT_N	"%.17g"

#define l_mathop(op)		op

#define lau_str2number(s,p)	strtod((s), (p))

#else						/* }{ */

#error "numeric float type not defined"

#endif					/* } */



/*
@@ LAU_UNSIGNED is the unsigned version of LAU_INTEGER.
@@ LAUI_UACINT is the result of a 'default argument promotion'
@@ over a LAU_INTEGER.
@@ LAU_INTEGER_FRMLEN is the length modifier for reading/writing integers.
@@ LAU_INTEGER_FMT is the format for writing integers.
@@ LAU_MAXINTEGER is the maximum value for a LAU_INTEGER.
@@ LAU_MININTEGER is the minimum value for a LAU_INTEGER.
@@ LAU_MAXUNSIGNED is the maximum value for a LAU_UNSIGNED.
@@ lau_integer2str converts an integer to a string.
*/


/* The following definitions are good for most cases here */

#define LAU_INTEGER_FMT		"%" LAU_INTEGER_FRMLEN "d"

#define LAUI_UACINT		LAU_INTEGER

#define lau_integer2str(s,sz,n)  \
	l_sprintf((s), sz, LAU_INTEGER_FMT, (LAUI_UACINT)(n))

/*
** use LAUI_UACINT here to avoid problems with promotions (which
** can turn a comparison between unsigneds into a signed comparison)
*/
#define LAU_UNSIGNED		unsigned LAUI_UACINT


/* now the variable definitions */

#if LAU_INT_TYPE == LAU_INT_INT		/* { int */

#define LAU_INTEGER		int
#define LAU_INTEGER_FRMLEN	""

#define LAU_MAXINTEGER		INT_MAX
#define LAU_MININTEGER		INT_MIN

#define LAU_MAXUNSIGNED		UINT_MAX

#elif LAU_INT_TYPE == LAU_INT_LONG	/* }{ long */

#define LAU_INTEGER		long
#define LAU_INTEGER_FRMLEN	"l"

#define LAU_MAXINTEGER		LONG_MAX
#define LAU_MININTEGER		LONG_MIN

#define LAU_MAXUNSIGNED		ULONG_MAX

#elif LAU_INT_TYPE == LAU_INT_LONGLONG	/* }{ long long */

/* use presence of macro LLONG_MAX as proxy for C99 compliance */
#if defined(LLONG_MAX)		/* { */
/* use ISO C99 stuff */

#define LAU_INTEGER		long long
#define LAU_INTEGER_FRMLEN	"ll"

#define LAU_MAXINTEGER		LLONG_MAX
#define LAU_MININTEGER		LLONG_MIN

#define LAU_MAXUNSIGNED		ULLONG_MAX

#elif defined(LAU_USE_WINDOWS) /* }{ */
/* in Windows, can use specific Windows types */

#define LAU_INTEGER		__int64
#define LAU_INTEGER_FRMLEN	"I64"

#define LAU_MAXINTEGER		_I64_MAX
#define LAU_MININTEGER		_I64_MIN

#define LAU_MAXUNSIGNED		_UI64_MAX

#else				/* }{ */

#error "Compiler does not support 'long long'. Use option '-DLAU_32BITS' \
  or '-DLAU_C89_NUMBERS' (see file 'lauconf.h' for details)"

#endif				/* } */

#else				/* }{ */

#error "numeric integer type not defined"

#endif				/* } */

/* }================================================================== */


/*
** {==================================================================
** Dependencies with C99 and other C details
** ===================================================================
*/

/*
@@ l_sprintf is equivalent to 'snprintf' or 'sprintf' in C89.
** (All uses in Lau have only one format item.)
*/
#if !defined(LAU_USE_C89)
#define l_sprintf(s,sz,f,i)	snprintf(s,sz,f,i)
#else
#define l_sprintf(s,sz,f,i)	((void)(sz), sprintf(s,f,i))
#endif


/*
@@ lau_strx2number converts a hexadecimal numeral to a number.
** In C99, 'strtod' does that conversion. Otherwise, you can
** leave 'lau_strx2number' undefined and Lau will provide its own
** implementation.
*/
#if !defined(LAU_USE_C89)
#define lau_strx2number(s,p)		lau_str2number(s,p)
#endif


/*
@@ lau_pointer2str converts a pointer to a readable string in a
** non-specified way.
*/
#define lau_pointer2str(buff,sz,p)	l_sprintf(buff,sz,"%p",p)


/*
** 'strtof' and 'opf' variants for math functions are not valid in
** C89. Otherwise, the macro 'HUGE_VALF' is a good proxy for testing the
** availability of these variants. ('math.h' is already included in
** all files that use these macros.)
*/
#if defined(LAU_USE_C89) || (defined(HUGE_VAL) && !defined(HUGE_VALF))
#undef l_mathop  /* variants not available */
#undef lau_str2number
#define l_mathop(op)		(lau_Number)op  /* no variant */
#define lau_str2number(s,p)	((lau_Number)strtod((s), (p)))
#endif


/*
@@ LAU_KCONTEXT is the type of the context ('ctx') for continuation
** functions.  It must be a numerical type; Lau will use 'intptr_t' if
** available, otherwise it will use 'ptrdiff_t' (the nearest thing to
** 'intptr_t' in C89)
*/
#define LAU_KCONTEXT	ptrdiff_t

#if !defined(LAU_USE_C89) && defined(__STDC_VERSION__) && \
    __STDC_VERSION__ >= 199901L
#include <stdint.h>
#if defined(INTPTR_MAX)  /* even in C99 this type is optional */
#undef LAU_KCONTEXT
#define LAU_KCONTEXT	intptr_t
#endif
#endif


/*
@@ lau_getlocaledecpoint gets the locale "radix character" (decimal point).
** Change that if you do not want to use C locales. (Code using this
** macro must include the header 'locale.h'.)
*/
#if !defined(lau_getlocaledecpoint)
#define lau_getlocaledecpoint()		(localeconv()->decimal_point[0])
#endif


/*
** macros to improve jump prediction, used mostly for error handling
** and debug facilities. (Some macros in the Lau API use these macros.
** Define LAU_NOBUILTIN if you do not want '__builtin_expect' in your
** code.)
*/
#if !defined(laui_likely)

#if !defined(LAU_NOBUILTIN) && defined(__GNUC__) && (__GNUC__ >= 3)
#define laui_likely(x)		(__builtin_expect(((x) != 0), 1))
#define laui_unlikely(x)	(__builtin_expect(((x) != 0), 0))
#else
#define laui_likely(x)		(x)
#define laui_unlikely(x)	(x)
#endif

#endif



/* }================================================================== */


/*
** {==================================================================
** Language Variations
** =====================================================================
*/

/*
@@ LAU_NOCVTN2S/LAU_NOCVTS2N control how Lau performs some
** coercions. Define LAU_NOCVTN2S to turn off automatic coercion from
** numbers to strings. Define LAU_NOCVTS2N to turn off automatic
** coercion from strings to numbers.
*/
/* #define LAU_NOCVTN2S */
/* #define LAU_NOCVTS2N */


/*
@@ LAU_USE_APICHECK turns on several consistency checks on the C API.
** Define it as a help when debugging C code.
*/
/* #define LAU_USE_APICHECK */

/* }================================================================== */


/*
** {==================================================================
** Macros that affect the API and must be stable (that is, must be the
** same when you compile Lau and when you compile code that links to
** Lau).
** =====================================================================
*/

/*
@@ LAU_EXTRASPACE defines the size of a raw memory area associated with
** a Lau state with very fast access.
** CHANGE it if you need a different size.
*/
#define LAU_EXTRASPACE		(sizeof(void *))


/*
@@ LAU_IDSIZE gives the maximum size for the description of the source
** of a function in debug information.
** CHANGE it if you want a different size.
*/
#define LAU_IDSIZE	60


/*
@@ LAUL_BUFFERSIZE is the initial buffer size used by the lauxlib
** buffer system.
*/
#define LAUL_BUFFERSIZE   ((int)(16 * sizeof(void*) * sizeof(lau_Number)))


/*
@@ LAUI_MAXALIGN defines fields that ensure proper alignment for
** memory areas offered by Lau (e.g., userdata memory).
** Add fields to it if you need alignment for non-ISO objects.
*/
#if defined(LLONG_MAX)
/* use ISO C99 stuff */
#define LAUI_MAXALIGN long double u; void *s; long long l
#else
/* use only C89 stuff */
#define LAUI_MAXALIGN  lau_Number n; double u; void *s; lau_Integer i; long l
#endif

/* }================================================================== */





/* =================================================================== */

/*
** Local configuration. You can use this space to add your redefinitions
** without modifying the main part of the file.
*/



#endif

