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

static char *chop(char *str, unsigned maxlen)
{
	int len = strlen(str);

	if(len <= maxlen)
		return str;

	strcpy(str, "...");
	strcat(str, str+(len-maxlen+3));
	return str;
}

static void dirtodots(char *path, unsigned maxlen)
{
	char *first_slash, *end_slash;

	first_slash = strchr(path, '/');

	if(!first_slash) {
		chop(path, maxlen);
		return;
	}

	if(strncmp(first_slash+1, "...", 3) == 0) {
		end_slash = strchr(first_slash+5, '/');
	} else
		end_slash = strchr(first_slash+1, '/');

	if(!end_slash) {
		chop(path, maxlen);
		return;
	}

	if(end_slash - first_slash < 4) /* /fu/ */
		strpush(first_slash+1, 4 - (end_slash - first_slash));
	else /* /foobar/ */
		strcpy(first_slash + 4, end_slash);
	strncpy(first_slash+1, "...", 3);
}

static char *_shortpath(char *path, unsigned maxlen)
{
	static char *tmp = NULL;
	int len = strlen(path);

	tmp = realloc(tmp, len+1);
	strcpy(tmp, path);

	if(len <= maxlen)
		return tmp;

	dirtodots(tmp, maxlen);
	return _shortpath(tmp, maxlen);
}

/* returns a static buffer, overwritten with each call */
char *shortpath(const char *path, unsigned maxlen, const char *home)
{
	char *e;
	static char *tmp = NULL;
	int len = strlen(path);
	extern bool gvTilde;

	if(!path)
		return 0;

	tmp = realloc(tmp, len+1);
	strcpy(tmp, path);

	path_collapse(tmp);

	if(home && home[0] && strlen(home)>=3 && gvTilde) {
		if(strncmp(tmp, home, strlen(home)) == 0) {
			tmp[0] = '~';
			strpull(tmp+1, strlen(home)-1);
		}
	}

	if(maxlen <= 3)
		return tmp;

	e = _shortpath(tmp, maxlen);
	return e;
}

#ifdef TEST

bool gvTilde = true;
char *gvLocalHomeDir = 0;

int main(int argc, char **argv)
{
	char *e;

	if(argc<2) {
		puts("gruff");
		return 1;
	}
	printf("before: %s\n", argv[1]);
	e = shortpath(argv[1], 30, getenv("HOME"));
	printf("shortened to max 30 chars: %s\n", e);

	if(strlen(e) > 30)
		printf("FAILED!\n");

	return 0;
}

#endif
