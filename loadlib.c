/*
** $Id: loadlib.c $
** Dynamic library loader for Lau
** See Copyright Notice in lau.h
**
** This module contains an implementation of loadlib for Unix systems
** that have dlfcn, an implementation for Windows, and a stub for other
** systems.
*/

#define loadlib_c
#define LAU_LIB

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lau.h"

#include "lauxlib.h"
#include "laulib.h"
#include "llimits.h"


/*
** LAU_CSUBSEP is the character that replaces dots in submodule names
** when searching for a C loader.
** LAU_LSUBSEP is the character that replaces dots in submodule names
** when searching for a Lau loader.
*/
#if !defined(LAU_CSUBSEP)
#define LAU_CSUBSEP		LAU_DIRSEP
#endif

#if !defined(LAU_LSUBSEP)
#define LAU_LSUBSEP		LAU_DIRSEP
#endif


/* prefix for open functions in C libraries */
#define LAU_POF		"lauopen_"

/* separator for open functions in C libraries */
#define LAU_OFSEP	"_"


/*
** key for table in the registry that keeps handles
** for all loaded C libraries
*/
static const char *const CLIBS = "_CLIBS";

#define LIB_FAIL	"open"


#define setprogdir(L)           ((void)0)


/* cast void* to a Lau function */
#define cast_Lfunc(p)	cast(lau_CFunction, cast_func(p))


/*
** system-dependent functions
*/

/*
** unload library 'lib'
*/
static void lsys_unloadlib (void *lib);

/*
** load C library in file 'path'. If 'seeglb', load with all names in
** the library global.
** Returns the library; in case of error, returns NULL plus an
** error string in the stack.
*/
static void *lsys_load (lau_State *L, const char *path, int seeglb);

/*
** Try to find a function named 'sym' in library 'lib'.
** Returns the function; in case of error, returns NULL plus an
** error string in the stack.
*/
static lau_CFunction lsys_sym (lau_State *L, void *lib, const char *sym);




#if defined(LAU_USE_DLOPEN)	/* { */
/*
** {========================================================================
** This is an implementation of loadlib based on the dlfcn interface,
** which is available in all POSIX systems.
** =========================================================================
*/

#include <dlfcn.h>


static void lsys_unloadlib (void *lib) {
  dlclose(lib);
}


static void *lsys_load (lau_State *L, const char *path, int seeglb) {
  void *lib = dlopen(path, RTLD_NOW | (seeglb ? RTLD_GLOBAL : RTLD_LOCAL));
  if (l_unlikely(lib == NULL))
    lau_pushstring(L, dlerror());
  return lib;
}


static lau_CFunction lsys_sym (lau_State *L, void *lib, const char *sym) {
  lau_CFunction f = cast_Lfunc(dlsym(lib, sym));
  if (l_unlikely(f == NULL))
    lau_pushstring(L, dlerror());
  return f;
}

/* }====================================================== */



#elif defined(LAU_DL_DLL)	/* }{ */
/*
** {======================================================================
** This is an implementation of loadlib for Windows using native functions.
** =======================================================================
*/

#include <windows.h>


/*
** optional flags for LoadLibraryEx
*/
#if !defined(LAU_LLE_FLAGS)
#define LAU_LLE_FLAGS	0
#endif


#undef setprogdir


/*
** Replace in the path (on the top of the stack) any occurrence
** of LAU_EXEC_DIR with the executable's path.
*/
static void setprogdir (lau_State *L) {
  char buff[MAX_PATH + 1];
  char *lb;
  DWORD nsize = sizeof(buff)/sizeof(char);
  DWORD n = GetModuleFileNameA(NULL, buff, nsize);  /* get exec. name */
  if (n == 0 || n == nsize || (lb = strrchr(buff, '\\')) == NULL)
    lauL_error(L, "unable to get ModuleFileName");
  else {
    *lb = '\0';  /* cut name on the last '\\' to get the path */
    lauL_gsub(L, lau_tostring(L, -1), LAU_EXEC_DIR, buff);
    lau_remove(L, -2);  /* remove original string */
  }
}




static void pusherror (lau_State *L) {
  int error = GetLastError();
  char buffer[128];
  if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, error, 0, buffer, sizeof(buffer)/sizeof(char), NULL))
    lau_pushstring(L, buffer);
  else
    lau_pushfstring(L, "system error %d\n", error);
}

