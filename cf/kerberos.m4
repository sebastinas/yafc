dnl -*- autoconf -*- macros that checks for Kerberos
dnl
dnl Based on check-kerberos.m4 from the Arla distribution
dnl
dnl checks for kerberos 5 + gssapi
dnl checks for kerberos 4 or Kerberos 5 w/ Kerberos 4 compatibility
dnl
dnl sets yafc_found_krb5 if found krb5 (libs + headers + gssapi)
dnl sets yafc_found_krb5_lib if found krb5 libs
dnl sets yafc_found_krb5_inc if found krb5 headers
dnl sets yafc_found_gssapi_lib if found gssapi libs
dnl sets yafc_found_gssapi_inc if found gssapi headers
dnl
dnl sets yafc_found_krb5_lib_dir to directory of krb5 libs
dnl sets yafc_found_krb5_inc_dir to directory of krb5 headers
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

AC_DEFUN(YAFC_KERBEROS,
[
  YAFC_ARG_WITH(krb4, yes, [Kerberos 4])
  YAFC_ARG_WITH(krb5, yes, [Kerberos 5])
  YAFC_ARG_WITH(gssapi, yes, [GSSAPI])

  if test "$yafc_with_krb5" = "yes"; then
    if test -z "$yafc_with_krb5_lib"; then
      YAFC_SEARCH_KRB5_LIBS(["" \
                             /usr/heimdal/lib \
                             /usr/athena/lib \
                             /usr/kerberos/lib \
                             /usr/local/lib])
    else
      YAFC_SEARCH_KRB5_LIBS([$yafc_with_krb5_lib])
    fi
   
    if test "$yafc_found_krb5_lib" = "yes"; then
      if test -z "$yafc_with_krb5_inc"; then
        YAFC_SEARCH_KRB5_HEADERS(["" \
                                  /usr/heimdal/include \
                                  /usr/kerberos/include \
                                  /usr/include/kerberosV \
                                  /usr/include/kerberos \
                                  /usr/include/krb5 \
                                  /usr/local/include \
                                  /usr/athena/include])
      else
        YAFC_SEARCH_KRB5_HEADERS([$yafc_with_krb5_inc])
      fi
      if test "$yafc_found_krb5_inc" = "yes"; then
        yafc_found_krb5="yes"
      fi
    fi

    if test "$yafc_found_krb5" = "yes"; then
      YAFC_CHECK_GSSAPI_LIB
    fi

    if test "$yafc_found_gssapi_lib" = "yes"; then
      if test -z "$yafc_with_gssapi_inc"; then
        YAFC_SEARCH_GSSAPI_HEADERS(["" \
                                    /usr/heimdal/include \
                                    /usr/kerberos/include \
                                    /usr/include/kerberosV \
                                    /usr/include/kerberos \
                                    /usr/include/krb5 \
                                    /usr/local/include \
                                    /usr/athena/include])
      else
        YAFC_SEARCH_GSSAPI_HEADERS([$yafc_with_gssapi_inc])
      fi
      if test "$yafc_found_gssapi_inc" = "yes"; then
        yafc_found_gssapi="yes"
      fi
    fi
  fi

  if test "$yafc_found_gssapi" = "no"; then
    yafc_found_krb5="no"
  fi

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


  if test "$yafc_found_krb5" = "yes"; then
    AC_DEFINE([HAVE_KRB5], [1], [define if you have Kerberos 5])
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



AC_DEFUN(YAFC_SEARCH_KRB5_LIBS_IN,
[
  if test -z "$1"; then
    AC_MSG_CHECKING([for Kerberos 5])
  else
    AC_MSG_CHECKING([for Kerberos 5 in $1])
  fi
  saved_LIBS=$LIBS
  saved_LDFLAGS=$LDFLAGS

  for xlibs in                                dnl
    "-lk5crypto -lcom_err"                    dnl new mit krb
    "-lcrypto -lcom_err"                      dnl old mit krb
    "-lasn1 -ldes -lroken"                    dnl heimda w/o dep on db
    "-lasn1 -ldes -lroken -lresolv"           dnl heimdal w/o dep on db
    "-lasn1 -lcrypto -lcom_err -lroken"       dnl heimdal-BSD w/o dep on db
    "-lasn1 -lcrypto -lcom_err -lroken -lcrypt -lresolv"       dnl 
    "-lasn1 -ldes -lroken -ldb"               dnl heimdal
    "-lasn1 -ldes -lroken -ldb -lresolv"      dnl heimdal
    "-lasn1 -lcrypto -lcom_err -lroken -ldb"  dnl heimdal-BSD
  ; do
    LIBS="-lkrb5 $xlibs $saved_LIBS"
    if test -n "$1"; then
      LDFLAGS="-L$1 $saved_LDFLAGS"
    fi

    AC_LINK_IFELSE(
    [
      AC_LANG_PROGRAM([[ void krb5_init_context();  ]],
                      [[ krb5_init_context(); ]])
    ], [ # action-if-found
      yafc_found_krb5_lib="yes"
      yafc_found_krb5_lib_dir=$1
      yafc_found_krb5_lib_libs="-lkrb5 $xlibs"
      if test -n "$1"; then
        yafc_found_krb5_lib_flags="-L$1"
      fi
      break
    ], [ # action-if-not-found
      yafc_found_krb5_lib="no"
    ])
  done
  AC_MSG_RESULT([$yafc_found_krb5_lib])
  LIBS=$saved_LIBS
  LDFLAGS=$saved_LDFLAGS
])

AC_DEFUN(YAFC_SEARCH_KRB5_LIBS,
[
  for p in $1; do
    if test -z "$p" -o -d "$p"; then
      YAFC_SEARCH_KRB5_LIBS_IN($p)
      if test "$yafc_found_krb5_lib" = "yes"; then
        break
      fi
    fi
  done
])

AC_DEFUN(YAFC_CHECK_GSSAPI_LIB,
[
  AC_MSG_CHECKING([for GSSAPI library])

  saved_LIBS=$LIBS
  saved_LDFLAGS=$LDFLAGS

  for lib in "-lgssapi" "-lgssapi_krb5" ; do
    LDFLAGS="$yafc_found_krb5_lib_flags $saved_LDFLAGS"
    LIBS="$lib $yafc_found_krb5_lib_libs $saved_LIBS"

    AC_LINK_IFELSE(
    [
      AC_LANG_PROGRAM([[ void gss_init_sec_context();  ]],
                      [[ gss_init_sec_context(); ]])
    ], [ # action-if-found
      yafc_found_gssapi_lib="yes"
      yafc_found_krb5_lib_libs="$lib $yafc_found_krb5_lib_libs"
      break
    ], [ # action-if-not-found
      yafc_found_gssapi_lib="no"
    ])
  done

  AC_MSG_RESULT([$yafc_found_gssapi_lib])

  LIBS=$saved_LIBS
  LDFLAGS=$saved_LDFLAGS
])

AC_DEFUN(YAFC_SEARCH_GSSAPI_HEADERS,
[
  saved_LIBS=$LIBS
  saved_LDFLAGS=$LDFLAGS
  saved_CPPFLAGS=$CPPFLAGS

  LIBS="$yafc_found_krb5_lib_libs $saved_LIBS"
  LDFLAGS="$yafc_found_krb5_lib_flags $saved_LDFLAGS"

  for p in $1; do
   if test -z "$p" -o -d "$p"; then
    if test -z "$p"; then
      AC_MSG_CHECKING([for GSSAPI headers])
    else
      AC_MSG_CHECKING([for GSSAPI headers in $p])
      CPPFLAGS="-I$p $saved_CPPFLAGS"
    fi
    YAFC_SEARCH_HEADER_IN(gssapi,GSSAPI,[gssapi.h], $p, [gss_buffer_desc foo;])
    if test "$yafc_found_gssapi_inc" = "yes"; then
      AC_MSG_RESULT([$yafc_found_gssapi_inc])
    else
      YAFC_SEARCH_HEADER_IN(gssapi, GSSAPI, [gssapi/gssapi.h], $p,
        [gss_buffer_desc foo;])
      AC_MSG_RESULT([$yafc_found_gssapi_inc])
      if test "$yafc_found_gssapi_inc" = "yes"; then
        AC_CHECK_HEADERS([gssapi/gssapi_krb5.h],,,
          [
#include <gssapi/gssapi.h>
          ])
      fi
    fi
    if test "$yafc_found_gssapi_inc" = "yes"; then
      yafc_found_krb5_inc_flags="$yafc_found_gssapi_inc_flags $yafc_found_krb5_inc_flags"
      break
    fi
   fi
  done

  LIBS=$saved_LIBS
  LDFLAGS=$saved_LDFLAGS
  CPPFLAGS=$saved_CPPFLAGS
]);

AC_DEFUN(YAFC_SEARCH_KRB5_HEADERS,
[
  for p in $1; do
    if test -z "$p" -o -d "$p"; then
      YAFC_SEARCH_HEADER(krb5, [Kerberos 5], [krb5.h], $p, [krb5_kvno foo;])
      if test "$yafc_found_krb5_inc" = "yes"; then
        break
      fi
    fi
  done
]);


dnl *************************************************************
dnl Kerberos 4


AC_DEFUN(YAFC_SEARCH_KRB4_LIBS_IN,
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

AC_DEFUN(YAFC_SEARCH_KRB4_LIBS,
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

AC_DEFUN(YAFC_SEARCH_KRB4_HEADERS,
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

AC_DEFUN(YAFC_CHECK_KRB_GET_OUR_IP_FOR_REALM,
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
AC_DEFUN(YAFC_SEARCH_HEADER_IN,
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
AC_DEFUN(YAFC_SEARCH_HEADER,
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
