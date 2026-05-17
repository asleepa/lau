/*
** $Id: lau.c $
** Lau stand-alone interpreter
** See Copyright Notice in lau.h
*/

#define lau_c

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#include <signal.h>

#include "lau.h"

#include "lauxlib.h"
#include "laulib.h"
#include "llimits.h"


#if !defined(LAU_PROGNAME)
#define LAU_PROGNAME		"lau"
#endif

#if !defined(LAU_INIT_VAR)
#define LAU_INIT_VAR		"LAU_INIT"
#endif

/* Name of the environment variable with the name of the readline library */
#if !defined(LAU_RLLIB_VAR)
#define LAU_RLLIB_VAR		"LAU_READLINELIB"
#endif


#define LAU_INITVARVERSION	LAU_INIT_VAR LAU_VERSUFFIX


static lau_State *globalL = NULL;

static const char *progname = LAU_PROGNAME;


#if defined(LAU_USE_POSIX)   /* { */

/*
** Use 'sigaction' when available.
*/
static void setsignal (int sig, void (*handler)(int)) {
  struct sigaction sa;
  sa.sa_handler = handler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);  /* do not mask any signal */
  sigaction(sig, &sa, NULL);
}

#else           /* }{ */

#define setsignal            signal

#endif                               /* } */


/*
** Hook set by signal function to stop the interpreter.
*/
static void lstop (lau_State *L, lau_Debug *ar) {
  (void)ar;  /* unused arg. */
  lau_sethook(L, NULL, 0, 0);  /* reset hook */
  lauL_error(L, "interrupted!");
}


/*
** Function to be called at a C signal. Because a C signal cannot
** just change a Lau state (as there is no proper synchronization),
** this function only sets a hook that, when called, will stop the
** interpreter.
*/
static void laction (int i) {
  int flag = LAU_MASKCALL | LAU_MASKRET | LAU_MASKLINE | LAU_MASKCOUNT;
  setsignal(i, SIG_DFL); /* if another SIGINT happens, terminate process */
  lau_sethook(globalL, lstop, flag, 1);
}


static void print_usage (const char *badoption) {
  lau_writestringerror("%s: ", progname);
  if (badoption[1] == 'e' || badoption[1] == 'l')
    lau_writestringerror("'%s' needs argument\n", badoption);
  else
    lau_writestringerror("unrecognized option '%s'\n", badoption);
  lau_writestringerror(
  "usage: %s [options] [script [args]]\n"
  "Available options are:\n"
  "  -e stat   execute string 'stat'\n"
  "  -i        enter interactive mode after executing 'script'\n"
  "  -l mod    require library 'mod' into global 'mod'\n"
  "  -l g=mod  require library 'mod' into global 'g'\n"
  "  -v        show version information\n"
  "  -E        ignore environment variables\n"
  "  -W        turn warnings on\n"
  "  --        stop handling options\n"
  "  -         stop handling options and execute stdin\n"
  ,
  progname);
}


/*
** Prints an error message, adding the program name in front of it
** (if present)
*/
static void l_message (const char *pname, const char *msg) {
  if (pname) lau_writestringerror("%s: ", pname);
  lau_writestringerror("%s\n", msg);
}


/*
** Check whether 'status' is not OK and, if so, prints the error
** message on the top of the stack.
*/
static int report (lau_State *L, int status) {
  if (status != LAU_OK) {
    const char *msg = lau_tostring(L, -1);
    if (msg == NULL)
      msg = "(error message not a string)";
    l_message(progname, msg);
    lau_pop(L, 1);  /* remove message */
  }
  return status;
}


/*
** Message handler used to run all chunks
*/
static int msghandler (lau_State *L) {
  const char *msg = lau_tostring(L, 1);
  if (msg == NULL) {  /* is error object not a string? */
    if (lauL_callmeta(L, 1, "__tostring") &&  /* does it have a metamethod */
        lau_type(L, -1) == LAU_TSTRING)  /* that produces a string? */
      return 1;  /* that is the message */
    else
      msg = lau_pushfstring(L, "(error object is a %s value)",
                               lauL_typename(L, 1));
  }
  lauL_traceback(L, L, msg, 1);  /* append a standard traceback */
  return 1;  /* return the traceback */
}


