/* $Id: syshdr.h,v 1.7 2001/05/13 11:49:43 mhe Exp $
 *
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
# include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
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

#ifdef NEED_herror_PROTO
void herror(const char *s);
#endif
#ifdef NEED_vasprintf_PROTO
int vasprintf(char **strp, const char *format, va_list ap);
#endif
#ifdef NEED_asprintf_PROTO
int asprintf(char **strp, const char *format, ...);
#endif

#if 0 && (!defined(HAVE_SETPROCTITLE) && defined(linux))
# include "setproctitle.h"
#endif

#if STDC_HEADERS
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

#ifdef HAVE_READLINE_READLINE_H
# include <readline/readline.h>
#endif
#ifdef HAVE_READLINE_HISTORY_H
# include <readline/history.h>
#endif

#if HAVE_LIBREADLINE < 420
# define rl_completion_matches completion_matches
# define rl_filename_completion_function filename_completion_function
# define rl_compentry_func_t Function
# define rl_dequote_func_t CPFunction
#endif

#if (HAVE_LIBREADLINE == 200)
int rl_insert_text();
int rl_do_undo();
int rl_forced_update_display();
char *filename_completion_function();
void rl_redisplay();
void add_history();
void stifle_history();
void write_history();
void read_history();
Function *rl_named_function();
extern int rl_filename_completion_desired;
#endif

#include "glob.h"  /* we always use GNU glob now */

/*#if defined(HAVE_FNMATCH) && defined(HAVE_FNMATCH_H)*/
/*# include <fnmatch.h>*/
/*#else*/
#include "fnmatch.h"  /* our own, in lib/ */
/*#endif*/

#if defined(HAVE_GETOPT_LONG) && defined(HAVE_GETOPT_H)
# include <getopt.h>
#else
# include "getopt.h"  /* our own, in lib/ */
#endif

#ifndef HAVE_STRCASECMP
# include <strcasecmp.h>  /* our own, in lib/ */
#endif

#define min(a, b) ((a) < (b) ? (a) : (b))

#if defined(KRB4) || defined(KRB5)
# ifdef HAVE_DES_H
#  include <des.h>
# endif
# ifdef HAVE_KRB_H
#  include <krb.h>
# endif
# ifdef HAVE_KRB5_H
#  include <krb5.h>
# endif
# include "security.h"  /* our own, in lib/ */
# ifndef HAVE_STRLCPY
size_t strlcpy (char *dst, const char *src, size_t dst_sz);
# endif
#endif

#ifndef HAVE_STRLCAT
size_t strlcat(char *dst, const char *src, size_t siz);
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

#include <pathmax.h>  /* define PATH_MAX somehow, requires sys/types.h */

typedef RETSIGTYPE (*sighandler_t)(int);

#ifndef bool
typedef enum {false, true} bool;
#endif

#define test(a, b)   (((a) & (b)) == (b))
#define STD_SHELL "/bin/sh"
#define DEFAULT_PAGER "more"

#endif
