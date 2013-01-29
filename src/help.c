/*
 * help.c -- local help and info
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
#include "alias.h"
#include "cmd.h"
#include "input.h"
#include "help.h"
#include "gvars.h"
#include "commands.h"

void cmd_help(int argc, char **argv)
{
#ifdef HAVE_LIBREADLINE
	Function *func;
#endif
	int i;
	listitem *li;

	maxargs_nohelp(1);

  if(argc==2) {
    if(find_func(argv[1], false)) {
      const size_t len = strlen(argv[1]) + 8;
      char *tmp = xmalloc(len);
      snprintf(tmp, len, "%s --help", argv[1]);
      exe_cmdline(tmp, false);
      return;
    } else {
			fprintf(stderr, _("No such command '%s'!\n\n"), argv[1]);
    }
  };

	fprintf(stderr, _("Available commands: (commands may be abbreviated)\n"));
#ifdef HAVE_LIBREADLINE
	/* hack to let readline display all commands */
	rl_point=rl_end=0;
	func = rl_named_function("possible-completions");
	if(func)
		func();
	else {
#endif
		for(i=0; cmds[i].cmd; i++)
			printf("%s\n", cmds[i].cmd);
		for(li = gvAliases->first; li; li = li->next)
			printf("%s\n", ((alias *)li->data)->name);
#ifdef HAVE_LIBREADLINE
	}
#endif
}

void cmd_version(int argc, char **argv)
{
	maxargs_nohelp(0);

	printf(FULLVER "\n");

	printf(_("Compiled " __TIME__ " " __DATE__ " (" HOSTTYPE ")\n"));
#if HAVE_LIBREADLINE
	printf(_("Using Readline version %s.\n"), rl_library_version);
#endif
#ifdef HAVE_LIBSSH
	printf(_("Using libssh version %d.%d.%d.\n"), LIBSSH_VERSION_MAJOR,
			LIBSSH_VERSION_MINOR, LIBSSH_VERSION_MICRO);
#endif
}

void cmd_warranty(int argc, char **argv)
{
	maxargs_nohelp(0);
	puts(WARRANTY);
}

void cmd_copyright(int argc, char **argv)
{
	maxargs_nohelp(0);
	puts(COPYRIGHT);
}
