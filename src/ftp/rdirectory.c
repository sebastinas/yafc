/* rdirectory.c -- representation of a remote directory
 *
 * This file is part of Yafc, an ftp client.
 * This program is Copyright (C) 1998-2001 martin HedenfaLk
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
#include "ftp.h"
#include "strq.h"

rdirectory *rdir_create(void)
{
	rdirectory *rdir;
	
	rdir = (rdirectory *)xmalloc(sizeof(rdirectory));
	rdir->files = list_new((listfunc)rfile_destroy);

	return rdir;
}

void rdir_destroy(rdirectory *rdir)
{
	if(!rdir)
		return;
	list_free(rdir->files);
	xfree(rdir->path);
	xfree(rdir);
}

unsigned long int rdir_size(rdirectory *rdir)
{
	return rglob_size(rdir->files);
}

int rdir_parse(rdirectory *rdir, FILE *fp, const char *path)
{
	char tmp[257];
	rfile *f;
	int r;
	bool failed = false;

	xfree(rdir->path);
	rdir->path = 0;
	list_clear(rdir->files);

	f = rfile_create();
	
	ftp_trace("*** start parsing directory listing ***\n");

	while(!feof(fp)) {
		if(fgets(tmp, 256, fp) == 0)
			break;
		strip_trailing_chars(tmp, "\r\n");
		if(tmp[0] == 0)
			break;
		ftp_trace("%s\n", tmp);

		if(!failed) {
			rfile_clear(f);
			r = rfile_parse(f, tmp, path);
			if(r == -1) {
				ftp_err("parsing failed on '%s'\n", tmp);
				list_clear(rdir->files);
				failed = true;
			} else if(r == 0)
				list_additem(rdir->files, (void *)rfile_clone(f));
			/* else r == 1, ie a 'total ###' line, which isn't an error */
		}
	}
	ftp_trace("*** end parsing directory listing ***\n");
	if(failed) {
		ftp_err("directory parsing failed\n");
		return -1;
	}
/*	if(list_numitem(rdir->files) == 0)
		return -1;*/
	rdir->path = xstrdup(path);
	return 0;
}

rfile *rdir_get_file(rdirectory *rdir, const char *filename)
{
	listitem *li;

	li = list_search(rdir->files,
					 (listsearchfunc)rfile_search_filename, filename);
	if(li)
		return (rfile *)li->data;
	return 0;
}