static void lsys_unloadlib (void *lib) {
  FreeLibrary((HMODULE)lib);
}


static void *lsys_load (lau_State *L, const char *path, int seeglb) {
  HMODULE lib = LoadLibraryExA(path, NULL, LAU_LLE_FLAGS);
  (void)(seeglb);  /* not used: symbols are 'global' by default */
  if (lib == NULL) pusherror(L);
  return lib;
}


static lau_CFunction lsys_sym (lau_State *L, void *lib, const char *sym) {
  lau_CFunction f = cast_Lfunc(GetProcAddress((HMODULE)lib, sym));
  if (f == NULL) pusherror(L);
  return f;
}

/* }====================================================== */


#else				/* }{ */
/*
** {======================================================
** Fallback for other systems
** =======================================================
*/

#undef LIB_FAIL
#define LIB_FAIL	"absent"


#define DLMSG	"dynamic libraries not enabled; check your Lau installation"


static void lsys_unloadlib (void *lib) {
  (void)(lib);  /* not used */
}


static void *lsys_load (lau_State *L, const char *path, int seeglb) {
  (void)(path); (void)(seeglb);  /* not used */
  lau_pushliteral(L, DLMSG);
  return NULL;
}


static lau_CFunction lsys_sym (lau_State *L, void *lib, const char *sym) {
  (void)(lib); (void)(sym);  /* not used */
  lau_pushliteral(L, DLMSG);
  return NULL;
}

/* }====================================================== */
#endif				/* } */


/*
** {==================================================================
** Set Paths
** ===================================================================
*/

/*
** LAU_PATH_VAR and LAU_CPATH_VAR are the names of the environment
** variables that Lau check to set its paths.
*/
#if !defined(LAU_PATH_VAR)
#define LAU_PATH_VAR    "LAU_PATH"
#endif

#if !defined(LAU_CPATH_VAR)
#define LAU_CPATH_VAR   "LAU_CPATH"
#endif



/*
** return registry.LAU_NOENV as a boolean
*/
static int noenv (lau_State *L) {
  int b;
  lau_getfield(L, LAU_REGISTRYINDEX, "LAU_NOENV");
  b = lau_toboolean(L, -1);
  lau_pop(L, 1);  /* remove value */
  return b;
}


/*
** Set a path. (If using the default path, assume it is a string
** literal in C and create it as an external string.)
*/
static void setpath (lau_State *L, const char *fieldname,
                                   const char *envname,
                                   const char *dft) {
  const char *dftmark;
  const char *nver = lau_pushfstring(L, "%s%s", envname, LAU_VERSUFFIX);
  const char *path = getenv(nver);  /* try versioned name */
  if (path == NULL)  /* no versioned environment variable? */
    path = getenv(envname);  /* try unversioned name */
  if (path == NULL || noenv(L))  /* no environment variable? */
    lau_pushexternalstring(L, dft, strlen(dft), NULL, NULL);  /* use default */
  else if ((dftmark = strstr(path, LAU_PATH_SEP LAU_PATH_SEP)) == NULL)
    lau_pushstring(L, path);  /* nothing to change */
  else {  /* path contains a ";;": insert default path in its place */
    size_t len = strlen(path);
    lauL_Buffer b;
    lauL_buffinit(L, &b);
    if (path < dftmark) {  /* is there a prefix before ';;'? */
      lauL_addlstring(&b, path, ct_diff2sz(dftmark - path));  /* add it */
      lauL_addchar(&b, *LAU_PATH_SEP);
    }
    lauL_addstring(&b, dft);  /* add default */
    if (dftmark < path + len - 2) {  /* is there a suffix after ';;'? */
      lauL_addchar(&b, *LAU_PATH_SEP);
      lauL_addlstring(&b, dftmark + 2, ct_diff2sz((path + len - 2) - dftmark));
    }
    lauL_pushresult(&b);
  }
  setprogdir(L);
  lau_setfield(L, -3, fieldname);  /* package[fieldname] = path value */
  lau_pop(L, 1);  /* pop versioned variable name ('nver') */
}

/* }================================================================== */


