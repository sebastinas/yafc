/* $Id: alias.c,v 1.4 2001/07/27 08:58:22 mhe Exp $
 *
 * alias.c -- define and undefine aliases
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
#include "linklist.h"
#include "gvars.h"
#include "cmd.h"
#include "commands.h"
#include "strq.h"

static int alias_searchfunc(alias *ap, const char *name)
{
	return strcmp(ap->name, name);
}

alias *alias_create(void)
{
	alias *ap = (alias *)xmalloc(sizeof(alias));
	return ap;
}

void alias_destroy(alias *ap)
{
	if(!ap)
		return;
	alias_clear(ap);
	xfree(ap);
}

void alias_clear(alias *ap)
{
	xfree(ap->name);
	ap->name = 0;
	args_destroy(ap->value);
	ap->value = 0;
}

alias *alias_search(const char *name)
{
	listitem *li;
	alias *ap = 0;
	alias *r = 0;

	li = gvAliases->first;
	while(li) {
		ap = (alias *)li->data;

		/* compare only strlen(name) chars, allowing aliases
		 * to be shortened, as long as they're not ambiguous
		 */
		if(strncmp(((alias *)li->data)->name, name, strlen(name)) == 0) {
			/* is this an exact match? */
			if(strlen(name) == strlen(ap->name))
				return ap;

			if(r)
				r = ALIAS_AMBIGUOUS;
			else
				r = ap;
		}
		li = li->next;
	}
	return r;
}

void alias_define(const char *name, args_t *args)
{
	alias *ap;
	cmd_t *cp;
	listitem *li;

	li = list_search(gvAliases, (listsearchfunc)alias_searchfunc, name);
	ap = li ? li->data : 0;

	if(!ap) {
		ap = alias_create();
		list_additem(gvAliases, ap);
	} else
		alias_clear(ap); /* overwrite if already defined */

	ap->name = xstrdup(name);

	/* special handling of '!', execute shell command */
	if(args->argv[0][0] == '!' && strlen(args->argv[0]) > 1) {
		strpull(args->argv[0], 1);
		args_push_front(args, "!");
	}

	/* warn if strange alias */
	if(strcmp(ap->name, args->argv[0]) != 0) {
		cp = find_cmd(ap->name);
		if(cp && cp != CMD_AMBIGUOUS && strcmp(ap->name, cp->cmd) == 0)
			fprintf(stderr, _("warning: alias '%s' shadows a command with"
							  " the same name\n"),
				   ap->name);
	}
	cp = find_cmd(args->argv[0]);
	if(!cp)
		fprintf(stderr,
				_("warning: alias '%s' points to a non-existing command\n"),
			   ap->name);
	else if(cp == CMD_AMBIGUOUS)
		fprintf(stderr, _("warning: alias '%s' is ambiguous\n"), ap->name);

	ap->value = args;
}

void cmd_alias(int argc, char **argv)
{
	alias *ap;
	listitem *li = 0;

	OPT_HELP("Define an alias.  Usage:\n"
			 "  alias [options]  [name [value]]\n"
			 "Options:\n"
			 "  -h, --help\n"
			 "without arguments, prints all defined aliases\n"
			 "with only [name] argument, prints that alias' value\n"
			 "with [name] and [value] arguments, define [name] to be [value]\n");

	maxargs(optind + 1);

	/* gvAliases created in main.c */

	li = gvAliases->first;

	if(argc < optind + 1) {  /* print all alias bindings */
		if(!li)
			fprintf(stderr, _("no aliases defined\n"));
		else {
			while(li) {
				char *f;
				f = args_cat2(((alias *)li->data)->value, 0);
				printf("%s = '%s'\n", ((alias *)li->data)->name, f);
				xfree(f);
				li = li->next;
			}
		}
		return;
	}

	ap = alias_search(argv[optind]);

	if(argc == optind + 1) {  /* print one alias binding */
		if(!ap)
			fprintf(stderr, _("no such alias '%s'\n"), argv[optind]);
		else if(ap == ALIAS_AMBIGUOUS)
			fprintf(stderr, _("ambiguous alias '%s'\n"), argv[optind]);
		else {
			char *f;
			f = args_cat2(ap->value, 0);
			printf("%s = '%s'\n", ap->name, f);
			xfree(f);
		}
		return;
	}

	/* define a new alias ... */

	{
		args_t *args;

		args = args_create();
		args_push_back(args, argv[optind + 1]);
/*		args_init2(args, argc, argv, 3);*/
		alias_define(argv[optind], args);
	}
}

void cmd_unalias(int argc, char **argv)
{
	int i;
	listitem *lip;

	OPT_HELP("Undefine an alias.  Usage:\n"
			 "  unalias [options] <alias>\n"
			 "Options:\n"
			 "  -h, --help    show this help\n");

	minargs(optind);

	if(list_numitem(gvAliases) == 0) {
		fprintf(stderr, _("no aliases defined\n"));
		return;
	}

	if(strcmp(argv[optind], "*") == 0)
		list_clear(gvAliases);
	else {
		for(i=optind; i<argc; i++) {
			lip = list_search(gvAliases, (listsearchfunc)alias_searchfunc,
							  argv[i]);
			if(!lip) {
				fprintf(stderr, _("no such alias '%s'\n"), argv[i]);
				continue;
			}
			list_delitem(gvAliases, lip);
		}
	}
}
