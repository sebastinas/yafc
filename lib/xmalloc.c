/* $Id: xmalloc.c,v 1.4 2003/07/12 10:25:41 mhe Exp $
 *
 * xmalloc.c -- wrapper for malloc etc.
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

void *xmalloc(size_t size)
{
    void *ptr;

	if(size == 0)
		return 0;

    ptr = malloc(size);
	if(ptr == 0) {
		fprintf(stderr, "\nxmalloc(%lu): %s\n",
				(unsigned long)size, strerror(errno));
		exit(errno);
    }
	memset(ptr, 0, size);  /* zero allocated memory */
    return ptr;
}

void *xrealloc(void *ptr, size_t size)
{
    void *new_ptr;

	if(ptr == 0)
		return xmalloc(size);

    new_ptr = realloc(ptr, size);
	if(new_ptr == 0) {
		fprintf(stderr,
				"\nxrealloc(%p, %lu): %s\n",
				ptr, (unsigned long)size, strerror(errno));
		exit(errno);
	}
	return new_ptr;
}

char *xstrdup(const char *s)
{
    char *r;

	if(s == 0 || *s == 0)
		return 0;

    r = (char *)xmalloc(strlen(s) + 1);
    strcpy(r, s);
    return r;
}

char *xstrndup(const char *s, size_t n)
{
    char *r;

	if(s == 0 || *s == 0)
		return 0;

    r = (char *)xmalloc(n + 1);
    strncpy(r, s, n);
    return r;
}

char *xstrncpy(char *dest, const char *src, size_t n)
{
	strncpy(dest, src, n);
	dest[n] = '\0';
	return dest;
}

int xstrcmp(const char *a, const char *b)
{
	if(a && !b)
		return 1;
	if(!a && b)
		return -1;
	if(!a && !b)
		return 0;
	return strcmp(a, b);
}