/*
** External strings created by DLLs may need the DLL code to be
** deallocated. This implies that a DLL can only be unloaded after all
** its strings were deallocated. To ensure that, we create a 'library
** string' to represent each DLL, and when this string is deallocated
** it closes its corresponding DLL.
** (The string itself is irrelevant; its userdata is the DLL pointer.)
*/


/*
** return registry.CLIBS[path]
*/
static void *checkclib (lau_State *L, const char *path) {
  void *plib;
  lau_getfield(L, LAU_REGISTRYINDEX, CLIBS);
  lau_getfield(L, -1, path);
  plib = lau_touserdata(L, -1);  /* plib = CLIBS[path] */
  lau_pop(L, 2);  /* pop CLIBS table and 'plib' */
  return plib;
}


/*
** Deallocate function for library strings.
** Unload the DLL associated with the string being deallocated.
*/
static void *freelib (void *ud, void *ptr, size_t osize, size_t nsize) {
  /* string itself is irrelevant and static */
  (void)ptr; (void)osize; (void)nsize;
  lsys_unloadlib(ud);  /* unload library represented by the string */
  return NULL;
}


/*
** Create a library string that, when deallocated, will unload 'plib'
*/
static void createlibstr (lau_State *L, void *plib) {
  /* common content for all library strings */
  static const char dummy[] = "01234567890";
  lau_pushexternalstring(L, dummy, sizeof(dummy) - 1, freelib, plib);
}


/*
** registry.CLIBS[path] = plib          -- for queries.
** Also create a reference to strlib, so that the library string will
** only be collected when registry.CLIBS is collected.
*/
static void addtoclib (lau_State *L, const char *path, void *plib) {
  lau_getfield(L, LAU_REGISTRYINDEX, CLIBS);
  lau_pushlightuserdata(L, plib);
  lau_setfield(L, -2, path);  /* CLIBS[path] = plib */
  createlibstr(L, plib);
  lauL_ref(L, -2);  /* keep library string in CLIBS */
  lau_pop(L, 1);  /* pop CLIBS table */
}


/* error codes for 'lookforfunc' */
#define ERRLIB		1
#define ERRFUNC		2

/*
** Look for a C function named 'sym' in a dynamically loaded library
** 'path'.
** First, check whether the library is already loaded; if not, try
** to load it.
** Then, if 'sym' is '*', return true (as library has been loaded).
** Otherwise, look for symbol 'sym' in the library and push a
** C function with that symbol.
** Return 0 with 'true' or a function in the stack; in case of
** errors, return an error code with an error message in the stack.
*/
static int lookforfunc (lau_State *L, const char *path, const char *sym) {
  void *reg = checkclib(L, path);  /* check loaded C libraries */
  if (reg == NULL) {  /* must load library? */
    reg = lsys_load(L, path, *sym == '*');  /* global symbols if 'sym'=='*' */
    if (reg == NULL) return ERRLIB;  /* unable to load library */
    addtoclib(L, path, reg);
  }
  if (*sym == '*') {  /* loading only library (no function)? */
    lau_pushboolean(L, 1);  /* return 'true' */
    return 0;  /* no errors */
  }
  else {
    lau_CFunction f = lsys_sym(L, reg, sym);
    if (f == NULL)
      return ERRFUNC;  /* unable to find function */
    lau_pushcfunction(L, f);  /* else create new function */
    return 0;  /* no errors */
  }
}


static int ll_loadlib (lau_State *L) {
  const char *path = lauL_checkstring(L, 1);
  const char *init = lauL_checkstring(L, 2);
  int stat = lookforfunc(L, path, init);
  if (l_likely(stat == 0))  /* no errors? */
    return 1;  /* return the loaded function */
  else {  /* error; error message is on stack top */
    lauL_pushfail(L);
    lau_insert(L, -2);
    lau_pushstring(L, (stat == ERRLIB) ?  LIB_FAIL : "init");
    return 3;  /* return fail, error message, and where */
  }
}



/*
** {======================================================
** 'require' function
** =======================================================
*/


static int readable (const char *filename) {
  FILE *f = fopen(filename, "r");  /* try to open file */
  if (f == NULL) return 0;  /* open failed */
  fclose(f);
  return 1;
}


