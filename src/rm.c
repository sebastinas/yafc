/* $Id: rm.c,v 1.3 2001/05/12 18:44:37 mhe Exp $
 *
 * rm.c -- remove files, the 'rm' command
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
#include "ftp.h"
#include "gvars.h"
#include "shortpath.h"
#include "strq.h"
#include "input.h"
#include "commands.h"

#define RM_INTERACTIVE 1
#define RM_FORCE 2
#define RM_VERBOSE 4
#define RM_RECURSIVE 8
#define RM_TAGGED 16

static void print_rm_syntax(void)
{
	printf(_("Remove file(s).  Usage:\n"
			 "  rm [options] file ...\n"
			 "Options:\n"
			 "  -f, --force           never prompt\n"
			 "  -i, --interactive     prompt before any removal\n"
			 "  -r, --recursive       remove the contents of"
			 " directories recursively\n"
			 "                          CAREFUL!\n"
			 "  -t, --tagged          remove tagged files\n"
			 "  -v, --verbose         explain what is being done\n"
			 "      --help            display this help\n"));
}

static bool rm_quit = false;
static bool rm_batch = false;

static void remove_file(const rfile *f, unsigned opt)
{
	ftp_set_tmp_verbosity(vbNone);
	ftp_unlink(f->path);
	if(test(opt, RM_VERBOSE)) {
		fprintf(stderr, "%s", shortpath(f->path, 40, ftp->homedir));
		if(ftp->code != ctComplete)
			fprintf(stderr, ": %s", ftp_getreply(false));
		fprintf(stderr, "\n");
	}
}

static void remove_files(const list *gl, unsigned opt)
{
	listitem *li;

	for(li=gl->first; li && !rm_quit; li=li->next) {
		rfile *f = (rfile *)li->data;

		if(gvInterrupted)
			break;

		/* skip dotdirs */
		if(strcmp(base_name_ptr(f->path), ".") == 0
		   || strcmp(base_name_ptr(f->path), "..") == 0)
			continue;

		if(test(opt, RM_INTERACTIVE) && !rm_batch) {
			int a = ask(ASKYES|ASKNO|ASKCANCEL|ASKALL, ASKYES,
						_("Remove remote file '%s'?"),
						shortpath(f->path, 42, ftp->homedir));
			if(a == ASKNO)
				continue;
			if(a == ASKCANCEL) {
				rm_quit = true;
				break;
			}
			if(a == ASKALL)
				rm_batch = true;
			/* else a==ASKYES */
		}

		if(risdir(f)) {
			if(test(opt, RM_RECURSIVE)) {
				char *recurs_mask;
				list *rgl;

				rgl = rglob_create();
				asprintf(&recurs_mask, "%s/*", f->path);
				rglob_glob(rgl, recurs_mask, true, true, 0);
				if(list_numitem(rgl) > 0)
					remove_files(rgl, opt);
				rglob_destroy(rgl);
				xfree(recurs_mask);
				ftp_rmdir(f->path);
				if(test(opt, RM_VERBOSE)) {
					fprintf(stderr, "%s", shortpath(f->path, 40,
													ftp->homedir));
					if(ftp->code != ctComplete)
						fprintf(stderr, "%s", ftp_getreply(false));
					fprintf(stderr, "\n");
				}

			} else
				fprintf(stderr, _("%s  omitting directory\n"),
						shortpath(f->path, 40, ftp->homedir));
			continue;
		}
		remove_file(f, opt);
	}
}

void cmd_rm(int argc, char **argv)
{
	int c, opt=0;
	struct option longopts[] = {
		{"force", no_argument, 0, 'f'},
		{"interactive", no_argument, 0, 'i'},
		{"verbose", no_argument, 0, 'v'},
		{"recursive", no_argument, 0, 'r'},
		{"tagged", no_argument, 0, 't'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};
	list *gl;

	optind = 0; /* force getopt() to re-initialize */
	while((c = getopt_long(argc, argv, "fivrt", longopts, 0)) != EOF) {
		switch(c) {
		case 'f':
			opt |= RM_FORCE;
			opt &= ~RM_INTERACTIVE;
			break;
		case 'i':
			opt |= RM_INTERACTIVE;
			break;
		case 'v':
			opt |= RM_VERBOSE;
			break;
		case 'r':
			opt |= RM_RECURSIVE;
			break;
		case 't':
			opt |= RM_TAGGED;
			break;
		case 'h':
			print_rm_syntax();
			return;
		  case '?':
			return;
		}
	}

	if(!test(opt, RM_TAGGED))
		minargs(optind);

	need_connected();
	need_loggedin();

	gl = rglob_create();
	while(optind < argc) {
		stripslash(argv[optind]);
		rglob_glob(gl, argv[optind], true, true, 0);
		optind++;
	}

	if(list_numitem(gl) == 0 && !test(opt, RM_TAGGED)) {
		fprintf(stderr, _("no files found\n"));
		rglob_destroy(gl);
		return;
	}
	if(test(opt, RM_TAGGED)
	   && (!ftp->taglist || list_numitem(ftp->taglist) == 0)) {
		fprintf(stderr, _("no tagged files\n"));
		if(list_numitem(gl) == 0) {
			rglob_destroy(gl);
			return;
		}
	}
	rm_quit = false;
	rm_batch = test(opt, RM_FORCE);
	if(test(opt, RM_FORCE))
		opt &= ~RM_INTERACTIVE;

	remove_files(gl, opt);
	if(test(opt, RM_TAGGED)) {
		remove_files(ftp->taglist, opt);
		list_clear(ftp->taglist);
	}
}
