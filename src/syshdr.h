/*
 * syshdr.h -- includes global header files etc.
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _syshdr_h_included
#define _syshdr_h_included

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

/* data type checks stolen from openssh-2.9p1
 */
#ifdef HAVE_SYS_BITYPES_H
# include <sys/bitypes.h> /* For u_intXX_t */
#endif
/* If sys/types.h does not supply u_intXX_t, supply them ourselves */
#ifndef HAVE_U_INTXX_T
# ifdef HAVE_UINTXX_T
typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
# define HAVE_U_INTXX_T 1
# else
#  if (SIZEOF_CHAR == 1)
typedef unsigned char u_int8_t;
#  else
#   error "8 bit int type not found."
#  endif
#  if (SIZEOF_SHORT_INT == 2)
typedef unsigned short int u_int16_t;
#  else
#   ifdef _CRAY
typedef unsigned long  u_int16_t;
#   else
#    error "16 bit int type not found."
#   endif
#  endif
#  if (SIZEOF_INT == 4)
typedef unsigned int u_int32_t;
#  else
#   ifdef _CRAY
typedef unsigned long  u_int32_t;
#   else
#    error "32 bit int type not found."
#   endif
#  endif
# endif
#endif
/* 64-bit types */
#ifndef HAVE_U_INT64_T
# ifdef HAVE_UINT64_T
typedef uint64_t u_int64_t;
#  define HAVE_U_INT64_T 1
# elif (SIZEOF_LONG_INT == 8)
typedef unsigned long int u_int64_t;
#  define HAVE_U_INT64_T 1
# elif (SIZEOF_LONG_LONG_INT == 8)
typedef unsigned long long int u_int64_t;
#  define HAVE_U_INT64_T 1
# endif
#endif

#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <stdarg.h>
#include <setjmp.h>

#include <errno.h>
#include <grp.h>

#include "xmalloc.h"

#include <sys/ioctl.h>

#if defined HAVE_DECL_HERROR && !HAVE_DECL_HERROR
void herror(const char *s);
#endif
#if defined HAVE_DECL_ASPRINTF && !HAVE_DECL_ASPRINTF
int asprintf(char **strp, const char *format, ...);
#endif
#if defined HAVE_DECL_VASPRINTF && !HAVE_DECL_VASPRINTF
int vasprintf(char **strp, const char *format, va_list ap);
#endif

#ifdef STDC_HEADERS
# include <string.h>
#else
# ifndef HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
char *strchr (), *strrchr ();
# ifndef HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
#  define memmove(d, s, n) bcopy ((s), (d), (n))
# endif
#endif

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_UNAME
# include <sys/utsname.h>
#endif

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#elif defined(HAVE_SYS_TIME_H)
# include <sys/time.h>
#else
# include <time.h>
#endif

#include <utime.h>

#ifdef HAVE_PWD_H
# include <pwd.h>
#endif

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

/* networking headers */
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#ifdef HAVE_NETDB_H
# include <netdb.h>
#endif
#ifdef HAVE_NETINET_IN_SYSTM_H /* needed for n_time (on NetBSD) */
# include <netinet/in_systm.h>
#endif
#ifdef HAVE_NETINET_IP_H
# include <netinet/ip.h> /* for IPTOS_* */
#endif
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h> /* for inet_aton() or inet_addr()  */
#endif


#ifdef HAVE_LIBREADLINE
#  if defined(HAVE_READLINE_READLINE_H)
#    include <readline/readline.h>
#  elif defined(HAVE_READLINE_H)
#    include <readline.h>
#  else /* !defined(HAVE_READLINE_H) */
extern char *readline ();
#  endif /* !defined(HAVE_READLINE_H) */
#else /* !defined(HAVE_READLINE_READLINE_H) */
  /* no readline */
#endif /* HAVE_LIBREADLINE */

#ifdef HAVE_READLINE_HISTORY
#  if defined(HAVE_READLINE_HISTORY_H)
#    include <readline/history.h>
#  elif defined(HAVE_HISTORY_H)
#    include <history.h>
#  else /* !defined(HAVE_HISTORY_H) */
extern void add_history ();
extern int write_history ();
extern int read_history ();
#  endif /* defined(HAVE_READLINE_HISTORY_H) */
  /* no history */
#endif /* HAVE_READLINE_HISTORY */


#ifdef HAVE_FNMATCH_GNU
#include <fnmatch.h>
#else
#include "fnmatch.h"  /* our own, in lib/ */
#endif

#if defined(HAVE_GETOPT_LONG) && defined(HAVE_GETOPT_H)
# include <getopt.h>
#else
# include "getopt.h"  /* our own, in lib/ */
#endif

#ifndef HAVE_STRCASECMP
# include "strcasecmp.h"  /* our own, in lib/ */
#endif

#define min(a, b) ((a) < (b) ? (a) : (b))

#ifdef HAVE_KERBEROS
# define SECFTP
# include "security.h"  /* our own, in lib/ */
# ifndef HAVE_STRLCPY
size_t strlcpy (char *dst, const char *src, size_t dst_sz);
# endif
#endif

/* NLS stuff (Native Language Support) */
#undef _  /* des.h seems to define this */
#ifdef ENABLE_NLS
# include <libintl.h>
# define _(text) gettext(text)
#else
# define _(text) (text)
#endif

#ifdef HAVE_LIBSOCKS5
# define SOCKS
# include <socks.h>
#else
# ifdef HAVE_LIBSOCKS
#  define connect Rconnect
#  define getsockname Rgetsockname
#  define getpeername Rgetpeername
#  define bind Rbind
#  define accept Raccept
#  define listen Rlisten
#  define select Rselect
# endif
#endif

typedef RETSIGTYPE (*sighandler_t)(int);

#ifndef bool
typedef enum {false, true} bool;
#endif

#define test(a, b)   (((a) & (b)) == (b))
#define STD_SHELL "/bin/sh"
#define DEFAULT_PAGER "more"


#ifndef YAFC_PRINTF
# if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
#  define YAFC_PRINTF(format_idx, arg_idx) \
    __attribute__((__format__ (__printf__, format_idx, arg_idx)))
# else
#  define YAFC_PRINTF(format_idx, arg_idx)
# endif
#endif

#endif