/*
** Get the next name in '*path' = 'name1;name2;name3;...', changing
** the ending ';' to '\0' to create a zero-terminated string. Return
** NULL when list ends.
*/
static const char *getnextfilename (char **path, char *end) {
  char *sep;
  char *name = *path;
  if (name == end)
    return NULL;  /* no more names */
  else if (*name == '\0') {  /* from previous iteration? */
    *name = *LAU_PATH_SEP;  /* restore separator */
    name++;  /* skip it */
  }
  sep = strchr(name, *LAU_PATH_SEP);  /* find next separator */
  if (sep == NULL)  /* separator not found? */
    sep = end;  /* name goes until the end */
  *sep = '\0';  /* finish file name */
  *path = sep;  /* will start next search from here */
  return name;
}


/*
** Given a path such as ";blabla.so;blublu.so", pushes the string
**
** no file 'blabla.so'
**	no file 'blublu.so'
*/
static void pusherrornotfound (lau_State *L, const char *path) {
  lauL_Buffer b;
  lauL_buffinit(L, &b);
  lauL_addstring(&b, "no file '");
  lauL_addgsub(&b, path, LAU_PATH_SEP, "'\n\tno file '");
  lauL_addstring(&b, "'");
  lauL_pushresult(&b);
}


static const char *searchpath (lau_State *L, const char *name,
                                             const char *path,
                                             const char *sep,
                                             const char *dirsep) {
  lauL_Buffer buff;
  char *pathname;  /* path with name inserted */
  char *endpathname;  /* its end */
  const char *filename;
  /* separator is non-empty and appears in 'name'? */
  if (*sep != '\0' && strchr(name, *sep) != NULL)
    name = lauL_gsub(L, name, sep, dirsep);  /* replace it by 'dirsep' */
  lauL_buffinit(L, &buff);
  /* add path to the buffer, replacing marks ('?') with the file name */
  lauL_addgsub(&buff, path, LAU_PATH_MARK, name);
  lauL_addchar(&buff, '\0');
  pathname = lauL_buffaddr(&buff);  /* writable list of file names */
  endpathname = pathname + lauL_bufflen(&buff) - 1;
  while ((filename = getnextfilename(&pathname, endpathname)) != NULL) {
    if (readable(filename))  /* does file exist and is readable? */
      return lau_pushstring(L, filename);  /* save and return name */
  }
  lauL_pushresult(&buff);  /* push path to create error message */
  pusherrornotfound(L, lau_tostring(L, -1));  /* create error message */
  return NULL;  /* not found */
}


static int ll_searchpath (lau_State *L) {
  const char *f = searchpath(L, lauL_checkstring(L, 1),
                                lauL_checkstring(L, 2),
                                lauL_optstring(L, 3, "."),
                                lauL_optstring(L, 4, LAU_DIRSEP));
  if (f != NULL) return 1;
  else {  /* error message is on top of the stack */
    lauL_pushfail(L);
    lau_insert(L, -2);
    return 2;  /* return fail + error message */
  }
}


static const char *findfile (lau_State *L, const char *name,
                                           const char *pname,
                                           const char *dirsep) {
  const char *path;
  lau_getfield(L, lau_upvalueindex(1), pname);
  path = lau_tostring(L, -1);
  if (l_unlikely(path == NULL))
    lauL_error(L, "'package.%s' must be a string", pname);
  return searchpath(L, name, path, ".", dirsep);
}


static int checkload (lau_State *L, int stat, const char *filename) {
  if (l_likely(stat)) {  /* module loaded successfully? */
    lau_pushstring(L, filename);  /* will be 2nd argument to module */
    return 2;  /* return open function and file name */
  }
  else
    return lauL_error(L, "error loading module '%s' from file '%s':\n\t%s",
                          lau_tostring(L, 1), filename, lau_tostring(L, -1));
}


static int searcher_Lau (lau_State *L) {
  const char *filename;
  const char *name = lauL_checkstring(L, 1);
  filename = findfile(L, name, "path", LAU_LSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (lauL_loadfilex(L, filename, "bt") == LAU_OK), filename);
}


