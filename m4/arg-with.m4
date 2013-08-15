dnl YAFC_ARG_WITH(package_name, default?, long_name)    -*- autoconf -*-
dnl
dnl defines yafc_with_$1 to yes or no and sets CPPFLAGS and/or LDFLAGS if given
dnl also sets yafc_with_$1_{lib|inc} to appropriate values (or empty)

AC_DEFUN([YAFC_ARG_WITH],
[
  yafc_with_$1=$2

  AC_ARG_WITH($1,
    AS_HELP_STRING([--with-$1=DIR], [use $3 in DIR]),
    [ # action-if-given
      yafc_with_$1="yes"
      if test -d "$withval"; then
        CPPFLAGS="$CPPFLAGS -I$withval/include"
        LDFLAGS="$LDFLAGS -L$withval/lib"
        yafc_with_$1_inc=$withval/include
        yafc_with_$1_lib=$withval/lib
      elif test "$withval" = "no"; then
        yafc_with_$1="no"
      else
        AC_MSG_WARN([argument to --with-$1 not a directory -- ignored])
      fi
    ])

  AC_ARG_WITH($1-lib,
    AS_HELP_STRING([--with-$1-lib=DIR], [use $3 libraries in DIR]),
    [ # action-if-given
      yafc_with_$1="yes"
      if test -d "$withval"; then
        LDFLAGS="$LDFLAGS -L$withval"
        yafc_with_$1_lib=$withval
      elif test "$withval" = "no"; then
        yafc_with_$1="no"
      else
        AC_MSG_WARN([argument to --with-$1 not a directory -- ignored])
      fi
    ])

  AC_ARG_WITH($1-include,
    AS_HELP_STRING([--with-$1-include=DIR], [use $3 header files in DIR]),
    [ # action-if-given
      yafc_with_$1="yes"
      if test -d "$withval"; then
        CPPFLAGS="$CPPFLAGS -I$withval"
        yafc_with_$1_inc=$withval
      elif test "$withval" = "no"; then
        yafc_with_$1="no"
      else
        AC_MSG_WARN([argument to --with-$1 not a directory -- ignored])
      fi
    ])
])