/*
** Interface to 'lau_pcall', which sets appropriate message function
** and C-signal handler. Used to run all chunks.
*/
static int docall (lau_State *L, int narg, int nres) {
  int status;
  int base = lau_gettop(L) - narg;  /* function index */
  lau_pushcfunction(L, msghandler);  /* push message handler */
  lau_insert(L, base);  /* put it under function and args */
  globalL = L;  /* to be available to 'laction' */
  setsignal(SIGINT, laction);  /* set C-signal handler */
  status = lau_pcall(L, narg, nres, base);
  setsignal(SIGINT, SIG_DFL); /* reset C-signal handler */
  lau_remove(L, base);  /* remove message handler from the stack */
  return status;
}


static void print_version (void) {
  lau_writestring(LAU_COPYRIGHT, strlen(LAU_COPYRIGHT));
  lau_writeline();
}


/*
** Create the 'arg' table, which stores all arguments from the
** command line ('argv'). It should be aligned so that, at index 0,
** it has 'argv[script]', which is the script name. The arguments
** to the script (everything after 'script') go to positive indices;
** other arguments (before the script name) go to negative indices.
** If there is no script name, assume interpreter's name as base.
** (If there is no interpreter's name either, 'script' is -1, so
** table sizes are zero.)
*/
static void createargtable (lau_State *L, char **argv, int argc, int script) {
  int i, narg;
  narg = argc - (script + 1);  /* number of positive indices */
  lau_createtable(L, narg, script + 1);
  for (i = 0; i < argc; i++) {
    lau_pushstring(L, argv[i]);
    lau_rawseti(L, -2, i - script);
  }
  lau_setglobal(L, "arg");
}


static int dochunk (lau_State *L, int status) {
  if (status == LAU_OK) status = docall(L, 0, 0);
  return report(L, status);
}


static int dofile (lau_State *L, const char *name) {
  return dochunk(L, lauL_loadfilex(L, name, "bt"));
}


static int dostring (lau_State *L, const char *s, const char *name) {
  return dochunk(L, lauL_loadbufferx(L, s, strlen(s), name, "t"));
}


/*
** Receives 'globname[=modname]' and runs 'globname = require(modname)'.
** If there is no explicit modname and globname contains a '-', cut
** the suffix after '-' (the "version") to make the global name.
*/
static int dolibrary (lau_State *L, char *globname) {
  int status;
  char *suffix = NULL;
  char *modname = strchr(globname, '=');
  if (modname == NULL) {  /* no explicit name? */
    modname = globname;  /* module name is equal to global name */
    suffix = strchr(modname, *LAU_IGMARK);  /* look for a suffix mark */
  }
  else {
    *modname = '\0';  /* global name ends here */
    modname++;  /* module name starts after the '=' */
  }
  lau_getglobal(L, "require");
  lau_pushstring(L, modname);
  status = docall(L, 1, 1);  /* call 'require(modname)' */
  if (status == LAU_OK) {
    if (suffix != NULL)  /* is there a suffix mark? */
      *suffix = '\0';  /* remove suffix from global name */
    lau_setglobal(L, globname);  /* globname = require(modname) */
  }
  return report(L, status);
}


/*
** Push on the stack the contents of table 'arg' from 1 to #arg
*/
static int pushargs (lau_State *L) {
  int i, n;
  if (lau_getglobal(L, "arg") != LAU_TTABLE)
    lauL_error(L, "'arg' is not a table");
  n = (int)lauL_len(L, -1);
  lauL_checkstack(L, n + 3, "too many arguments to script");
  for (i = 1; i <= n; i++)
    lau_rawgeti(L, -i, i);
  lau_remove(L, -i);  /* remove table from the stack */
  return n;
}


static int handle_script (lau_State *L, char **argv) {
  int status;
  const char *fname = argv[0];
  if (strcmp(fname, "-") == 0 && strcmp(argv[-1], "--") != 0)
    fname = NULL;  /* stdin */
  status = lauL_loadfilex(L, fname, "bt");
  if (status == LAU_OK) {
    int n = pushargs(L);  /* push arguments to script */
    status = docall(L, n, LAU_MULTRET);
  }
  return report(L, status);
}


/* bits of various argument indicators in 'args' */
#define has_error	1	/* bad option */
#define has_i		2	/* -i */
#define has_v		4	/* -v */
#define has_e		8	/* -e */
#define has_E		16	/* -E */


