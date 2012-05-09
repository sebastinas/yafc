dnl -*- autoconf -*- macros that checks for Kerberos
dnl
dnl Based on check-kerberos.m4 from the Arla distribution
dnl
dnl checks for kerberos 5 + gssapi
dnl checks for kerberos 4 or Kerberos 5 w/ Kerberos 4 compatibility
dnl
dnl sets yafc_found_krb5 if found krb5 (libs + headers + gssapi)
dnl sets yafc_found_krb5_lib_libs to required LIBS
dnl sets yafc_found_krb5_lib_flags to required LDFLAGS
dnl sets yafc_found_krb5_inc_flags to required CPPFLAGS
dnl
dnl sets yafc_found_krb4 if found krb4 (libs + headers)
dnl sets yafc_found_krb4_lib if found krb4 libs
dnl sets yafc_found_krb4_inc if found krb4 headers
dnl sets yafc_found_krb4_lib_dir to directory of krb4 libs
dnl sets yafc_found_krb4_inc_dir to directory of krb4 headers
dnl
dnl sets yafc_found_krb4_lib_libs to required LIBS
dnl sets yafc_found_krb4_lib_flags to required LDFLAGS
dnl sets yafc_found_krb4_inc_flags to required CPPFLAGS

AC_DEFUN([YAFC_KERBEROS],
[
  YAFC_ARG_WITH(krb4, yes, [Kerberos 4])
  YAFC_KRB5_CHECK

  if test "$yafc_with_krb4" = "yes"; then
    if test -z "$yafc_with_krb4_lib"; then
      YAFC_SEARCH_KRB4_LIBS(["" \
                             /usr/athena/lib \
                             /usr/kerberos/lib \
                             /usr/lib \
                             /usr/local/lib])
    else
      YAFC_SEARCH_KRB4_LIBS([$yafc_with_krb4_lib])
    fi

    if test "$yafc_found_krb4_lib" = "yes"; then
      if test -z "$yafc_with_krb4_inc"; then
        YAFC_SEARCH_KRB4_HEADERS(["" \
                                  /usr/include/kerberos \
                                  /usr/include/kerberosIV \
                                  /usr/athena/include \
                                  /usr/kerberos/include \
                                  /usr/kerberos/include/kerberosIV \
                                  /usr/local/include \
                                  /usr/local/include/kerberos \
                                  /usr/local/include/kerberosIV ])
      else
        YAFC_SEARCH_KRB4_HEADERS([$yafc_with_krb4_inc])
      fi
      if test "$yafc_found_krb4_inc" = "yes"; then
        yafc_found_krb4="yes"
      fi
    fi

    if test "$yafc_found_krb4" = "yes"; then
      YAFC_CHECK_KRB_GET_OUR_IP_FOR_REALM

      AC_MSG_CHECKING([for afs_string_to_key])
      saved_LIBS=$LIBS
      saved_LDFLAGS=$LDFLAGS
      LIBS="$yafc_found_krb4_lib_libs $saved_LIBS"
      LDFLAGS="$yafc_found_krb4_lib_flags $saved_LDFLAGS"
      AC_LINK_IFELSE(
      [
        AC_LANG_PROGRAM([[ void afs_string_to_key();  ]],
                        [[ afs_string_to_key(); ]])
      ], [ # action-if-found
        AC_MSG_RESULT([yes])
        yafc_found_krb4_lib_libs="$yafc_found_krb4_lib_libs $libs"
        AC_DEFINE([HAVE_AFS_STRING_TO_KEY], [1],
                  [define if you have the afs_string_to_key function])
        break
      ], [ # action-if-not-found
        AC_MSG_RESULT([no])
      ])
      LIBS=$saved_LIBS
      LDFLAGS=$saved_LDFLAGS
    fi
  fi

  if test "$yafc_found_krb4" = "yes"; then
    AC_DEFINE([HAVE_KRB4], [1], [define if you have Kerberos 4])
  fi

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
      AC_PATH_PROG([KRB5CONFIG], [krb5-config],"no",[$KRB5ROOT/bin$PATH_SEPARATOR$PATH])
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

  if test "x$yafc_found_krb5" = "xyes" ; then
    AC_DEFINE([HAVE_KRB5], [1], [define if you have Kerberos 5])
    if test "x$yafc_krb5_vendor" = "xMIT" ; then
      AC_DEFINE([HAVE_KRB5_MIT], [1], [define if you have Kerberos 5 - MIT])
    elif test "x$yafc_krb5_vendor" = "xHeimdal" ; then
      AC_DEFINE([HAVE_KRB5_HEIMDAL], [1], [define if you have Kerberos 5 - Heimdal])
    fi
    yafc_found_krb5_inc_flags="`$KRB5CONFIG --cflags krb5 gssapi`"
    yafc_found_krb5_lib_libs="`$KRB5CONFIG --libs krb5 gssapi | $AWK '{for(i=1;i<=NF;i++){ if ($i ~ \"^-l.*\"){ printf \"%s \", $i }}}'`"
    yafc_found_krb5_lib_flags="`$KRB5CONFIG --libs krb5 gssapi | $AWK '{for(i=1;i<=NF;i++){ if ($i !~ \"^-l.*\"){ printf \"%s \", $i }}}'`"

    AC_MSG_CHECKING([for krb5 CFLAGS])
    AC_MSG_RESULT([$yafc_found_krb5_inc_flags])
    AC_MSG_CHECKING([for krb5 LDFLAGS])
    AC_MSG_RESULT([$yafc_found_krb5_lib_flags])
    AC_MSG_CHECKING([for krb5 LIBS])
    AC_MSG_RESULT([$yafc_found_krb5_lib_libs])

    old_CFLAGS="$CFLAGS"
    old_LDFLAGS="$LDFLAGS"
    old_LIBS="$LIBS"
    CFLAGS="$CFLAGS $yafc_found_krb5_inc_flags"
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
  fi
])


