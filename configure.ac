dnl Process this file with autoconf to produce an m4 configure script.

dnl init

AC_PREREQ([2.65])
AC_INIT([yafc],
        [1.2.1],
        [https://github.com/sebastinas/yafc/issues/],
        [yafc],
        [http://yafc-ftp.com])
AC_CONFIG_SRCDIR(src/main.c)
AC_CONFIG_LIBOBJ_DIR([lib])
AC_PROG_CC_C99
AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_CONFIG_MACRO_DIR([cf])
AC_CONFIG_AUX_DIR(support)
AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_HEADERS([config.h])

CDPATH=""

AC_CANONICAL_HOST
AC_DEFINE_UNQUOTED(HOSTTYPE, "${host_cpu}-${host_vendor}-${host_os}", [the hosttype])

AC_PROG_INSTALL

dnl gettext et al
AC_ARG_ENABLE([nls],
              [AS_HELP_STRING([--enable-nls],[enable native language support])],,
              [enable_nls=yes])
use_nls=no
if test "x$enable_nls" = "xyes"; then
  use_nls=yes

  IT_PROG_INTLTOOL
  GETTEXT_PACKAGE=yafc
  AC_SUBST(GETTEXT_PACKAGE)
  AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"],
                     [The domain to use with gettext])
  AC_DEFINE([ENABLE_NLS], [], [Define is you have gettext support.])
fi
AM_CONDITIONAL([USE_NLS],[test "x$use_nls" = "xyes"])

YAFC_LOCALEDIR=[${datadir}/locale]
AC_SUBST(YAFC_LOCALEDIR)

dnl
dnl Checks for header files.
dnl

AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_HEADER_STAT
AC_HEADER_TIME
AC_HEADER_STDBOOL
AC_HEADER_DIRENT

AC_CHECK_HEADER([stdint.h],[],
                [AC_MSG_ERROR([stdint.h is required by C99 but is not present.])])

AC_CHECK_HEADERS(unistd.h \
                 pwd.h \
                 sys/socket.h \
                 sys/types.h \
                 netdb.h \
                 netinet/in.h \
                 netinet/ip.h \
                 netinet/in_systm.h \
                 arpa/inet.h \
                 sys/ioctl.h \
                 memory.h \
                 setjmp.h \
                 errno.h \
                 strcasecmp.h \
                 fcntl.h \
                 resolv.h
)


dnl
dnl Checks for typedefs, structures, and compiler characteristics.
dnl

AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_MODE_T
dnl AC_STRUCT_TM

AC_FUNC_FNMATCH_GNU
if test "$ac_cv_func_fnmatch_gnu" = "yes"; then
  AC_DEFINE([HAVE_FNMATCH_GNU], [],
            [Define if you have a fnmatch supporting GNU extensions])
fi

AC_CHECK_FUNCS(strcoll memcpy sigsetjmp)

BASH_FUNC_POSIX_SETJMP

AC_SEARCH_LIBS(socket, socket, [],
[
  AC_MSG_WARN(cannot find socket function)
])

AC_REPLACE_FUNCS(vasprintf)

AC_CHECK_FUNCS(getopt_long, [],
[
  AC_LIBOBJ([getopt])
  AC_LIBOBJ([getopt1])
])

AC_CHECK_FUNC(regcomp,
[
  AC_CHECK_HEADERS(regex.h)
  AC_DEFINE([HAVE_REGEX], [], [define if you have regex])
])

# socks
YAFC_ARG_WITH(socks, no, [Socks])
if test "$yafc_with_socks" = "yes"; then
  AC_CHECK_LIB(socks, Rconnect)
fi

YAFC_ARG_WITH(socks5, no, [Socks 5])
if test "$yafc_with_socks5" = "yes"; then
  AC_CHECK_LIB(socks5, SOCKSconnect)
fi

# readline
yafc_got_readline="no, disabled"
YAFC_ARG_WITH(readline, yes, [Readline])
if test "$yafc_with_readline" = "yes"; then
  AX_LIB_READLINE
  if test "$ax_cv_lib_readline" = "no"; then
    yafc_got_readline="no, not found"
  else
    yafc_got_readline="yes"
  fi
fi

# misc functions
AC_SEARCH_LIBS([strerror],[cposix])

AC_SEARCH_LIBS([inet_ntop, getaddrinfo], resolv nsl,,
               [AC_MSG_ERROR([inet_ntop or getaddrinfo is missing])])
AC_SEARCH_LIBS(herror, resolv nsl)

AC_CHECK_FUNCS(gettimeofday uname setsockopt)

AC_CHECK_DECLS([asprintf, vasprintf])
AC_CHECK_DECLS([herror],,,[#include <netdb.h>])
AC_CHECK_DECLS([strcasecmp],,
               [AC_MSG_ERROR([strcasecmp is not available.])],
               [
                #ifdef HAVE_STRINGS_H
                #include <strings.h>
                #endif
               ])

# kerberos
YAFC_KERBEROS
if test "$yafc_found_krb4" = "yes"; then
  AC_LIBOBJ(krb4)
  AC_LIBOBJ(kauth)
  LIBS="$yafc_found_krb4_lib_libs $LIBS"
  CPPFLAGS="$yafc_found_krb4_inc_flags $CPPFLAGS"
  LDFLAGS="$yafc_found_krb4_lib_flags $LDFLAGS"

  AC_SEARCH_LIBS(strlcpy, [bsd], [], [AC_REPLACE_FUNCS(strlcpy)])
  AC_CHECK_HEADERS(bsd/string.h)
fi

if test "$yafc_found_krb5" = "yes"; then
  AC_LIBOBJ(gssapi)
  LIBS="$yafc_found_krb5_lib_libs $LIBS"
  CPPFLAGS="$yafc_found_krb5_inc_flags $CPPFLAGS"
  LDFLAGS="$yafc_found_krb5_lib_flags $LDFLAGS"

  if test "$yafc_krb5_vendor" = "Heimdal" ; then
    AC_CHECK_HEADERS(hcrypto/ui.h)
  fi
fi

if test "x$yafc_found_krb" = "xyes"; then
  AC_LIBOBJ(security)
fi

yafc_have_openssl="notchecked"
if test "x$yafc_krb5_vendor" != "xHeimdal" ; then
  AX_CHECK_OPENSSL(
   [
    AC_CHECK_HEADERS(openssl/ui_compat.h \
                     openssl/ui.h)
    yafc_have_openssl="yes"
   ],
   [
    yafc_have_openssl="no"
   ])
fi

# read_pw_string
old_CPPFLAGS="$CPPFLAGS $OPENSSL_INCLUDES"
yafc_have_read_pw_string="no"
AC_CHECK_DECLS(des_read_pw_string, [yafc_have_read_pw_string="yes"],,
  [
    #ifdef HAVE_OPENSSL_UI_COMPAT_H
    #include <openssl/ui_compat.h>
    #endif
  ])
AC_CHECK_DECLS(hc_UI_UTIL_read_pw_string,
  [
   AC_SEARCH_LIBS(hc_UI_UTIL_read_pw_string, hcrypto,
                  [yafc_have_read_pw_string="yes"])
  ],,
  [
    #ifdef HAVE_HCRYPTO_UI_H
    #include <hcrypto/ui.h>
    #endif
  ])
AC_CHECK_DECLS(UI_UTIL_read_pw_string, [yafc_have_read_pw_string="yes"],,
  [
    #if defined(HAVE_OPENSSL_UI_H)
    #include <openssl/ui.h>
    #endif
  ])
CPPFLAGS="$old_CPPFLAGS"

if test "x$yafc_have_read_pw_string" = "xyes" ; then
  LIBS="$LIBS $OPENSSL_LIBS"
  LDFLAGS="$LDFLAGS $OPENSSL_LDFLAGS"
  CPPFLAGS="$CPPFLAGS $OPENSSL_INCLUDES"
else
  AC_CHECK_HEADERS(termios.h)
  AC_CHECK_DECLS([IUCLC],,,
    [
      #include <fcntl.h>
      #include <sys/ioctl.h>
      #include <termios.h>
    ])
  AC_CHECK_DECLS([TCGETS],,
    [AC_CHECK_DECLS([TIOCGETA],,
      AC_MSG_ERROR([TCGETS and TIOCGETA are not available]),
      [
        #include <fcntl.h>
        #include <sys/ioctl.h>
        #include <termios.h>
      ])
    ],
    [
      #include <fcntl.h>
      #include <sys/ioctl.h>
      #include <termios.h>
    ])
  AC_CHECK_DECLS([TCSETS],,
    [AC_CHECK_DECLS([TIOCSETA],,
      AC_MSG_ERROR([TCSETS and TIOCSETA are not available]),
      [
        #include <fcntl.h>
        #include <sys/ioctl.h>
        #include <termios.h>
      ])
    ],
    [
      #include <fcntl.h>
      #include <sys/ioctl.h>
      #include <termios.h>
    ])
fi

# libssh
AC_ARG_WITH([ssh],
  AS_HELP_STRING([--with-ssh], [Build with SSH support]))
if test "x$with_ssh" != "xno"; then
  PKG_CHECK_MODULES([SSH], [libssh],
    [
      AC_DEFINE(HAVE_LIBSSH, [], [Define if you have libssh installed.])
      AM_CONDITIONAL(HAVE_LIBSSH, [true])
      yafc_got_ssh="yes"
    ],
    [
      AC_CHECK_HEADERS([libssh/libssh.h],
      [
        AC_DEFINE(HAVE_LIBSSH, [], [Define if you have libssh installed.])
        AM_CONDITIONAL(HAVE_LIBSSH, [true])
        LIBS="$LIBS -lssh"
        yafc_got_ssh="yes"
      ],
      [
        AM_CONDITIONAL(HAVE_LIBSSH, [false])
        yafc_got_ssh="no, not found"
      ])
    ])
else
  AM_CONDITIONAL(HAVE_LIBSSH, [false])
  yafc_got_ssh="no, disabled"
fi

# base64
yafc_have_base64="no"
yafc_need_base64_impl="true"
# use Heimdal if available
if test "x$yafc_krb5_vendor" = "xHeimdal" ; then
  AC_CHECK_HEADERS(
   heimdal/base64.h,
   [
    yafc_have_base64="yes"
    yafc_need_base64_impl="false"
    AC_DEFINE(HAVE_BASE64_HEIMDAL, [], [Define if base64.h from Heimdal is available.])
   ])
fi
# check OpenSSL
if test "x$yafc_have_base64" = "xno" && test "x$yafc_have_openssl" != "xno" ; then
  if test "x$yafc_have_openssl" = "xnotchecked" ; then
    AX_CHECK_OPENSSL(
     [yafc_have_openssl="yes"],
     [yafc_have_openssl="no"])
  fi
  if test "x$yafc_have_openssl" = "xyes" ; then
    old_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS $OPENSSL_INCLUDES"

    AC_CHECK_HEADERS(openssl/evp.h)
    AC_CHECK_DECLS(
     BIO_f_base64,
     [
      yafc_have_base64="yes"
      AC_DEFINE(HAVE_BASE64_OPENSSL, [], [Define if base64 from OpenSSL is available.])
      LIBS="$LIBS $OPENSSL_LIBS"
      LDFLAGS="$LDFLAGS $OPENSSL_LDFLAGS"
     ],
     [
      CPPFLAGS="$old_CPPFLAGS"
     ],
     [
      #ifdef HAVE_OPENSSL_EVP_H
      #include <openssl/evp.h>
      #endif
     ])
  fi
fi
# check for b64_ntop and b64_pton
if test "x$yafc_have_base64" = "xno" ; then
  AC_CHECK_DECLS(
   [b64_ntop, b64_pton],
   [
    yafc_have_base64="yes"
    AC_SEARCH_LIBS([b64_ntop, b64_pton], resolv)
    AC_SEARCH_LIBS([__b64_ntop, __b64_pton], resolv)
    AC_DEFINE(HAVE_BASE64_B64, [], [Define if b64_* function are available.])
   ],,
   [
    #ifdef HAVE_RESOLV_H
    #include <resolv.h>
    #endif
   ])
fi

if test "x$yafc_have_base64" = "xno" ; then
  AC_MSG_ERROR([no suitable base64 implementation is available])
fi

AM_CONDITIONAL([NEED_BASE_IMPL], [${yafc_need_base64_impl}])

# bash completion
AC_ARG_WITH([bash-completion],
  AS_HELP_STRING([--with-bash-completion@<:@=DIR@:>@],
                 [install bash_completion conf file in DIR (default is yes)]),
  [
    if test "$withval" = "no"; then
      BASH_COMPLETION_DIR=""
    elif test "$withval" = "yes"; then
      BASH_COMPLETION_DIR=$sysconfdir/bash_completion.d
    else
      BASH_COMPLETION_DIR="$withval"
    fi
  ],
  [BASH_COMPLETION_DIR=$sysconfdir/bash_completion.d])

bash_completiondir="${BASH_COMPLETION_DIR}"
AC_SUBST(BASH_COMPLETION_DIR)
AC_SUBST(bash_completiondir)
AM_CONDITIONAL(USE_BASH_COMPLETION, test "${BASH_COMPLETION_DIR}" != "")

# IPv6
AC_MSG_CHECKING(for IPv6 support)
AC_CACHE_VAL(ac_cv_ipv6,
  AC_RUN_IFELSE(
    [AC_LANG_SOURCE([[
      #include <sys/types.h>
      #include <sys/socket.h>
      #include <netinet/in.h>
      /* Make sure the definitions for AF_INET6 and struct sockaddr_in6
       * are defined, and that we can actually create an IPv6 TCP socket.
       */
      int main()
      {
              int fd;
              struct sockaddr_in6 foo;
              fd = socket(AF_INET6, SOCK_STREAM, 0);
              return (fd >= 0 ? 0 : 1);
      }
    ]])],
    [ac_cv_ipv6=yes],
    [ac_cv_ipv6=no],
    [ac_cv_ipv6=no]))
AC_MSG_RESULT($ac_cv_ipv6)
if test "x$ac_cv_ipv6" = "xyes" ; then
  AC_DEFINE([HAVE_IPV6],[],
            [Define is IPv6 support is available.])
fi

# finish
AC_CONFIG_FILES([Makefile \
                 doc/Makefile \
                 po/Makefile.in])
AC_OUTPUT

echo
echo "compiler ....................... $CC $CFLAGS"
echo "preprocessor flags ............. $CPPFLAGS"
echo "linker flags ................... $LDFLAGS"
echo "libraries ...................... $LIBS"
echo "installation prefix ............ $prefix"
echo "using Kerberos 4 ............... $yafc_found_krb4"
echo "using Kerberos 5 ............... $yafc_found_krb5"
echo "using readline ................. $yafc_got_readline"
echo "using ssh ...................... $yafc_got_ssh"
echo "using IPv6 ..................... $ac_cv_ipv6"
echo "using NLS ...................... $use_nls"
echo
