/* $Id: help.c,v 1.7 2001/05/21 20:52:23 mhe Exp $
 *
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

void cmd_help(int argc, char **argv)
{
	cmd_t *cp;
	alias *a;
#ifdef HAVE_LIBREADLINE
	Function *func;
#endif
	int i;
	if(argc > 1) {
		for(i=1; i<argc; i++) {
			cp = find_cmd(argv[i]);
			a = alias_search(argv[i]);
			if(cp) {
				if(cp == CMD_AMBIGUOUS)
					printf("%s: amgiguous command\n", argv[i]);
				else {
					if(cp->hint)
						printf("%s: %s\n", argv[i], cp->hint);
					else {
						char *tmp[2];
						tmp[0]=cp->cmd;
						tmp[1]="--help";
						cp->func(2, tmp);
					}
				}
			} else if(a) {
				if(a == ALIAS_AMBIGUOUS)
					printf("%s: amgiguous alias\n", argv[i]);
				else {
					char *tmp = args_cat2(a->value, 0);
					printf(_("%s is an alias for '%s'\n"), a->name, tmp);
					xfree(tmp);
				}
			} else
				printf(_("%s: no such command\n"), argv[i]);
		}
	} else {
		fprintf(stderr,
				_("Available commands: (commands may be abbreviated)\n"));
#ifdef HAVE_LIBREADLINE
		/* hack to let readline display all commands */
		rl_point=rl_end=0;
		func = rl_named_function("possible-completions");
		if(func)
			func();
		else
#endif
		{
			listitem *li = gvAliases->first;
			for(i=0;cmds[i].cmd;i++)
				printf("%s\n", cmds[i].cmd);
			while(li) {
				printf("%s\n", ((alias *)li->data)->name);
				li = li->next;
			}
		}
	}
}

void cmd_version(int argc, char **argv)
{
	printf(FULLVER "\n");

#if defined(KRB4) || defined(KRB5)
	printf(_("This product includes software developed by the"
			 " Kungliga Tekniska\n"
			 "Högskolan and its contributors.\n\n"));
#endif

	printf(_("Compiled " __TIME__ " " __DATE__ " (" HOSTTYPE ")\n"));
#if (HAVE_LIBREADLINE >= 210)
	printf(_("Using Readline version %s\n"), rl_library_version);
#elif defined(HAVE_LIBREADLINE)
	printf(_("Using Readline version 2.0\n"));
#endif
}

void cmd_warranty(int argc, char **argv)
{
	puts(WARRANTY);
}

void cmd_copyright(int argc, char **argv)
{
	puts(COPYRIGHT);

#if defined(KRB4) || defined(KRB5)
	puts(_("This product includes software developed by the"
		   " Kungliga Tekniska\n" \
         "Högskolan and its contributors.\n"));
#endif
}
