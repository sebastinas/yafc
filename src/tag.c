/* tag.c -- tag files
 *
 * This file is part of Yafc, an ftp client.
 * This program is Copyright (C) 1998, 1999, 2000 Martin Hedenfalk
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "syshdr.h"
#include "rglob.h"
#include "gvars.h"
#include "strq.h"
#include "commands.h"
#include "input.h"
#include "utils.h"

static void print_tag_syntax(void)
{
	printf(_("Tag file(s) for later transferring.  Usage:\n"
			 "  tag [option(s)] [file(s)]\n"
			 "Options:\n"
			 "  -c, --clear             clear the taglist\n"
			 "  -i, --info              show info of tagged files\n"
			 "  -l, --list              list tagged files\n"
			 "  -L, --load[=FILE]       load saved taglist file for this host\n"
			 "  -s, --save[=FILE]       save taglist\n"
			 "  -h, --help              show this help\n"));
}

void save_taglist(const char *alt_filename)
{
	FILE *fp;
	char *f, *e = 0;
	listitem *li;

	if(!ftp_connected())
		return;

	if(list_numitem(ftp->taglist) == 0 || gvLoadTaglist == 0)
		return;

	if(alt_filename == 0)
		asprintf(&e, "%s/taglist.%s",
				 gvWorkingDirectory, ftp->host->ohostname);
	f = tilde_expand_home(alt_filename ? alt_filename : e, gvLocalHomeDir);
	fp = fopen(f, "w");
	if(!fp) {
		perror(_("Unable to save taglist file"));
		xfree(f);
		xfree(e);
		return;
	}

	fprintf(fp, "[yafc taglist]\n");
	for(li=ftp->taglist->first; li; li=li->next)
		fprintf(fp, "%s\n", ((rfile *)li->data)->path);
	fclose(fp);
	fprintf(stderr, _("Saved taglist to %s\n"), f);
	xfree(f);
	xfree(e);
}

void load_taglist(bool showerr, bool always_autoload,
				  const char *alt_filename)
{
	FILE *fp;
	char *f, *e = 0;
	unsigned n = 0;
	int c;
	char tmp[4096];

	if(gvLoadTaglist == 0 && !always_autoload)
		return;

	need_connected();
	need_loggedin();

	if(alt_filename == 0)
		asprintf(&e, "%s/taglist.%s",
				 gvWorkingDirectory, ftp->host->ohostname);
	f = tilde_expand_home(alt_filename ? alt_filename : e, gvLocalHomeDir);
	fp = fopen(f, "r");
	if(!fp) {
		if(showerr) {
			if(alt_filename)
				perror(alt_filename);
			else
				fprintf(stderr, _("No saved taglist for %s\n"),
						ftp->host->ohostname);
		}
		xfree(e);
		xfree(f);
		return;
	}

	if(gvLoadTaglist == 2 && !always_autoload) {
		c = ask(ASKYES|ASKNO, ASKYES, _("Found saved taglist, load it now?"));
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
		rfile *rf;

		if(fgets(tmp, 4096, fp) == 0)
			break;
		strip_trailing_chars(tmp, "\r\n");
		if(tmp[0] == 0)
			continue;
		rf = ftp_get_file(tmp);
		if(rf == 0) {
			rf = rfile_create();
			rfile_fake(rf, tmp);
		}
		list_additem(ftp->taglist, rfile_clone(rf));
		n++;
	}
	if(n)
		fprintf(stderr, "Loaded %u files from saved taglist %s\n", n, f);
	xfree(e);
	fclose(fp);
	if(alt_filename == 0)
		unlink(f);
	xfree(f);
}


static void show_taglist(void)
{
	listitem *li;

	if(list_numitem(ftp->taglist) == 0) {
		fprintf(stderr, _("nothing tagged -- use 'tag' to tag files\n"));
		return;
	}

	for(li=ftp->taglist->first; li; li=li->next)
		printf("%s\n", ((rfile *)li->data)->path);
}


static void show_taglist_info(void)
{
	unsigned long tlsize;

	if(list_numitem(ftp->taglist) == 0) {
		fprintf(stderr, _("nothing tagged -- use 'tag' to tag files\n"));
		return;
	}
	tlsize = rglob_size(ftp->taglist);
	printf(_("%lu bytes (%s) in %u files tagged\n"),
		   tlsize, human_size(tlsize), list_numitem(ftp->taglist));
	printf(_("%lu directories tagged\n"), rglob_numdirs(ftp->taglist));
}

void cmd_tag(int argc, char **argv)
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
			list_clear(ftp->taglist);
			return;
		  case 's':
			save_taglist(optarg);
			return;
		  case 'l':
			show_taglist();
			return;
		  case 'L':
			load_taglist(true, true, optarg);
			return;
		  case 'i':
			show_taglist_info();
			return;
		  case 'h':
			print_tag_syntax();
			/* fall through */
		  case '?':
			return;
		}
	}

	need_connected();
	need_loggedin();

	if(argc == 1) {
#if defined(HAVE_LIBNEWT) && 0
		newt_tag();
#else
		minargs(1);
#endif
	} else {
		for(i=1;i<argc;i++) {
			stripslash(argv[i]);
			rglob_glob(ftp->taglist, argv[i], true, true, 0);
		}
	}
}

void cmd_untag(int argc, char **argv)
{
	int i, num;
	listitem *li, *pli;
	rfile *f;

	OPT_HELP("Remove files from the tag list.  Usage:\n"
			 "  untag [options] <filemask>...\n"
			 "Options:\n"
			 "  -h, --help    show this help\n");

	minargs(optind);
	need_connected();
	need_loggedin();

	if(list_numitem(ftp->taglist)==0) {
		puts(_("nothing tagged -- use 'tag' to tag files"));
		return;
	}

	for(i=1;i<argc;i++) {
		num = list_numitem(ftp->taglist);
		li = ftp->taglist->first;
		while(li) {
			f = (rfile *)li->data;
			pli = li;
			li = li->next;
			if(fnmatch(argv[i], f->path, FNM_PATHNAME|FNM_PERIOD) != FNM_NOMATCH) {
				printf("%s\n", f->path);
				list_delitem(ftp->taglist, pli);
			}
		}
		if(list_numitem(ftp->taglist) == num)
			printf(_("%s: no matches found\n"), argv[i]);
	}
}