/*
** Try to find a load function for module 'modname' at file 'filename'.
** First, change '.' to '_' in 'modname'; then, if 'modname' has
** the form X-Y (that is, it has an "ignore mark"), build a function
** name "lauopen_X" and look for it. (For compatibility, if that
** fails, it also tries "lauopen_Y".) If there is no ignore mark,
** look for a function named "lauopen_modname".
*/
static int loadfunc (lau_State *L, const char *filename, const char *modname) {
  const char *openfunc;
  const char *mark;
  modname = lauL_gsub(L, modname, ".", LAU_OFSEP);
  mark = strchr(modname, *LAU_IGMARK);
  if (mark) {
    int stat;
    openfunc = lau_pushlstring(L, modname, ct_diff2sz(mark - modname));
    openfunc = lau_pushfstring(L, LAU_POF"%s", openfunc);
    stat = lookforfunc(L, filename, openfunc);
    if (stat != ERRFUNC) return stat;
    modname = mark + 1;  /* else go ahead and try old-style name */
  }
  openfunc = lau_pushfstring(L, LAU_POF"%s", modname);
  return lookforfunc(L, filename, openfunc);
}


static int searcher_C (lau_State *L) {
  const char *name = lauL_checkstring(L, 1);
  const char *filename = findfile(L, name, "cpath", LAU_CSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (loadfunc(L, filename, name) == 0), filename);
}


static int searcher_Croot (lau_State *L) {
  const char *filename;
  const char *name = lauL_checkstring(L, 1);
  const char *p = strchr(name, '.');
  int stat;
  if (p == NULL) return 0;  /* is root */
  lau_pushlstring(L, name, ct_diff2sz(p - name));
  filename = findfile(L, lau_tostring(L, -1), "cpath", LAU_CSUBSEP);
  if (filename == NULL) return 1;  /* root not found */
  if ((stat = loadfunc(L, filename, name)) != 0) {
    if (stat != ERRFUNC)
      return checkload(L, 0, filename);  /* real error */
    else {  /* open function not found */
      lau_pushfstring(L, "no module '%s' in file '%s'", name, filename);
      return 1;
    }
  }
  lau_pushstring(L, filename);  /* will be 2nd argument to module */
  return 2;
}


static int searcher_preload (lau_State *L) {
  const char *name = lauL_checkstring(L, 1);
  lau_getfield(L, LAU_REGISTRYINDEX, LAU_PRELOAD_TABLE);
  if (lau_getfield(L, -1, name) == LAU_TNIL) {  /* not found? */
    lau_pushfstring(L, "no field package.preload['%s']", name);
    return 1;
  }
  else {
    lau_pushliteral(L, ":preload:");
    return 2;
  }
}


static void findloader (lau_State *L, const char *name) {
  int i;
  lauL_Buffer msg;  /* to build error message */
  /* push 'package.searchers' to index 3 in the stack */
  if (l_unlikely(lau_getfield(L, lau_upvalueindex(1), "searchers")
                 != LAU_TTABLE))
    lauL_error(L, "'package.searchers' must be a table");
  lauL_buffinit(L, &msg);
  lauL_addstring(&msg, "\n\t");  /* error-message prefix for first message */
  /*  iterate over available searchers to find a loader */
  for (i = 1; ; i++) {
    if (l_unlikely(lau_rawgeti(L, 3, i) == LAU_TNIL)) {  /* no more searchers? */
      lau_pop(L, 1);  /* remove nil */
      lauL_buffsub(&msg, 2);  /* remove last prefix */
      lauL_pushresult(&msg);  /* create error message */
      lauL_error(L, "module '%s' not found:%s", name, lau_tostring(L, -1));
    }
    lau_pushstring(L, name);
    lau_call(L, 1, 2);  /* call it */
    if (lau_isfunction(L, -2))  /* did it find a loader? */
      return;  /* module loader found */
    else if (lau_isstring(L, -2)) {  /* searcher returned error message? */
      lau_pop(L, 1);  /* remove extra return */
      lauL_addvalue(&msg);  /* concatenate error message */
      lauL_addstring(&msg, "\n\t");  /* prefix for next message */
    }
    else  /* no error message */
      lau_pop(L, 2);  /* remove both returns */
  }
}


