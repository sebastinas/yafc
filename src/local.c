/* $Id: local.c,v 1.5 2003/07/12 10:25:41 mhe Exp $
 *
 * local.c -- local commands
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#include "syshdr.h"
#include "input.h"
#include "commands.h"
#include "gvars.h"
#include "strq.h"
#include "utils.h"

/* print local working directory */
void cmd_lpwd(int argc, char **argv)
{
	char tmp[PATH_MAX+1];

	if(argv) {
		OPT_HELP("Print local working directory.  Usage:\n"
				 "  lpwd [options]\n"
				 "Options:\n"
				 "  -h, --help    show this help\n");

		maxargs(optind - 1);
	}

	if(getcwd(tmp, PATH_MAX) == 0) {
		fprintf(stderr, _("Couldn't get local working directory...\n"));
		return;
	}
	printf(_("local working directory is '%s'\n"), tmp);
}

/* local change directory */
void cmd_lcd(int argc, char **argv)
{
	char *e = 0, tmp[PATH_MAX+1];
	char *te;

	OPT_HELP("Change local working directory.  Usage:\n"
			 "  lcd [options] [directory]\n"
			 "Options:\n"
			 "  -h, --help    show this help\n"
			 "if [directory] is '-', lcd changes to the previous working directory\n"
			 "if omitted, changes to home directory\n");

	maxargs(optind);

	if(argc < optind + 1)
		e = gvLocalHomeDir;
	else {
		e = argv[optind];
		if(strcmp(e, "-") == 0)
			e = gvLocalPrevDir;
	}
	if(!e)
		return;
	getcwd(tmp, PATH_MAX);
	te = tilde_expand_home(e, gvLocalHomeDir);
	if(chdir(te) == -1)
		perror(te);
	else {
		free(gvLocalPrevDir);
		gvLocalPrevDir = xstrdup(tmp);
	}
	free(te);
	cmd_lpwd(0, 0);
}

/* FIXME: consider the following:
 *  shell wget -r -q %f
 * which expands to
 *  shell wget -r -q ftp://user:pass@site:port/dir
 * could be useful!?
 *
 * %f - full path ("ftp://user:pass@site:port/dir")
 * %h - hostname ("ftp.foo.bar")
 * %p - port number
 * %u - username
 * %d - current directory
 */

void cmd_shell(int argc, char **argv)
{
	char *e = 0;

	if(argc > 1)
		e = args_cat(argc, argv, 1);
	invoke_shell(e);
	free(e);
}