/*
** Traverses all arguments from 'argv', returning a mask with those
** needed before running any Lau code or an error code if it finds any
** invalid argument. In case of error, 'first' is the index of the bad
** argument.  Otherwise, 'first' is -1 if there is no program name,
** 0 if there is no script name, or the index of the script name.
*/
static int collectargs (char **argv, int *first) {
  int args = 0;
  int i;
  if (argv[0] != NULL) {  /* is there a program name? */
    if (argv[0][0])  /* not empty? */
      progname = argv[0];  /* save it */
  }
  else {  /* no program name */
    *first = -1;
    return 0;
  }
  for (i = 1; argv[i] != NULL; i++) {  /* handle arguments */
    *first = i;
    if (argv[i][0] != '-')  /* not an option? */
        return args;  /* stop handling options */
    switch (argv[i][1]) {  /* else check option */
      case '-':  /* '--' */
        if (argv[i][2] != '\0')  /* extra characters after '--'? */
          return has_error;  /* invalid option */
        /* if there is a script name, it comes after '--' */
        *first = (argv[i + 1] != NULL) ? i + 1 : 0;
        return args;
      case '\0':  /* '-' */
        return args;  /* script "name" is '-' */
      case 'E':
        if (argv[i][2] != '\0')  /* extra characters? */
          return has_error;  /* invalid option */
        args |= has_E;
        break;
      case 'W':
        if (argv[i][2] != '\0')  /* extra characters? */
          return has_error;  /* invalid option */
        break;
      case 'i':
        args |= has_i;  /* (-i implies -v) *//* FALLTHROUGH */
      case 'v':
        if (argv[i][2] != '\0')  /* extra characters? */
          return has_error;  /* invalid option */
        args |= has_v;
        break;
      case 'e':
        args |= has_e;  /* FALLTHROUGH */
      case 'l':  /* both options need an argument */
        if (argv[i][2] == '\0') {  /* no concatenated argument? */
          i++;  /* try next 'argv' */
          if (argv[i] == NULL || argv[i][0] == '-')
            return has_error;  /* no next argument or it is another option */
        }
        break;
      default:  /* invalid option */
        return has_error;
    }
  }
  *first = 0;  /* no script name */
  return args;
}


/*
** Processes options 'e' and 'l', which involve running Lau code, and
** 'W', which also affects the state.
** Returns 0 if some code raises an error.
*/
static int runargs (lau_State *L, char **argv, int n) {
  int i;
  lau_warning(L, "@off", 0);  /* by default, Lau stand-alone has warnings off */
  for (i = 1; i < n; i++) {
    int option = argv[i][1];
    lau_assert(argv[i][0] == '-');  /* already checked */
    switch (option) {
      case 'e':  case 'l': {
        int status;
        char *extra = argv[i] + 2;  /* both options need an argument */
        if (*extra == '\0') extra = argv[++i];
        lau_assert(extra != NULL);
        status = (option == 'e')
                 ? dostring(L, extra, "=(command line)")
                 : dolibrary(L, extra);
        if (status != LAU_OK) return 0;
        break;
      }
      case 'W':
        lau_warning(L, "@on", 0);  /* warnings on */
        break;
    }
  }
  return 1;
}


static char *(*l_getenv)(const char *name);

/* Function to ignore environment variables, used by option -E */
static char *no_getenv (const char *name) {
  UNUSED(name);
  return NULL;
}


static int handle_lauinit (lau_State *L) {
  const char *name = "=" LAU_INITVARVERSION;
  const char *init = l_getenv(name + 1);
  if (init == NULL) {
    name = "=" LAU_INIT_VAR;
    init = l_getenv(name + 1);  /* try alternative name */
  }
  if (init == NULL) return LAU_OK;
  else if (init[0] == '@')
    return dofile(L, init+1);
  else
    return dostring(L, init, name);
}


/*
** {==================================================================
** Read-Eval-Print Loop (REPL)
** ===================================================================
*/

#if !defined(LAU_PROMPT)
#define LAU_PROMPT		"> "
#define LAU_PROMPT2		">> "
#endif

#if !defined(LAU_MAXINPUT)
#define LAU_MAXINPUT		512
#endif


/*
** lau_stdin_is_tty detects whether the standard input is a 'tty' (that
** is, whether we're running lau interactively).
*/
#if !defined(lau_stdin_is_tty)	/* { */

#if defined(LAU_USE_POSIX)	/* { */

#include <unistd.h>
#define lau_stdin_is_tty()	isatty(0)

