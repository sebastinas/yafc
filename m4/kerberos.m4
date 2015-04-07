dnl -*- autoconf -*- macros that checks for Kerberos
dnl
dnl Based on check-kerberos.m4 from the Arla distribution
dnl
dnl checks for kerberos 5 + gssapi
dnl
dnl sets yafc_found_krb5 if found krb5 (libs + headers + gssapi)
dnl sets yafc_found_krb5_lib_libs to required LIBS
dnl sets yafc_found_krb5_lib_flags to required LDFLAGS
dnl sets yafc_found_krb5_inc_flags to required CPPFLAGS

AC_DEFUN([YAFC_KERBEROS],
[
  YAFC_KRB5_CHECK

  if test "$yafc_found_krb5" = "yes" -o "$yafc_found_krb4" = "yes"; then
    yafc_found_krb="yes";
    AC_DEFINE([HAVE_KERBEROS], [1], [define if you have Kerberos 4 or 5])
  fi
])


dnl *************************************************************
dnl Kerberos 5

AC_DEFUN([YAFC_KRB5_CHECK],
[
  AC_REQUIRE([AC_PROG_GREP])
  AC_REQUIRE([AC_PROG_AWK])

  AC_ARG_WITH([krb5],
	  [AS_HELP_STRING([--with-krb5=@<:@=DIR@:>@], [use Kerberos 5 in @<:@=DIR@:>@])],
	  [],
    [with_krb5=yes])

  yafc_found_krb5="no, disabled"
  AS_IF([test "x$with_krb5" != "xno"],
    [
      if test "x$withval" = "xyes" || test "x$withval" = "x"; then
        KRB5ROOT="/usr/local"
      else
        KRB5ROOT=${withval}
      fi
      AC_PATH_PROGS([KRB5CONFIG], [krb5-config krb5-config.mit krb5-config.heimdal],"no",[$KRB5ROOT/bin$PATH_SEPARATOR$PATH])
      if test "x$KRB5CONFIG" = "xno" ; then
        yafc_found_krb5="no, not found"
      else
        AC_MSG_CHECKING([for krb5 vendor])
        yafc_krb5_vendor="`$KRB5CONFIG --vendor 2>/dev/null || echo unknown`"
        if test "x$yafc_krb5_vendor" = "xMassachusetts Institute of Technology"; then
          yafc_krb5_vendor="MIT"
        elif test "x$yafc_krb5_vendor" != "xHeimdal"; then
          yafc_krb5_vendor="unknown"
        fi

        AC_MSG_RESULT([$yafc_krb5_vendor])

        AC_MSG_CHECKING([for GSSAPI])
        if $KRB5CONFIG | $GREP gssapi > /dev/null; then
          AC_MSG_RESULT([yes])
          yafc_found_krb5="yes"
        else
          AC_MSG_RESULT([no])
          yafc_found_krb5="no, gssapi not found"
        fi
      fi
    ])

  if test "x$with_krb5" != "xno" ; then
    AC_ARG_WITH([krb5-cflags],
      [AS_HELP_STRING([--with-krb5-cflags], [CFLAGS to use Kerberos 5 - overrides flags found with krb5-config])],
      [
        yafc_found_krb5_inc_flags="$with_krb5_cflags"
        yafc_found_krb5="yes"
      ])

    AC_ARG_WITH([krb5-ldflags],
      [AS_HELP_STRING([--with-krb5-ldflags], [LDFLAGS to use Kerberos 5 - overrides flags found with krb5-config])],
      [
        yafc_found_krb5_lib_flags="$with_krb5_ldflags"
        yafc_found_krb5="yes"
      ])

    AC_ARG_WITH([krb5-libs],
      [AS_HELP_STRING([--with-krb5-libs], [LIBS to use Kerberos 5 - overrides flags found with krb5-config])],
      [
        yafc_found_krb5_lib_libs="$withval"
        yafc_found_krb5="yes"
      ])


    AC_ARG_WITH([krb5-vendor],
      [AS_HELP_STRING([--with-krb5-vendor], [Kerberos 5 vendor: MIT or Heimdal - overrides flags found with krb5-config])],
      [
        yafc_krb5_vendor="$withval"
        yafc_found_krb5="yes"
      ])
  fi

  if test "x$yafc_found_krb5" = "xyes" ; then
    AC_DEFINE([HAVE_KRB5], [1], [define if you have Kerberos 5])
    if test "x$yafc_krb5_vendor" = "xMIT" ; then
      AC_DEFINE([HAVE_KRB5_MIT], [1], [define if you have Kerberos 5 - MIT])
    elif test "x$yafc_krb5_vendor" = "xHeimdal" ; then
      AC_DEFINE([HAVE_KRB5_HEIMDAL], [1], [define if you have Kerberos 5 - Heimdal])
    fi
    dnl check krb5-config for CFLAGS, LDFLAGS, LIBS if not supplied by the user
    if test "x$KRB5CONFIG" != "x" && test "x$KRB5CONFIG" != "xno" ; then
      if test "x$yafc_found_krb5_inc_flags" = "x"; then
        yafc_found_krb5_inc_flags="`$KRB5CONFIG --cflags krb5 gssapi`"
      fi
      if test "x$yafc_found_krb5_lib_libs" = "x" ; then
        yafc_found_krb5_lib_libs="`$KRB5CONFIG --libs krb5 gssapi | $AWK '{for(i=1;i<=NF;i++){ if ($i ~ \"^-l.*\"){ printf \"%s \", $i }}}'`"
      fi
      if test "x$yafc_found_krb5_lib_flags" = "x" ; then
        yafc_found_krb5_lib_flags="`$KRB5CONFIG --libs krb5 gssapi | $AWK '{for(i=1;i<=NF;i++){ if ($i !~ \"^-l.*\"){ printf \"%s \", $i }}}'`"
      fi
    fi

    AC_MSG_CHECKING([for krb5 CFLAGS])
    AC_MSG_RESULT([$yafc_found_krb5_inc_flags])
    AC_MSG_CHECKING([for krb5 LDFLAGS])
    AC_MSG_RESULT([$yafc_found_krb5_lib_flags])
    AC_MSG_CHECKING([for krb5 LIBS])
    AC_MSG_RESULT([$yafc_found_krb5_lib_libs])

    old_CFLAGS="$CFLAGS"
    old_LDFLAGS="$LDFLAGS"
    old_LIBS="$LIBS"
    old_CPPFLAGS="$CPPFLAGS"
    CFLAGS="$CFLAGS $yafc_found_krb5_inc_flags"
    CPPFLAGS="$CPPFLAGS $yafc_found_krb5_inc_flags"
    LIBS="$LIBS $yafc_found_krb5_lib_libs"
    LDFLAGS="$LDFLAGS $yafc_found_krb5_lib_flags"
    AC_CHECK_HEADERS(krb5.h)
    AC_CHECK_HEADERS(gssapi.h,,
      [
        AC_CHECK_HEADERS(gssapi/gssapi.h,
          [
            AC_CHECK_HEADERS(gssapi/gssapi_krb5.h \
                             gssapi/krb5_err.h)
          ])
      ])
    AC_MSG_CHECKING([if krb5 is usable])
    AC_LINK_IFELSE(
      [
        AC_LANG_PROGRAM([[ void gss_init_sec_context();  ]],
                        [[ gss_init_sec_context(); ]])
      ], [ # action-if-found
        AC_MSG_RESULT([yes])
      ], [ # action-if-not-found
        AC_MSG_RESULT([no])
        yafc_found_krb5="no, not usable"
        yafc_found_krb5_inc_flags=""
        yafc_found_krb5_lib_libs=""
        yafc_found_krb5_lib_flags=""
      ])
    CFLAGS="$old_CFLAGS"
    LIBS="$old_LIBS"
    LDFLAGS="$old_LDFLAGS"
    CPPFLAGS="$old_CPPFLAGS"
  fi
])
