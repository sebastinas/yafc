/* cache.c -- remote directory cache
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
#include "xmalloc.h"
#include "strq.h"

void ftp_cache_list_contents(void)
{
	listitem *li = ftp->cache->first;

	printf("Directory cache contents: (%u items)\n",
		   list_numitem(ftp->cache));

	while(li) {
		printf("%s\n", ((rdirectory *)li->data)->path);
		li = li->next;
	}
}

/* marks the directory PATH to be flushed in the
 * next call to  ftp_cache_flush()
 */
void ftp_cache_flush_mark(const char *path)
{
	char *p = ftp_path_absolute(path);
	stripslash(p);

	if(list_search(ftp->dirs_to_flush, (listsearchfunc)strcmp, p) != 0)
		/* already in the flush list */
		return;

	list_additem(ftp->dirs_to_flush, p);
	ftp_trace("marked directory '%s' for flush\n", p);
}

/* marks the directory *containing* PATH to be flushed in
 * the next call to ftp_cache_flush()
 */
void ftp_cache_flush_mark_for(const char *path)
{
	char *p;
	char *e = base_dir_xptr(path);
	if(!e) {
		xfree(e);
		e = xstrdup(ftp->curdir);
	}
	p = ftp_path_absolute(e);
	stripslash(p);
	xfree(e);

	if(list_search(ftp->dirs_to_flush, (listsearchfunc)strcmp, p) != 0)
		/* already in the flush list */
		return;

	list_additem(ftp->dirs_to_flush, p);
	ftp_trace("marked directory '%s' for flush\n", p);
}

static int cache_search(rdirectory *rdir, const char *arg)
{
	return strcmp(rdir->path, arg);
}

/* flushes directories marked by ftp_cache_flush_mark*()
 */
void ftp_cache_flush(void)
{
	listitem *e;
	listitem *li = ftp->dirs_to_flush->first;

	while(li) {
		char *dir = (char *)li->data;
		e = list_search(ftp->cache, (listsearchfunc)cache_search, dir);
		if(e) {
			ftp_trace("flushed directory '%s'\n", dir);
			list_delitem(ftp->cache, e);
		} else
			ftp_trace("error flushing directory '%s' (not cached)\n", dir);

		li = li->next;
	}

	list_clear(ftp->dirs_to_flush);
}

void ftp_cache_clear(void)
{
	list_clear(ftp->dirs_to_flush);
	list_clear(ftp->cache);
	ftp_trace("clear whole directory cache\n");
}

/* PATH is NOT quoted */
/* returns a directory from the cache, or 0 if not found
 */
rdirectory *ftp_cache_get_directory(const char *path)
{
	listitem *li;
	char *dir_to_search_for;

	if(path)
		dir_to_search_for = ftp_path_absolute(path);
	else
		dir_to_search_for = xstrdup(ftp->curdir);

	li = list_search(ftp->cache, (listsearchfunc)cache_search,
					 dir_to_search_for);
/*	if(li)
		ftp_trace("cache hit: '%s'\n", dir_to_search_for);*/
	xfree(dir_to_search_for);
	if(!li)
		return 0;
	return (rdirectory *)li->data;
}

/* returns a file from the cache, or 0 if not found
 */
rfile *ftp_cache_get_file(const char *path)
{
	rdirectory *rdir;
	rfile *f;
	char *dir;

	if(!path)
		return 0;

	dir = base_dir_xptr(path);
	rdir = ftp_cache_get_directory(dir);
	xfree(dir);

	if(rdir) {
		f = rdir_get_file(rdir, base_name_ptr(path));
		return f;
	}
	return 0;
}
