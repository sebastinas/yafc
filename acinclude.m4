dnl mhe_CHECK_TYPES
dnl stolen from configure.in in openssh-2.9p1

AC_DEFUN(mhe_CHECK_TYPES,
[
  dnl Checks for data types
  AC_CHECK_SIZEOF(char, 1)
  AC_CHECK_SIZEOF(short int, 2)
  AC_CHECK_SIZEOF(int, 4)
  AC_CHECK_SIZEOF(long int, 4)
  AC_CHECK_SIZEOF(long long int, 8)

  AC_CACHE_CHECK([for u_intXX_t types], ac_cv_have_u_intxx_t, [
    AC_TRY_COMPILE(
      [ #include <sys/types.h> ],
      [ u_int8_t a; u_int16_t b; u_int32_t c; a = b = c = 1;],
      [ ac_cv_have_u_intxx_t="yes" ],
      [ ac_cv_have_u_intxx_t="no" ]
    )
  ])
  if test "x$ac_cv_have_u_intxx_t" = "xyes" ; then
    AC_DEFINE(HAVE_U_INTXX_T)
    have_u_intxx_t=1
  fi

  if test -z "$have_u_intxx_t" ; then
    AC_CACHE_CHECK([for uintXX_t types], ac_cv_have_uintxx_t, [
      AC_TRY_COMPILE(
        [#include <sys/types.h>],
        [ uint8_t a; uint16_t b; uint32_t c; a = b = c = 1; ], 
        [ ac_cv_have_uintxx_t="yes" ],
        [ ac_cv_have_uintxx_t="no" ]
      )
    ])
    if test "x$ac_cv_have_uintxx_t" = "xyes" ; then
      AC_DEFINE(HAVE_UINTXX_T)
    fi
  fi
])

dnl mhe_CHECK_PROTO
dnl checks if function $1 needs a prototype
dnl Syntax: mhe_CHECK_PROTO(function, includes)
dnl defines NEED_$1_PROTO

AC_DEFUN(mhe_CHECK_PROTO,
[
  AC_MSG_CHECKING(if $1 needs a prototype)
  AC_CACHE_VAL(mhe_cv_$1_need_proto, [
    AC_TRY_COMPILE([$2], [
      struct foo { int bar; } x;
      void $1(struct foo *);
      $1(&x);],
	  eval "mhe_cv_$1_need_proto=yes",
	  eval "mhe_cv_$1_need_proto=no"
	)
  ])

  if test "$mhe_cv_$1_need_proto" = "yes"; then
    AC_MSG_RESULT(yes)
	AC_DEFINE(NEED_$1_PROTO)
  else
    AC_MSG_RESULT(no)
  fi
])

AC_DEFUN(mhe_ARG_WITH,
[
  if test "$2" = ""; then
    mhe_with_$1_default="no"
  else
    mhe_with_$1_default="yes"
  fi
	AC_ARG_WITH($1, [
  --with-$1[=DIR]            use $1 in DIR, looks for libraries
                             in DIR/lib and header files in DIR/include],
		if test "$withval" != "yes" && test "$withval" != "no" ; then
			CPPFLAGS="$CPPFLAGS -I$withval/include"
			LDFLAGS="$LDFLAGS -L$withval/lib"
		fi)

	AC_ARG_WITH($1-lib,
	[  --with-$1-lib=DIR          use $1 libraries in DIR],
		LDFLAGS="$LDFLAGS -L$withval")

	AC_ARG_WITH($1-include,
	[  --with-$1-include=DIR      use $1 header files in DIR],
		CPPFLAGS="$CPPFLAGS -I$withval")

  if test "$with_$1" = "" -a "$with_$1_lib" = "" -a "$with_$1_include" = ""; then
    mhe_with_$1=$mhe_with_$1_default
  elif test "$with_$1" = "no"; then
    mhe_with_$1="no"
  elif test "$with_$1" != "no" -o "$with_$1_lib" != "no" -o "$with_$1_include" != "no"; then
    mhe_with_$1="yes"
  fi
])

AC_DEFUN(mhe_WITH_WARN,
[
	AC_ARG_WITH(warn,
	[  --with-warn                   compile with warning options (default=no)],
	if test "$with_warn" = "yes"; then
	  AC_MSG_CHECKING(for warning options)
	  dnl add -Wall if we're using GNU CC
	  if test "$GCC" = "yes"; then
	    CFLAGS="$CFLAGS -Wall"
	    AC_MSG_RESULT(-Wall)
	  else
	    AC_MSG_RESULT(none)
	  fi
	fi)
])

dnl mhe_CHECK_READLINE
dnl
dnl Checks for GNU Readline library version >= 2.0
dnl No arguments, adds appropriate libraries to LIBS,
dnl and defines HAVE_LIBREADLINE if found.
dnl
dnl Copyright (C) 1998 Martin Hedenfalk. All rights reserved.
dnl This macro is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by the
dnl Free Software Foundation; either version 2 of the License, or (at your
dnl option) any later version.  As usual, there's no warranty at all: see
dnl the License for details.
dnl
dnl checks for rl_function_of_keyseq (appeared in 2.0)
dnl checks for rl_completion_append_character (appeared in 2.1)
dnl check for rl_completion_matches (renamed from completion_matches in 4.2)

AC_DEFUN(mhe_CHECK_READLINE,
[
    AC_SEARCH_LIBS(tgetent, ncurses curses termcap, ,
     AC_WARN(can't find terminal library))

	AC_MSG_CHECKING(for readline >= 2.0)

	AC_CACHE_VAL(mhe_cv_lib_readline,
	[
		mhe_LIBS="$LIBS"
	    LIBS="-lreadline $LIBS"
	    AC_TRY_LINK(
			[extern int rl_function_of_keyseq(int foo);],
			[rl_function_of_keyseq(0);],
			mhe_cv_lib_readline=200,
			mhe_cv_lib_readline=no
		)
		LIBS="$mhe_LIBS"

		if test "$mhe_cv_lib_readline" = "200"; then
			mhe_LIBS="$LIBS"
		    LIBS="-lreadline $LIBS"
	    	AC_TRY_LINK(
				[extern int rl_completion_append_character;],
				[rl_completion_append_character=0;],
				mhe_cv_lib_readline=210
			)
			LIBS="$mhe_LIBS"
		fi

		if test "$mhe_cv_lib_readline" = "210"; then
			mhe_LIBS="$LIBS"
		    LIBS="-lreadline $LIBS"
	    	AC_TRY_LINK(
				[extern void rl_completion_matches();],
				[rl_completion_matches();],
				mhe_cv_lib_readline=420
			)
			LIBS="$mhe_LIBS"
		fi
	])

	if test "$mhe_cv_lib_readline" != "no"; then
		if test "$mhe_cv_lib_readline" = "420"; then
		    AC_MSG_RESULT(version 4.2 or higher)
		elif test "$mhe_cv_lib_readline" = "210"; then
		    AC_MSG_RESULT(version 2.1 or higher)
		else
		    AC_MSG_RESULT(version 2.0)
		fi
	    LIBS="-lreadline $LIBS"
		AC_DEFINE_UNQUOTED(HAVE_LIBREADLINE, $mhe_cv_lib_readline)
	else
	    AC_MSG_RESULT(no)
	fi

	if test "$mhe_cv_lib_readline" != "no"; then
		AC_CHECK_HEADERS(readline/readline.h)
		AC_CHECK_HEADERS(readline/history.h)
	fi
])



dnl these are from bash...

dnl Check type of signal routines (posix, 4.2bsd, 4.1bsd or v7)
AC_DEFUN(BASH_SIGNAL_CHECK,
[AC_REQUIRE([AC_TYPE_SIGNAL])
AC_MSG_CHECKING(for type of signal functions)
AC_CACHE_VAL(bash_cv_signal_vintage,
[
  AC_TRY_LINK([#include <signal.h>],[
    sigset_t ss;
    struct sigaction sa;
    sigemptyset(&ss); sigsuspend(&ss);
    sigaction(SIGINT, &sa, (struct sigaction *) 0);
    sigprocmask(SIG_BLOCK, &ss, (sigset_t *) 0);
  ], bash_cv_signal_vintage=posix,
  [
    AC_TRY_LINK([#include <signal.h>], [
	int mask = sigmask(SIGINT);
	sigsetmask(mask); sigblock(mask); sigpause(mask);
    ], bash_cv_signal_vintage=4.2bsd,
    [
      AC_TRY_LINK([
	#include <signal.h>
	RETSIGTYPE foo() { }], [
		int mask = sigmask(SIGINT);
		sigset(SIGINT, foo); sigrelse(SIGINT);
		sighold(SIGINT); sigpause(SIGINT);
        ], bash_cv_signal_vintage=svr3, bash_cv_signal_vintage=v7
    )]
  )]
)
])
AC_MSG_RESULT($bash_cv_signal_vintage)
if test "$bash_cv_signal_vintage" = posix; then
AC_DEFINE(HAVE_POSIX_SIGNALS)
elif test "$bash_cv_signal_vintage" = "4.2bsd"; then
AC_DEFINE(HAVE_BSD_SIGNALS)
elif test "$bash_cv_signal_vintage" = svr3; then
AC_DEFINE(HAVE_USG_SIGHOLD)
fi
])





AC_DEFUN(BASH_FUNC_POSIX_SETJMP,
[AC_REQUIRE([BASH_SIGNAL_CHECK])
AC_MSG_CHECKING(for presence of POSIX-style sigsetjmp/siglongjmp)
AC_CACHE_VAL(bash_cv_func_sigsetjmp,
[AC_TRY_RUN([
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <signal.h>
#include <setjmp.h>

main()
{
#if !defined (_POSIX_VERSION) || !defined (HAVE_POSIX_SIGNALS)
exit (1);
#else

int code;
sigset_t set, oset;
sigjmp_buf xx;

/* get the mask */
sigemptyset(&set);
sigemptyset(&oset);
sigprocmask(SIG_BLOCK, (sigset_t *)NULL, &set);
sigprocmask(SIG_BLOCK, (sigset_t *)NULL, &oset);

/* save it */
code = sigsetjmp(xx, 1);
if (code)
  exit(0);	/* could get sigmask and compare to oset here. */

/* change it */
sigaddset(&set, SIGINT);
sigprocmask(SIG_BLOCK, &set, (sigset_t *)NULL);

/* and siglongjmp */
siglongjmp(xx, 10);
exit(1);
#endif
}], bash_cv_func_sigsetjmp=present, bash_cv_func_sigsetjmp=missing,
    [AC_MSG_ERROR(cannot check for sigsetjmp/siglongjmp if cross-compiling -- defaulting to missing)
     bash_cv_func_sigsetjmp=missing]
)])
AC_MSG_RESULT($bash_cv_func_sigsetjmp)
if test $bash_cv_func_sigsetjmp = present; then
AC_DEFINE(HAVE_POSIX_SIGSETJMP)
fi
])