dnl *************************************************************
dnl Kerberos 4


AC_DEFUN([YAFC_SEARCH_KRB4_LIBS_IN],
[
  if test -z "$1"; then
    AC_MSG_CHECKING([for Kerberos 4])
  else
    AC_MSG_CHECKING([for Kerberos 4 in $1])
  fi
  saved_LIBS=$LIBS
  saved_LDFLAGS=$LDFLAGS

  for xlibs in                 dnl
    "-ldes"                    dnl kth-krb && mit-krb
    "-ldes -lroken"            dnl kth-krb in nbsd
    "-ldes -lroken -lcom_err"  dnl kth-krb in nbsd for real
    "-ldes -lcom_err"          dnl kth-krb in fbsd
    "-ldes -lcom_err -lcrypt"  dnl CNS q96 à la SCS
    "-lcrypto"                 dnl kth-krb with openssl
  ; do

    LIBS="-lkrb $xlibs $saved_LIBS"
    if test -n "$1"; then
      LDFLAGS="-L$1 $saved_LDFLAGS"
    fi

    AC_LINK_IFELSE(
    [
      AC_LANG_PROGRAM([[ void dest_tkt();  ]],
                      [[ dest_tkt(); ]])
    ], [ # action-if-found
      yafc_found_krb4_lib="yes"
      yafc_found_krb4_lib_dir=$1
      yafc_found_krb4_lib_libs="-lkrb $xlibs"
      if test -n "$1"; then
        yafc_found_krb4_lib_flags="-L$1"
      fi
      break
    ], [ # action-if-not-found
      yafc_found_krb4_lib="no"
    ])
  done

  # check for Kerberos 5 with Kerberos 4 compatibility
  if test "$yafc_found_krb4_lib" = "no"; then
    for xlibs in "-lkrb4 -ldes425 -lkrb5 -lcom_err $yafc_krb5_lib_libs"  dnl
                 "-lkrb4 -ldes524 -lkrb5 -lcom_err $yafc_krb5_lib_libs"  dnl
    ; do

      LIBS="$xlibs $saved_LIBS"
      if test -n "$1"; then
        LDFLAGS="-L$1 $saved_LDFLAGS"
      fi

      AC_LINK_IFELSE(
      [
        AC_LANG_PROGRAM([[ void dest_tkt();  ]],
                        [[ dest_tkt(); ]])
      ], [ # action-if-found
        yafc_found_krb4_lib="yes"
        yafc_found_krb4_lib_dir=$1
        yafc_found_krb4_lib_libs="$xlibs"
        if test -n "$1"; then
          yafc_found_krb4_lib_flags="-L$1"
        fi
        break
      ], [ # action-if-not-found
        yafc_found_krb4_lib="no"
      ])
    done
  fi

  AC_MSG_RESULT([$yafc_found_krb4_lib])

  LIBS=$saved_LIBS
  LDFLAGS=$saved_LDFLAGS
])