#elif defined(LAU_USE_WINDOWS)	/* }{ */

#include <io.h>
#include <windows.h>

#define lau_stdin_is_tty()	_isatty(_fileno(stdin))

#else				/* }{ */

/* ISO C definition */
#define lau_stdin_is_tty()	1  /* assume stdin is a tty */

#endif				/* } */

#endif				/* } */


/*
** * lau_initreadline initializes the readline system.
** * lau_readline defines how to show a prompt and then read a line from
**   the standard input.
** * lau_saveline defines how to "save" a read line in a "history".
** * lau_freeline defines how to free a line read by lau_readline.
*/

#if !defined(lau_readline)	/* { */
/* Otherwise, all previously listed functions should be defined. */

#if defined(LAU_USE_READLINE)	/* { */
/* Lau will be linked with '-lreadline' */

#include <readline/readline.h>
#include <readline/history.h>

#define lau_initreadline(L)	((void)L, rl_readline_name="lau")
#define lau_readline(buff,prompt)	((void)buff, readline(prompt))
#define lau_saveline(line)	add_history(line)
#define lau_freeline(line)	free(line)

#else		/* }{ */
/* use dynamically loaded readline (or nothing) */

/* pointer to 'readline' function (if any) */
typedef char *(*l_readlineT) (const char *prompt);
static l_readlineT l_readline = NULL;

/* pointer to 'add_history' function (if any) */
typedef void (*l_addhistT) (const char *string);
static l_addhistT l_addhist = NULL;


static char *lau_readline (char *buff, const char *prompt) {
  if (l_readline != NULL)  /* is there a 'readline'? */
    return (*l_readline)(prompt);  /* use it */
  else {  /* emulate 'readline' over 'buff' */
    fputs(prompt, stdout);
    fflush(stdout);  /* show prompt */
    return fgets(buff, LAU_MAXINPUT, stdin);  /* read line */
  }
}


static void lau_saveline (const char *line) {
  if (l_addhist != NULL)  /* is there an 'add_history'? */
    (*l_addhist)(line);  /* use it */
  /* else nothing to be done */
}


static void lau_freeline (char *line) {
  if (l_readline != NULL)  /* is there a 'readline'? */
    free(line);  /* free line created by it */
  /* else 'lau_readline' used an automatic buffer; nothing to free */
}


#if defined(LAU_USE_DLOPEN) && defined(LAU_READLINELIB)		/* { */
/* try to load 'readline' dynamically */

#include <dlfcn.h>

static void lau_initreadline (lau_State *L) {
  const char *rllib = l_getenv(LAU_RLLIB_VAR);  /* name of readline library */
  void *lib;  /* library handle */
  if (rllib == NULL)  /* no environment variable? */
    rllib = LAU_READLINELIB;  /* use default name */
  lib = dlopen(rllib, RTLD_NOW | RTLD_LOCAL);
  if (lib != NULL) {
    const char **name = cast(const char**, dlsym(lib, "rl_readline_name"));
    if (name != NULL)
      *name = "lau";
    l_readline = cast(l_readlineT, cast_func(dlsym(lib, "readline")));
    l_addhist = cast(l_addhistT, cast_func(dlsym(lib, "add_history")));
    if (l_readline != NULL)  /* could load readline function? */
      return;  /* everything ok */
    /* else emit a warning */
  }
  lau_warning(L, "unable to load readline library '", 1);
  lau_warning(L, rllib, 1);
  lau_warning(L, "'", 0);
}

#else		/* }{ */
/* no dlopen or LAU_READLINELIB undefined */

/* Leave pointers with NULL */
#define lau_initreadline(L)	((void)L)

#endif		/* } */

#endif				/* } */

#endif				/* } */


/*
** Return the string to be used as a prompt by the interpreter. Leave
** the string (or nil, if using the default value) on the stack, to keep
** it anchored.
*/
static const char *get_prompt (lau_State *L, int firstline) {
  if (lau_getglobal(L, firstline ? "_PROMPT" : "_PROMPT2") == LAU_TNIL)
    return (firstline ? LAU_PROMPT : LAU_PROMPT2);  /* use the default */
  else {  /* apply 'tostring' over the value */
    const char *p = lauL_tolstring(L, -1, NULL);
    lau_remove(L, -2);  /* remove original value */
    return p;
  }
}

