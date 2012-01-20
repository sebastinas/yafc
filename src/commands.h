/* $Id: commands.h,v 1.9 2003/07/12 10:25:41 mhe Exp $
 *
 * commands.h --
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _commands_h_included
#define _commands_h_included

#include <config.h>
#include "args.h"

#define DEFCMD(CMDNAME)    void cmd_ ## CMDNAME (int argc, char **argv)

#define maxargs(n) \
  if(argc > n+1) { \
    char *fooargs; \
    fooargs = args_cat(argc, argv, n+1); \
    fprintf(stderr, _("unexpected arguments -- '%s', try '%s --help'" \
					  " for more information\n"), fooargs, argv[0]); \
    free(fooargs); \
    return; \
  }

#define maxargs_nohelp(n) \
  if(argc > n+1) { \
    char *fooargs; \
    fooargs = args_cat(argc, argv, n+1); \
    fprintf(stderr, _("unexpected arguments -- '%s'\n"), fooargs); \
    free(fooargs); \
    return; \
  }

#define minargs(n) \
  if(argc <= n) { \
    fprintf(stderr, _("missing argument, try '%s --help'" \
					  " for more information\n"), argv[0]); \
    return; \
  }

#define minargs_nohelp(n) \
  if(argc <= n) { \
    fprintf(stderr, _("missing argument\n")); \
    return; \
  }

#define need_connected() \
  if(!ftp_connected()) { \
    fprintf(stderr, _("Not connected. Try 'open --help'" \
					  " for more information.\n")); \
    return; \
  }

#define need_loggedin() \
  if(!ftp_loggedin()) { \
    fprintf(stderr, _("Not logged in. Try 'user --help'" \
					  " for more information.\n")); \
    return; \
  }

typedef enum {
	cpNone, cpRemoteFile, cpRemoteDir, cpLocalFile,
	cpCommand, cpAlias, cpVariable, cpHostname, cpBookmark,
	cpFtpList, cpTaglist, cpLocalTaglist, cpUnset
} cpl_t;

/* in completion.c */
extern cpl_t force_completion_type;

DEFCMD(cachelist);
DEFCMD(status);
DEFCMD(quit);
DEFCMD(exit);
DEFCMD(filetime);
DEFCMD(source);
DEFCMD(system);
DEFCMD(switch);

/* in bookmark.c */
DEFCMD(bookmark);

/* cacheops.c */
DEFCMD(cache);

/* login.c */
DEFCMD(open);
DEFCMD(reopen);
DEFCMD(close);
DEFCMD(user);

DEFCMD(cdup);
DEFCMD(cd);
DEFCMD(pwd);
DEFCMD(url);
DEFCMD(mkdir);
DEFCMD(rmdir);
DEFCMD(mv);
DEFCMD(chmod);
DEFCMD(cat);
DEFCMD(page);
DEFCMD(zcat);
DEFCMD(zpage);

DEFCMD(quote);
DEFCMD(site);
DEFCMD(rhelp);
DEFCMD(rstatus);
DEFCMD(sstatus);
DEFCMD(nop);
DEFCMD(idle);

/* rm.c */
DEFCMD(rm);

/* get.c */
DEFCMD(get);

/* put.c */
DEFCMD(put);

/* fxp.c */
DEFCMD(fxp);

/* ls.c */
DEFCMD(ls);
DEFCMD(nlist);
DEFCMD(list);

/* help.c */
DEFCMD(help);
DEFCMD(version);
DEFCMD(warranty);
DEFCMD(copyright);

/* set.c */
DEFCMD(set);

/* alias.c */
DEFCMD(alias);
DEFCMD(unalias);

/* local.c */
DEFCMD(lpwd);
DEFCMD(lcd);
DEFCMD(shell);

#ifdef SECFTP
/* in ../lib/security.c */
DEFCMD(prot);
#endif
#ifdef HAVE_KRB4
/* in ../lib/kauth.c */
DEFCMD(kauth);
DEFCMD(kdestroy);
DEFCMD(klist);
DEFCMD(afslog);
DEFCMD(krbtkfile);
#endif

/* tag.c */
DEFCMD(tag);
DEFCMD(untag);
DEFCMD(taglist);
DEFCMD(taginfo);

/* ltag.c */
DEFCMD(ltag);
DEFCMD(luntag);
DEFCMD(ltaglist);
DEFCMD(ltaginfo);

#define OPT_HELP(help) { opt_help(argc, argv, _(help)); \
 if(optind == -1) return; }

void opt_help(int argc, char **argv, char *help);
void expand_alias_parameters(args_t **args, args_t *alias_args);

#endif
