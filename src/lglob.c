/* lglob.c -- local glob functions
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
#include "strq.h"
#include "linklist.h"
#include "lglob.h"
#include "gvars.h"

list *lglob_create(void)
{
	list *gl;
	gl = list_new((listfunc)xfree);
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
 * and that file is excluded if EXCLUDE_FUNC returns true
 * returns 0 if successful, -1 if failure
 */
int lglob_glob(list *gl, const char *mask, lglobfunc exclude_func)
{
	int i;
	char **result;
	char tmp[PATH_MAX];

	result = glob_filename((char *)mask);
	if(result == 0) {
		fprintf(stderr, "out of memory\n");
		return -1;
	}
	if(result == &glob_error_return) {
		perror(mask);
		return -1;
	}
	if(!*result) {
		fprintf(stderr, "%s: no matches found\n", mask);
		return 1;
	}

	getcwd(tmp, PATH_MAX);
	for(i=0; result[i]; i++) {
		char *p;

		if(exclude_func && exclude_func(result[i]))
			continue;

		p = path_absolute(result[i], tmp, gvLocalHomeDir);
		list_additem(gl, p);
		xfree(result[i]);
	}
	xfree(result);

#if 0	
	glob_t glb;
	/* GLOB_PERIOD is GNU extension */
	int glob_flags = GLOB_NOSORT | GLOB_PERIOD;

	memset(&glb, 0, sizeof(glb));

	if(glob(mask, glob_flags, 0, &glb) != 0) {
		fprintf(stderr, "%s: %s\n", mask, strerror(errno));
		return -1;
	}

	if(glb.gl_pathc == 0) {
		fprintf(stderr, "%s: no matches found\n", mask);
		globfree(&glb);
		return 1;
	}

	getcwd(tmp, PATH_MAX);
	for(i=0; i<glb.gl_pathc; i++) {
		char *p;

		if(exclude_func && exclude_func(glb.gl_pathv[i]))
			continue;

		p = path_absolute(glb.gl_pathv[i], tmp, gvLocalHomeDir);
		list_additem(gl, p);
	}

	globfree(&glb);
#endif
	return 0;
}