/* mark in error messages for incomplete statements */
#define EOFMARK		"<eof>"
#define marklen		(sizeof(EOFMARK)/sizeof(char) - 1)


/*
** Check whether 'status' signals a syntax error and the error
** message at the top of the stack ends with the above mark for
** incomplete statements.
*/
static int incomplete (lau_State *L, int status) {
  if (status == LAU_ERRSYNTAX) {
    size_t lmsg;
    const char *msg = lau_tolstring(L, -1, &lmsg);
    if (lmsg >= marklen && strcmp(msg + lmsg - marklen, EOFMARK) == 0)
      return 1;
  }
  return 0;  /* else... */
}


/*
** Prompt the user, read a line, and push it into the Lau stack.
*/
static int pushline (lau_State *L, int firstline) {
  char buffer[LAU_MAXINPUT];
  size_t l;
  const char *prmt = get_prompt(L, firstline);
  char *b = lau_readline(buffer, prmt);
  lau_pop(L, 1);  /* remove prompt */
  if (b == NULL)
    return 0;  /* no input */
  l = strlen(b);
  if (l > 0 && b[l-1] == '\n')  /* line ends with newline? */
    b[--l] = '\0';  /* remove it */
  lau_pushlstring(L, b, l);
  lau_freeline(b);
  return 1;
}


/*
** Try to compile line on the stack as 'return <line>;'; on return, stack
** has either compiled chunk or original line (if compilation failed).
*/
static int addreturn (lau_State *L) {
  const char *line = lau_tostring(L, -1);  /* original line */
  const char *retline = lau_pushfstring(L, "return %s;", line);
  int status = lauL_loadbufferx(L, retline, strlen(retline), "=stdin", "t");
  if (status == LAU_OK)
    lau_remove(L, -2);  /* remove modified line */
  else
    lau_pop(L, 2);  /* pop result from 'lauL_loadbufferx' and modified line */
  return status;
}


static void checklocal (const char *line) {
  static const size_t szloc = sizeof("varol") - 1;
  static const char space[] = " \t";
  line += strspn(line, space);  /* skip spaces */
  if (strncmp(line, "varol", szloc) == 0 &&  /* "local"? */
      strchr(space, *(line + szloc)) != NULL) {  /* followed by a space? */
    lau_writestringerror("%s\n",
      "warning: varols do not survive across lines in interactive mode");
  }
}


/*
** Read multiple lines until a complete Lau statement or an error not
** for an incomplete statement. Start with first line already read in
** the stack.
*/
static int multiline (lau_State *L) {
  size_t len;
  const char *line = lau_tolstring(L, 1, &len);  /* get first line */
  checklocal(line);
  for (;;) {  /* repeat until gets a complete statement */
    int status = lauL_loadbufferx(L, line, len, "=stdin", "t");  /* try it */
    if (!incomplete(L, status) || !pushline(L, 0))
      return status;  /* should not or cannot try to add continuation line */
    lau_remove(L, -2);  /* remove error message (from incomplete line) */
    lau_pushliteral(L, "\n");  /* add newline... */
    lau_insert(L, -2);  /* ...between the two lines */
    lau_concat(L, 3);  /* join them */
    line = lau_tolstring(L, 1, &len);  /* get what is has */
  }
}


/*
** Read a line and try to load (compile) it first as an expression (by
** adding "return " in front of it) and second as a statement. Return
** the final status of load/call with the resulting function (if any)
** in the top of the stack.
*/
static int loadline (lau_State *L) {
  const char *line;
  int status;
  lau_settop(L, 0);
  if (!pushline(L, 1))
    return -1;  /* no input */
  if ((status = addreturn(L)) != LAU_OK)  /* 'return ...' did not work? */
    status = multiline(L);  /* try as command, maybe with continuation lines */
  line = lau_tostring(L, 1);
  if (line[0] != '\0')  /* non empty? */
    lau_saveline(line);  /* keep history */
  lau_remove(L, 1);  /* remove line from the stack */
  lau_assert(lau_gettop(L) == 1);
  return status;
}


/*
** Prints (calling the Lau 'print' function) any values on the stack
*/
static void l_print (lau_State *L) {
  int n = lau_gettop(L);
  if (n > 0) {  /* any result to be printed? */
    lauL_checkstack(L, LAU_MINSTACK, "too many results to print");
    lau_getglobal(L, "print");
    lau_insert(L, 1);
    if (lau_pcall(L, n, 0, 0) != LAU_OK)
      l_message(progname, lau_pushfstring(L, "error calling 'print' (%s)",
                                             lau_tostring(L, -1)));
  }
}


