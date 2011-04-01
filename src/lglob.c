/* $Id: lglob.c,v 1.8 2004/05/20 11:10:52 mhe Exp $
 *
 * lglob.c -- local glob functions
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
#include "strq.h"
#include "linklist.h"
#include "lglob.h"
#include "gvars.h"

#if defined (HAVE_DIRENT_H)
#  include <dirent.h>
#else /* !HAVE_DIRENT_H */
#  if defined (HAVE_SYS_NDIR_H)
#    include <sys/ndir.h>
#  endif
#  if defined (HAVE_SYS_DIR_H)
#    include <sys/dir.h>
#  endif /* HAVE_SYS_DIR_H */
#  if defined (HAVE_NDIR_H)
#    include <ndir.h>
#  endif
#  if !defined (dirent)
#    define dirent direct
#  endif
#endif /* !HAVE_DIRENT_H */

list *lglob_create(void)
{
	list *gl;
	gl = list_new((listfunc)free);
	return gl;
}

void lglob_destroy(list *gl)
{
	list_free(gl);
}

bool lglob_exclude_dotdirs(char *f)
{
	const char *e = base_name_ptr(f);
	return f == 0 || (strcmp(e, ".") == 0 || strcmp(e, "..") == 0);
}

/* appends char * items in list LP matching MASK
 * EXCLUDE_FUNC (if not 0) is called for each path found
 * and that path is excluded if EXCLUDE_FUNC returns true
 *
 * returns 0 if successful, -1 if failure
 */
int lglob_glob(list *gl, const char *mask, bool ignore_multiples,
			   lglobfunc exclude_func)
{
	struct dirent *de;
	DIR *dp;
	char *directory;
	char *tmp;
	bool added = false, found = false;

	directory = base_dir_xptr(mask);

	if((dp = opendir(directory ? directory : ".")) == 0) {
		ftp_err("Unable to read directory %s\n", directory ? directory : ".");
		return -1;
	}

	tmp = getcwd(NULL, 0);
	if (tmp == (char *)NULL) {
		ftp_err("getcwd(): %s\n", strerror(errno));
		return -1;
	}

	while((de = readdir(dp)) != 0) {
		char *path;

		asprintf(&path, "%s/%s", directory ? directory : ".", de->d_name);

		if(fnmatch(base_name_ptr(mask), de->d_name, 0) == 0) {
			if(!(exclude_func && exclude_func(path))) {
				char *p;
				bool ignore_item;

				p = path_absolute(path, tmp, gvLocalHomeDir);

				ignore_item = 
					(ignore_multiples &&
					 (list_search(gl, (listsearchfunc)strcmp, p) != 0));

				if(!ignore_item) {
					list_additem(gl, p);
					added = true;
				}
			}
			found = true;
		}
		free(path);
	}
	closedir(dp);

	if(!found) {
		ftp_err("%s: no matches found\n", mask);
		return -1;
	}

	return added ? 0 : -1;
}
