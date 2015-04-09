/*
 * shortpath.c -- shorten path
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
#include "xmalloc.h"
#include "strq.h"

/* examples:
 * /home/mhe/blablabla/asdf => ~/.../asdf
 * /usr/local/include/X11/foo/bar/tmp/junk.txt => /.../bar/tmp/junk.txt
 * averylongdirectoryname/filename.extension => .../filename.extension
 * averylongfilenamethatistoolongindeed.tar.gz => ...oolongindeed.tar.gz
 * /usr/bin/averylongfilenamethatistoolongindeed.tar.gz => /...oolongindeed.tar.gz
 */

static char *_shortpath(char *path, size_t maxlen)
{
	size_t len = strlen(path);
	char *result = NULL;
	const char *copy_from = NULL;
	const char *first_slash = NULL;
	size_t start_len = 0;

	if (len <= maxlen)
		return xstrdup(path);

	result = xmalloc(maxlen + 1);

	first_slash = strchr(path, '/');
	if (first_slash)
		start_len = first_slash - path + 1;

	if (maxlen - start_len > 3)
		copy_from = strchr(path + len - (maxlen - start_len - 3), '/');

	if (copy_from)
		strlcpy(result, path, start_len);
	else
		copy_from = path + len - (maxlen - 3);

	strlcat(result, "...", maxlen + 1);
	strlcat(result, copy_from, maxlen + 1);

	return result;
}

/* returns a malloced memory, needs to be deallocated with free */
char *shortpath(const char *path, size_t maxlen, const char *home)
{
	extern bool gvTilde;

	if (!path)
		return NULL;

	const size_t len = strlen(path);
	if (!len)
		return NULL;

	char* tmp = xstrdup(path);
	path_collapse(tmp);

	if(home && home[0] && strlen(home)>=3u && gvTilde) {
		if(strncmp(tmp, home, strlen(home)) == 0) {
			tmp[0] = '~';
			strpull(tmp+1, strlen(home)-1);
		}
	}

	if(maxlen <= 3)
		return tmp;

	char* res = _shortpath(tmp, maxlen);
	free(tmp);
	return res;
}
