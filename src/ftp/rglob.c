/* $Id: rglob.c,v 1.5 2002/12/02 12:24:39 mhe Exp $
 *
 * rglob.c -- remote glob functions
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
#include "strq.h"

list *rglob_create(void)
{
	list *gl;
	gl = list_new((listfunc)rfile_destroy);
	return gl;
}

void rglob_destroy(list *gl)
{
	list_free(gl);
}

bool rglob_exclude_dotdirs(rfile *f)
{
	return risdotdir(f);
}

static bool contains_wildcards(char *str)
{
	return (strqchr(str, '*') != 0 || strqchr(str, '?') != 0
			|| strqchr(str, '[') != 0 || strqchr(str, ']') != 0);
}

/* appends rglob items in list LP matching MASK
 * EXCLUDE_FUNC (if not 0) is called for each fileinfo item found
 * and that file is excluded if EXCLUDE_FUNC returns true
 * returns 0 if successful, -1 if failure (no files found)
 *
 * if ignore_multiples is true, a file isn't added to the list
 * if it already exists in the list
 *
 * any spaces or other strange characters in MASK should be backslash-quoted
 */
int rglob_glob(list *gl, const char *mask, bool cpifnomatch,
			   bool ignore_multiples, rglobfunc exclude_func)
{
	char *path;
	char *dep, *mp;
	rdirectory *rdir;
	listitem *lip;
	rfile *fi = 0, *nfi;
	char *d;
	int found = 0;
	int before = list_numitem(gl);

	path = tilde_expand_home(mask, ftp->homedir);
	dep = strrchr(path, '/');
	if(!dep)
		dep = path;
	else dep++;
	mp = xstrdup(dep);
	if(mp)
		unquote(mp);
	/* note: mp might be NULL here, treat it like mp == "*" */

	/* read the directory */
	d = base_dir_xptr(path);
	if(!d) d = xstrdup(ftp->curdir);
	else unquote(d);

	rdir = ftp_get_directory(d);
	xfree(d);

	if(rdir) {
		lip = rdir->files->first;
		while(lip) {
			fi = (rfile *)lip->data;
			lip = lip->next;

			/* check if the mask includes this file */
			if(mp == 0 || fnmatch(mp, base_name_ptr(fi->path), 0)
			   != FNM_NOMATCH)
			{
				bool ignore_item;

				found++;

				/* call the exclude function, if any, and skip file
					if the function returns true
				*/
				if(exclude_func && exclude_func(fi))
					ignore_item = true;
				else
					ignore_item =
						(ignore_multiples &&
						 (list_search(gl, (listsearchfunc)rfile_search_path,
										  fi->path) != 0));

				if(!ignore_item) {
					nfi = rfile_clone(fi);
					list_additem(gl, (void *)nfi);
				} else
					ftp_trace("ignoring file '%s'\n", fi->path);
			}
		}
	}

	if(found == 0) {
		char *p;
		bool ignore_item;

		if(!cpifnomatch || mp == 0 || *mp == 0 || contains_wildcards(mp)) {
			xfree(mp);
			return -1;
		}
		p = ftp_path_absolute(path);
		unquote(p);

		/* disallow multiples of the same file */
		ignore_item =
			(ignore_multiples &&
			 (list_search(gl, (listsearchfunc)rfile_search_path,
						  p)) != 0);

		if(!ignore_item) {
			nfi = rfile_create();
			rfile_fake(nfi, p);
			list_additem(gl, (void *)nfi);
		}
		xfree(p);
	}

	xfree(mp);
	xfree(path);
	return 0;
}

/* computes the total size (in bytes) of the files in (rglob list) GL */
unsigned long rglob_size(list *gl)
{
	listitem *li;
	unsigned long size = 0;

	li = gl->first;
	while(li) {
		size += ((rfile *)li->data)->size;
		li = li->next;
	}
	return size;
}

/* returns number of directories in GL */
unsigned long rglob_numdirs(list *gl)
{
	listitem *li;
	unsigned long n = 0;

	li = gl->first;
	while(li) {
		n += risdir((rfile *)li->data);
		li = li->next;
	}
	return n;
}
