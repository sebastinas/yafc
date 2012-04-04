dnl Process this file with autoconf to produce an m4 configure script.
dnl $Id: configure.ac,v 1.4 2005/10/05 20:10:09 splicednetworks Exp $

dnl
dnl init
dnl

AC_PREREQ(2.60)
AC_INIT(yafc, 1.1.2, [yafc-devel@lists.sourceforge.net])
AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_CONFIG_MACRO_DIR([cf])
AC_CONFIG_AUX_DIR(support)
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)
AC_CONFIG_SRCDIR(src/main.c)

CDPATH=""

dnl AM_MAINTAINER_MODE

AC_CANONICAL_HOST

AC_DEFINE_UNQUOTED(HOSTTYPE, "${host_cpu}-${host_vendor}-${host_os}", [the hosttype])

AC_PROG_CC

dnl checks for different unix variants
AC_ISC_POSIX
AC_AIX
AC_MINIX

AM_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_LIBTOOL
LT_INIT

dnl check if to use pipes or socketpair in src/ftp/ssh_ftp.c
dnl checks taken from openssh-2.9p1
case "$host" in
*-*-hpux10* | *-*-hpux11* | *-next-* | *-*-sunos4* | *-sni-sysv* | \
*-*-sysv4.2* | *-*-sysv5* | *-*-sco3.2v4* | *-*-sco3.2v5*)
	AC_DEFINE([USE_PIPES], [], [define if using pipes instead of socketpair])
	;;
esac


dnl
dnl Checks for header files.
dnl

AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_HEADER_STAT
AC_HEADER_TIME
AC_CHECK_HEADERS(unistd.h \
                 stdlib.h \
                 pwd.h \
                 sys/stat.h \
                 sys/socket.h \
                 sys/types.h \
                 sys/bitypes.h \
                 netdb.h \
                 netinet/in.h \
                 netinet/ip.h \
                 netinet/in_systm.h \
                 arpa/inet.h \
                 sys/ioctl.h \
                 memory.h \
                 setjmp.h \
                 errno.h
)

AC_CHECK_HEADERS(dirent.h, [], [
  AC_CHECK_HEADERS(ndir.h sys/ndir.h sys/dir.h)
])

dnl
dnl Checks for typedefs, structures, and compiler characteristics.
dnl

AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_MODE_T
dnl AC_STRUCT_TM
YAFC_CHECK_TYPES

AC_FUNC_ALLOCA

AC_CHECK_FUNCS(strcoll memcpy sigsetjmp)

BASH_FUNC_POSIX_SETJMP

AC_SEARCH_LIBS(socket, socket, [],
[
  AC_WARN(can't find socket function)
])

AC_SEARCH_LIBS(gethostbyname, resolv nsl, [],
[
  AC_WARN(can't find resolver functions)
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

YAFC_ARG_WITH(socks, no, [Socks])
if test "$yafc_with_socks" = "yes"; then
  AC_CHECK_LIB(socks, Rconnect)
fi

YAFC_ARG_WITH(socks5, no, [Socks 5])
if test "$yafc_with_socks5" = "yes"; then
  AC_CHECK_LIB(socks5, SOCKSconnect)
fi

yafc_got_readline="no, disabled"
YAFC_ARG_WITH(readline, yes, [Readline])
if test "$yafc_with_readline" = "yes"; then
  YAFC_CHECK_READLINE
fi

AC_SEARCH_LIBS(inet_aton, resolv nsl)
AC_SEARCH_LIBS(herror, resolv nsl)

AC_CHECK_FUNCS(gettimeofday uname setsockopt)

AC_CHECK_DECLS([asprintf, vasprintf])
AC_CHECK_DECLS([herror],,,[#include<netdb.h>])

YAFC_KERBEROS

if test "$yafc_found_krb" = "yes"; then
  AC_LIBOBJ(security)
fi

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
fi

AC_CONFIG_FILES([Makefile \
                 doc/Makefile \
                 lib/Makefile \
                 src/Makefile \
                 src/ftp/Makefile \
                 src/libmhe/Makefile \
                 contrib/yafc-rw.spec
                 contrib/yafc-mandrake.spec])
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
echo
