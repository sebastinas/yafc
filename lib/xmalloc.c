/* xmalloc.c -- wrapper for malloc etc.
 * 
 * This file is part of Yafc, an ftp client.
 * This program is Copyright (C) 1998-2001 martin HedenfaLk.
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

void xfree(void *ptr)
{
	if(ptr)
		free(ptr);
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