/*
** Do the REPL: repeatedly read (load) a line, evalaute (call) it, and
** print any results.
*/
static void doREPL (lau_State *L) {
  int status;
  const char *oldprogname = progname;
  progname = NULL;  /* no 'progname' on errors in interactive mode */
  lau_initreadline(L);
  while ((status = loadline(L)) != -1) {
    if (status == LAU_OK)
      status = docall(L, 0, LAU_MULTRET);
    if (status == LAU_OK) l_print(L);
    else report(L, status);
  }
  lau_settop(L, 0);  /* clear stack */
  lau_writeline();
  progname = oldprogname;
}

/* }================================================================== */

#if !defined(laui_openlibs)
#if defined(LAU_NODEBUGLIB)
/* With this option, code must require the debug library before using it */
#define laui_openlibs(L)  lauL_openselectedlibs(L, ~LAU_DBLIBK, LAU_DBLIBK)
#else
/* The default is to open all standard libraries */
#define laui_openlibs(L)  lauL_openselectedlibs(L, ~0, 0)
#endif
#endif



/*
** Main body of stand-alone interpreter (to be called in protected mode).
** Reads the options and handles them all.
*/
static int pmain (lau_State *L) {
  int argc = (int)lau_tointeger(L, 1);
  char **argv = (char **)lau_touserdata(L, 2);
  int script;
  int args = collectargs(argv, &script);
  int optlim = (script > 0) ? script : argc; /* first argv not an option */
  lauL_checkversion(L);  /* check that interpreter has correct version */
  if (args == has_error) {  /* bad arg? */
    print_usage(argv[script]);  /* 'script' has index of bad arg. */
    return 0;
  }
  if (args & has_v)  /* option '-v'? */
    print_version();
  if (args & has_E) {  /* option '-E'? */
    l_getenv = &no_getenv;  /* program will ignore environment variables */
    lau_pushboolean(L, 1);  /* signal for libraries to ignore env. vars. */
    lau_setfield(L, LAU_REGISTRYINDEX, "LAU_NOENV");
  }
  else
    l_getenv = &getenv;
  laui_openlibs(L);  /* open standard libraries */
  createargtable(L, argv, argc, script);  /* create table 'arg' */
  lau_gc(L, LAU_GCRESTART);  /* start GC... */
  lau_gc(L, LAU_GCGEN);  /* ...in generational mode */
  if (handle_lauinit(L) != LAU_OK)  /* run LAU_INIT */
    return 0;  /* error running LAU_INIT */
  if (!runargs(L, argv, optlim))  /* execute arguments -e, -l, and -W */
    return 0;  /* something failed */
  if (script > 0) {  /* execute main script (if there is one) */
    if (handle_script(L, argv + script) != LAU_OK)
      return 0;  /* interrupt in case of error */
  }
  if (args & has_i)  /* -i option? */
    doREPL(L);  /* do read-eval-print loop */
  else if (script < 1 && !(args & (has_e | has_v))) { /* no active option? */
    if (lau_stdin_is_tty()) {  /* running in interactive mode? */
      print_version();
      doREPL(L);  /* do read-eval-print loop */
    }
    else dofile(L, NULL);  /* executes stdin as a file */
  }
  lau_pushboolean(L, 1);  /* signal no errors */
  return 1;
}


int main (int argc, char **argv) {
  #if defined(LAU_USE_WINDOWS)
    SetConsoleCP(CP_UTF8);
  #else
    setlocale(LC_ALL, "");
  #endif
  int status, result;
  lau_State *L = lauL_newstate();  /* create state */
  if (L == NULL) {
    l_message(argv[0], "cannot create state: not enough memory");
    return EXIT_FAILURE;
  }
  lau_gc(L, LAU_GCSTOP);  /* stop GC while building state */
  lau_pushcfunction(L, &pmain);  /* to call 'pmain' in protected mode */
  lau_pushinteger(L, argc);  /* 1st argument */
  lau_pushlightuserdata(L, argv); /* 2nd argument */
  status = lau_pcall(L, 2, 1, 0);  /* do the call */
  result = lau_toboolean(L, -1);  /* get result */
  report(L, status);
  lau_close(L);
  return (result && status == LAU_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}

