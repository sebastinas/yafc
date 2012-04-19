dnl YAFC_CHECK_TYPES
dnl stolen from configure.in in openssh-2.9p1

AC_DEFUN([YAFC_CHECK_TYPES],
[
  dnl Checks for data types

  AC_CACHE_CHECK([for u_intXX_t types], ac_cv_have_u_intxx_t, [
    AC_TRY_COMPILE(
      [ #include <sys/types.h> ],
      [ u_int8_t a; u_int16_t b; u_int32_t c; a = b = c = 1;],
      [ ac_cv_have_u_intxx_t="yes" ],
      [ ac_cv_have_u_intxx_t="no" ]
    )
  ])
  if test "x$ac_cv_have_u_intxx_t" = "xyes" ; then
    AC_DEFINE([HAVE_U_INTXX_T], [], [define if you have u_intxx_t types])
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
      AC_DEFINE([HAVE_UINTXX_T], [], [define if you have uintxx_t types])
      have_u_intxx_t=1
    fi
  fi

  if test -z "$have_u_intxx_t" ; then
    AC_CHECK_SIZEOF(char, 1)
    AC_CHECK_SIZEOF(short int, 2)
    AC_CHECK_SIZEOF(int, 4)
  fi

  AC_CACHE_CHECK([for u_int64_t type], ac_cv_have_u_int64_t, [
    AC_TRY_COMPILE(
      [ #include <sys/types.h> ],
      [ u_int64_t a; a = 1;],
      [ ac_cv_have_u_int64_t="yes" ],
      [ ac_cv_have_u_int64_t="no" ]
    )
  ])
  if test "x$ac_cv_have_u_int64_t" = "xyes" ; then
    AC_DEFINE([HAVE_U_INT64_T], [], [define if you have u_int64_t type])
    have_u_int64_t=1
  fi

  if test -z "$have_u_int64_t" ; then
    AC_CACHE_CHECK([for uint64_t type], ac_cv_have_uint64_t, [
      AC_TRY_COMPILE(
        [ #include <sys/types.h> ],
        [ uint64_t a; a = 1;],
        [ ac_cv_have_uint64_t="yes" ],
        [ ac_cv_have_uint64_t="no" ]
      )
    ])
    if test "x$ac_cv_have_uint64_t" = "xyes" ; then
      AC_DEFINE([HAVE_UINT64_T], [], [define if you have uint64_t type])
      have_u_int64_t=1
    fi
  fi

  if test -z "$have_u_int64_t" ; then
    AC_CHECK_SIZEOF(long int, 4)
    AC_CHECK_SIZEOF(long long int, 8)
  fi

])
