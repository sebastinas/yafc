dnl YAFC_CHECK_READLINE    -*- autoconf -*-
dnl
dnl Checks for GNU Readline library version >= 2.0
dnl No arguments, adds appropriate libraries to LIBS,
dnl and defines HAVE_LIBREADLINE if found.
dnl
dnl sets $yafc_got_readline to a string saying what version of readline
dnl was found, if any
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

AC_DEFUN(YAFC_CHECK_READLINE,
[
  AC_SEARCH_LIBS(tgetent, [ncurses curses termcap], [])

  AC_MSG_CHECKING([for readline >= 2.0])

  AC_CACHE_VAL(yafc_cv_lib_readline,
  [
    save_LIBS="$LIBS"
    LIBS="-lreadline $LIBS"
    AC_LINK_IFELSE(
      [ AC_LANG_PROGRAM([[ int rl_function_of_keyseq(int);  ]],
                        [[ rl_function_of_keyseq(0); ]]) ],
      yafc_cv_lib_readline=200,
      yafc_cv_lib_readline=no)
    LIBS="$save_LIBS"

    if test "$yafc_cv_lib_readline" = "200"; then
      save_LIBS="$LIBS"
      LIBS="-lreadline $LIBS"
      AC_LINK_IFELSE(
        [ AC_LANG_PROGRAM([[ extern int rl_completion_append_character; ]],
                          [[ rl_completion_append_character=0; ]]) ],
        yafc_cv_lib_readline=210)
        LIBS="$save_LIBS"
    fi

    if test "$yafc_cv_lib_readline" = "210"; then
      save_LIBS="$LIBS"
      LIBS="-lreadline $LIBS"
      AC_LINK_IFELSE(
        [ AC_LANG_PROGRAM([[ void rl_completion_matches(); ]],
                          [[ rl_completion_matches(); ]]) ],
        yafc_cv_lib_readline=420)
      LIBS="$save_LIBS"
    fi
  ])

  if test "$yafc_cv_lib_readline" != "no"; then
    if test "$yafc_cv_lib_readline" = "420"; then
      AC_MSG_RESULT(version 4.2 or higher)
      yafc_got_readline="yes, version 4.2 or higher"
    elif test "$yafc_cv_lib_readline" = "210"; then
      AC_MSG_RESULT(version 2.1 or higher)
      yafc_got_readline="yes, version 2.1 or higher"
    else
      AC_MSG_RESULT(version 2.0)
      yafc_got_readline="yes, version 2.0"
    fi
    LIBS="-lreadline $LIBS"
    AC_DEFINE_UNQUOTED([HAVE_LIBREADLINE], $yafc_cv_lib_readline,
                       [define to the version of readline you got])
    AC_CHECK_HEADERS(readline/readline.h)
    AC_CHECK_HEADERS(readline/history.h)
  else
    AC_MSG_RESULT(no)
    yafc_got_readline="no"
  fi
])