static int ll_require (lau_State *L) {
  const char *name = lauL_checkstring(L, 1);
  lau_settop(L, 1);  /* LOADED table will be at index 2 */
  lau_getfield(L, LAU_REGISTRYINDEX, LAU_LOADED_TABLE);
  lau_getfield(L, 2, name);  /* LOADED[name] */
  if (lau_toboolean(L, -1))  /* is it there? */
    return 1;  /* package is already loaded */
  /* else must load package */
  lau_pop(L, 1);  /* remove 'getfield' result */
  findloader(L, name);
  lau_rotate(L, -2, 1);  /* function <-> loader data */
  lau_pushvalue(L, 1);  /* name is 1st argument to module loader */
  lau_pushvalue(L, -3);  /* loader data is 2nd argument */
  /* stack: ...; loader data; loader function; mod. name; loader data */
  lau_call(L, 2, 1);  /* run loader to load module */
  /* stack: ...; loader data; result from loader */
  if (!lau_isnil(L, -1))  /* non-nil return? */
    lau_setfield(L, 2, name);  /* LOADED[name] = returned value */
  else
    lau_pop(L, 1);  /* pop nil */
  if (lau_getfield(L, 2, name) == LAU_TNIL) {   /* module set no value? */
    lau_pushboolean(L, 1);  /* use true as result */
    lau_copy(L, -1, -2);  /* replace loader result */
    lau_setfield(L, 2, name);  /* LOADED[name] = true */
  }
  lau_rotate(L, -2, 1);  /* loader data <-> module result  */
  return 2;  /* return module result and loader data */
}

/* }====================================================== */




static const lauL_Reg pk_funcs[] = {
  {"loadlib", ll_loadlib},
  {"searchpath", ll_searchpath},
  /* placeholders */
  {"preload", NULL},
  {"cpath", NULL},
  {"path", NULL},
  {"searchers", NULL},
  {"loaded", NULL},
  {NULL, NULL}
};


static const lauL_Reg ll_funcs[] = {
  {"require", ll_require},
  {NULL, NULL}
};


static void createsearcherstable (lau_State *L) {
  static const lau_CFunction searchers[] = {
    searcher_preload,
    searcher_Lau,
    searcher_C,
    searcher_Croot,
    NULL
  };
  int i;
  /* create 'searchers' table */
  lau_createtable(L, sizeof(searchers)/sizeof(searchers[0]) - 1, 0);
  /* fill it with predefined searchers */
  for (i=0; searchers[i] != NULL; i++) {
    lau_pushvalue(L, -2);  /* set 'package' as upvalue for all searchers */
    lau_pushcclosure(L, searchers[i], 1);
    lau_rawseti(L, -2, i+1);
  }
  lau_setfield(L, -2, "searchers");  /* put it in field 'searchers' */
}


LAUMOD_API int lauopen_package (lau_State *L) {
  lauL_getsubtable(L, LAU_REGISTRYINDEX, CLIBS);  /* create CLIBS table */
  lau_pop(L, 1);  /* will not use it now */
  lauL_newlib(L, pk_funcs);  /* create 'package' table */
  createsearcherstable(L);
  /* set paths */
  setpath(L, "path", LAU_PATH_VAR, LAU_PATH_DEFAULT);
  setpath(L, "cpath", LAU_CPATH_VAR, LAU_CPATH_DEFAULT);
  /* store config information */
  lau_pushliteral(L, LAU_DIRSEP "\n" LAU_PATH_SEP "\n" LAU_PATH_MARK "\n"
                     LAU_EXEC_DIR "\n" LAU_IGMARK "\n");
  lau_setfield(L, -2, "config");
  /* set field 'loaded' */
  lauL_getsubtable(L, LAU_REGISTRYINDEX, LAU_LOADED_TABLE);
  lau_setfield(L, -2, "loaded");
  /* set field 'preload' */
  lauL_getsubtable(L, LAU_REGISTRYINDEX, LAU_PRELOAD_TABLE);
  lau_setfield(L, -2, "preload");
  lau_pushglobaltable(L);
  lau_pushvalue(L, -2);  /* set 'package' as upvalue for next lib */
  lauL_setfuncs(L, ll_funcs, 1);  /* open lib into global table */
  lau_pop(L, 1);  /* pop global table */
  return 1;  /* return 'package' table */
}