AC_DEFUN([YAFC_SEARCH_KRB4_LIBS],
[
  for p in $1; do
    if test -z "$p" -o -d "$p"; then
      YAFC_SEARCH_KRB4_LIBS_IN($p)
      if test "$yafc_found_krb4_lib" = "yes"; then
        break
      fi
    fi
  done
])

AC_DEFUN([YAFC_SEARCH_KRB4_HEADERS],
[
  for p in $1; do
    if test -z "$p" -o -d "$p"; then
      YAFC_SEARCH_HEADER(krb4, [Kerberos 4], [krb.h], $p,
        [struct ktext foo;])
      if test "$yafc_found_krb4_inc" = "yes"; then
        break
      fi
    fi
  done
])

AC_DEFUN([YAFC_CHECK_KRB_GET_OUR_IP_FOR_REALM],
[
  AC_MSG_CHECKING([for krb_get_our_ip_for_realm])

  saved_LIBS=$LIBS
  saved_LDFLAGS=$LDFLAGS
  LDFLAGS="$yafc_found_krb4_lib_flags $LDFLAGS"

  for xlibs in "" "-lresolv" ; do
    LIBS="$yafc_found_krb4_lib_libs $xlibs $saved_LIBS"

    AC_LINK_IFELSE(
    [
      AC_LANG_PROGRAM([[ void krb_get_our_ip_for_realm();  ]],
                      [[ krb_get_our_ip_for_realm(); ]])
    ], [ # action-if-found
      yafc_found_krb_get_our_ip_for_realm="yes"
      yafc_found_krb4_lib_libs="$yafc_found_krb4_lib_libs $xlibs $libs"
      AC_DEFINE([HAVE_KRB_GET_OUR_IP_FOR_REALM], [],
                [define if you have the krb_get_our_ip_for_realm function])
      break
    ], [ # action-if-not-found
      yafc_found_krb_get_our_ip_for_realm="no"
    ])
  done

  AC_MSG_RESULT([$yafc_found_krb_get_our_ip_for_realm])

  LIBS=$saved_LIBS
  LDFLAGS=$saved_LDFLAGS
])

dnl YAFC_SEARCH_HEADER_IN(name, desc, hdr, path, body)
AC_DEFUN([YAFC_SEARCH_HEADER_IN],
[
  AC_COMPILE_IFELSE(
  [
    AC_LANG_PROGRAM([#include <$3>], [$5])
  ], [ # action-if-found
    yafc_found_$1_inc="yes"
    yafc_found_$1_inc_dir=$4
    if test -n "$4"; then
      yafc_found_$1_inc_flags="-I$4"
    fi
    AC_DEFINE_UNQUOTED(AS_TR_CPP(HAVE_$3), [1], [define to 1 if you have the $3 header file])
  ], [ # action-if-not-found
    yafc_found_$1_inc="no"
  ])
])

dnl YAFC_SEARCH_HEADER(name, desc, hdr, path, body)
AC_DEFUN([YAFC_SEARCH_HEADER],
[
  saved_LIBS=$LIBS
  saved_LDFLAGS=$LDFLAGS
  saved_CPPFLAGS=$CPPFLAGS

  if test -z "$4"; then
    AC_MSG_CHECKING([for $2 headers])
  else
    AC_MSG_CHECKING([for $2 headers in $4])
    CPPFLAGS="-I$4 $saved_CPPFLAGS"
  fi

  YAFC_SEARCH_HEADER_IN($1, $2, $3, $4, $5)

  AC_MSG_RESULT([$yafc_found_$1_inc])

  LIBS=$saved_LIBS
  LDFLAGS=$saved_LDFLAGS
  CPPFLAGS=$saved_CPPFLAGS
])
