/*
 * lscolors.c -- parsing of env variable LS_COLORS
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
#include "rfile.h"
#include "xmalloc.h"
#include "strq.h"

typedef struct {
	char *msk;
	char *clr;
} lscolor;

static bool colors_initialized = false;
static int number_of_colors = 0;

/* these are the default colors */
static char *diclr="01;34", *lnclr="01;36", *ficlr="00", *exclr="01;32";
static char *bdclr="40;01;33", *cdclr="40;01;33", *piclr="40;33", *soclr="01;35";
static char *lc="\x1B[", *rc="m", *ec="\x1B[0m";

static lscolor *clrs;

char *endcolor(void)
{
	return ec;
}

void init_colors(void)
{
	int i = 0;
	char *p, *e, *q, *org;
	unsigned int n;

	if(colors_initialized)
		return;

	p = getenv("LS_COLORS");
	if(!p)
		p = getenv("LS_COLOURS");

	if(p && *p) {
		org = p = xstrdup(p);
		n = strqnchr(p, ':')+2;
		clrs = (lscolor *)xmalloc(n * sizeof(lscolor));
		e = strqsep(&p, ':');
		while(e && *e) {
			q = strqsep(&e, '=');
			clrs[i].msk = xstrdup(q);
			q = strqsep(&e, ':');
			clrs[i].clr = xstrdup(q);

			e = strqsep(&p, ':');

			if(strcmp(clrs[i].msk, "di")==0) /* directory */
				diclr = clrs[i].clr;
			else if(strcmp(clrs[i].msk, "ln")==0) /* link */
				lnclr = clrs[i].clr;
			else if(strcmp(clrs[i].msk, "fi")==0) /* normal file */
				ficlr = clrs[i].clr;
			else if(strcmp(clrs[i].msk, "ex")==0) /* executable */
				exclr = clrs[i].clr;
			else if(strcmp(clrs[i].msk, "bd")==0) /* block device */
				bdclr = clrs[i].clr;
			else if(strcmp(clrs[i].msk, "cd")==0) /* character device */
				cdclr = clrs[i].clr;
			else if(strcmp(clrs[i].msk, "pi")==0) /* pipe */
				piclr = clrs[i].clr;
			else if(strcmp(clrs[i].msk, "so")==0) /* socket */
				soclr = clrs[i].clr;
			else if(strcmp(clrs[i].msk, "lc") == 0) /* left code */
				lc = clrs[i].clr;
			else if(strcmp(clrs[i].msk, "rc") == 0) /* right code */
				rc = clrs[i].clr;
			else if(strcmp(clrs[i].msk, "ec") == 0) /* end code (lc+fi+rc) */
				ec = clrs[i].clr;
			else
				i++;
		}
		number_of_colors = i;
		free(org);
		if(!ec) {
			ec = xmalloc(strlen(lc)+strlen(ficlr)+strlen(rc)+1);
			sprintf(ec, "%s%s%s", lc, ficlr, rc);
		}
	}
}

void rfile_parse_colors(rfile *f)
{
	char *e, *q;
	int i;

	/* colors should already been initialized by init_colors */

	e = 0;
	if(risdir(f))
		e = diclr;
	else if(rislink(f))
		e = lnclr;
	else if(rischardev(f))
		e = cdclr;
	else if(risblockdev(f))
		e = bdclr;
	else if(rispipe(f))
		e = piclr;
	else if(rissock(f))
		e = soclr;

	/* do this before checking for executable, because
	 * on [v]fat filesystems, all files are 'executable'
	 */
	for(i=0;i<number_of_colors && !e;i++) {
		if(fnmatch(clrs[i].msk, base_name_ptr(f->path), 0) == 0)
			e = clrs[i].clr;
	}

	/* a file is considered executable if there is
	 * an 'x' anywhere in its permission string */
	if(!e && risexec(f))
		e = exclr;

	q = e ? e : ficlr;
	f->color = (char *)xmalloc(strlen(lc) + strlen(q) + strlen(rc) + 1);
	sprintf(f->color, "%s%s%s", lc, q, rc);
}
