/*
 * ltag.c -- tag local files
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
#include "gvars.h"
#include "strq.h"
#include "commands.h"
#include "lglob.h"
#include "bashline.h"
#include "input.h"

static void print_ltag_syntax(void)
{
	printf(_("Tag local file(s) for later transferring.  Usage:\n"
			 "  ltag [option(s)] file(s)\n"
			 "Options:\n"
			 "  -c, --clear             clear the local taglist\n"
			 "  -i, --info              show info of tagged files\n"
			 "  -l, --list              list tagged files\n"
			 "  -L, --load[=FILE]       load saved local taglist file\n"
			 "  -s, --save[=FILE]       save local taglist\n"
			 "  -h, --help              show this help\n"));
}

void save_ltaglist(const char *alt_filename)
{
	FILE *fp;
	char *f, *e = 0;
	listitem *li;

	if(list_numitem(gvLocalTagList) == 0 || gvLoadTaglist == 0)
		return;

	if(alt_filename == 0)
		asprintf(&e, "%s/taglist.local", gvWorkingDirectory);
	f = tilde_expand_home(alt_filename ? alt_filename : e, gvLocalHomeDir);
	free(e);
	fp = fopen(f, "w");
	if(!fp) {
		perror(_("Unable to save local taglist file"));
		free(f);
		return;
	}

	fprintf(fp, "[yafc taglist]\n");
	for(li=gvLocalTagList->first; li; li=li->next)
		fprintf(fp, "%s\n", (char *)li->data);
	fclose(fp);
	fprintf(stderr, _("Saved local taglist to %s\n"), f);
	free(f);
}

void load_ltaglist(bool showerr, bool always_autoload,
				   const char *alt_filename)
{
	FILE *fp;
	char *f, *e = 0;
	unsigned n = 0;
	int c;
	char tmp[4096];

	if(gvLoadTaglist == 0 && !always_autoload)
		return;

	if(alt_filename == 0)
		asprintf(&e, "%s/taglist.local", gvWorkingDirectory);
	f = tilde_expand_home(alt_filename ? alt_filename : e, gvLocalHomeDir);
	free(e);
	fp = fopen(f, "r");
	if(!fp) {
		if(showerr) {
			if(alt_filename)
				perror(alt_filename);
			else
				fprintf(stderr, _("No saved local taglist\n"));
		}
		free(f);
		return;
	}

	if(gvLoadTaglist == 2 && !always_autoload) {
		c = ask(ASKYES|ASKNO, ASKYES,
				_("Found saved local taglist, load it now?"));
		if(c == ASKNO)
			return;
	} /* else gvLoadTaglist == 1 == yes */

	if(fgets(tmp, 4096, fp) != 0) {
		strip_trailing_chars(tmp, "\n\r");
		if(strcasecmp(tmp, "[yafc taglist]") != 0) {
			fprintf(stderr, "Not a Yafc taglist file: %s\n", f);
			fclose(fp);
			return;
		}
	}

	while(true) {
		if(fgets(tmp, 4096, fp) == 0)
			break;
		strip_trailing_chars(tmp, "\r\n");
		if(tmp[0] == 0)
			continue;
		list_additem(gvLocalTagList, xstrdup(tmp));
		n++;
	}
	if(n)
		fprintf(stderr, "Loaded %u files from saved local taglist %s\n", n, f);
	fclose(fp);
	if(alt_filename == 0)
		unlink(f);
	free(f);
}

static void show_ltaglist(void)
{
	listitem *li;

	if(list_numitem(gvLocalTagList) == 0) {
		fprintf(stderr, _("nothing tagged -- use 'ltag' to tag files\n"));
		return;
	}

	for(li=gvLocalTagList->first; li; li=li->next)
		printf("%s\n", (char *)li->data);
}

static void show_ltaglist_info(void)
{
	int num = list_numitem(gvLocalTagList);
	if(num == 0) {
		fprintf(stderr, _("nothing tagged -- use 'ltag' to tag files\n"));
		return;
	}
	printf(_("%u files tagged\n"), num);
}

void cmd_ltag(int argc, char **argv)
{
	int i;
	int c;
	struct option longopts[] = {
		{"clear", no_argument, 0, 'c'},
		{"info", no_argument, 0, 'i'},
		{"list", no_argument, 0, 'l'},
		{"load", optional_argument, 0, 'L'},
		{"save", optional_argument, 0, 's'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};

	optind = 0;
	while((c = getopt_long(argc, argv, "ciL::lhs::", longopts, 0)) != EOF) {
		switch(c) {
		  case 'c':
			list_clear(gvLocalTagList);
			return;
		  case 's':
			save_ltaglist(optarg);
			return;
		  case 'l':
			show_ltaglist();
			return;
		  case 'L':
			load_ltaglist(true, true, optarg);
			return;
		  case 'i':
			show_ltaglist_info();
			return;
		  case 'h':
			print_ltag_syntax();
			/* fall through */
		  case '?':
			return;
		}
	}

	minargs(1);

	for(i=1;i<argc;i++) {
		stripslash(argv[i]);
		lglob_glob(gvLocalTagList, argv[i], true, &lglob_exclude_dotdirs);
	}
}

static int ltagcmp(const char *tag, const char *str)
{
	return (fnmatch(str, tag, 0) == FNM_NOMATCH) ? 1 : 0;
}

void cmd_luntag(int argc, char **argv)
{
	int i;

	OPT_HELP("Remove files from the local tag list.  Usage:\n"
			 "  luntag [options] <filemask>...\n"
			 "Options:\n"
			 "  -h, --help    show this help\n");


	minargs(1);

	if(list_numitem(gvLocalTagList) == 0) {
		printf(_("nothing tagged -- use 'ltag' to tag files\n"));
		return;
	}

	for(i=1;i<argc;i++) {
		bool del_done = false;
		listitem *li;

		while((li = list_search(gvLocalTagList,
								(listsearchfunc)ltagcmp,
								argv[i])) != 0)
		{
			printf("%s\n", (const char *)li->data);
			list_delitem(gvLocalTagList, li);
			del_done = true;
		}

		if(!del_done)
			printf(_("%s: no matches found\n"), argv[i]);
	}
}
