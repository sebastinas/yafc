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

static char* chop(const char *str, size_t maxlen)
{
	const size_t len = strlen(str);
	if (len <= maxlen)
		return xstrdup(str);

	char* result = malloc(maxlen + 1);
	strncpy(result, "...", 3);
	strncpy(result + 3, str+(len-maxlen+3), maxlen - 3);
	result[maxlen] = '\0';
	return result;
}

static char* dirtodots(const char *path, size_t maxlen)
{
	const char* first_slash = strchr(path, '/');
	if (!first_slash)
		return chop(path, maxlen);

	const char* end_slash = NULL;
	if (strncmp(first_slash+1, "...", 3) == 0)
		end_slash = strchr(first_slash+5, '/');
	else
		end_slash = strchr(first_slash+1, '/');

	if (!end_slash)
		return chop(path, maxlen);

	size_t first = first_slash - path,
				 end = end_slash - path;
	char* result = xstrdup(path);
	if (end - first < 4) /* /fu/ */
		strpush(result + first +1, 4 - (end - first));
	else /* /foobar/ */
		strcpy(result + first + 4, end_slash);
	strncpy(result + first + 1, "...", 3);
	return result;
}

static char *_shortpath(char *path, size_t maxlen)
{
	const size_t len = strlen(path);
	if (len <= maxlen)
		return xstrdup(path);

	char* tmp = dirtodots(path, maxlen);
	char* result = _shortpath(tmp, maxlen);
	free(tmp);
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
