/*
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

void* xmalloc(size_t size)
{
  if (size == 0)
    return NULL;

  void* ptr = malloc(size);
  if (!ptr) {
    fprintf(stderr, "\nxmalloc(%zu): %s\n", size, strerror(errno));
    exit(errno);
  }
  memset(ptr, 0, size);  /* zero allocated memory */
  return ptr;
}

void* xrealloc(void* ptr, size_t size)
{
  if (!ptr)
    return xmalloc(size);

  void* new_ptr = realloc(ptr, size);
  if (!new_ptr) {
    free(ptr);
    fprintf(stderr, "\nxrealloc(%p, %zu): %s\n", ptr, size, strerror(errno));
    exit(errno);
  }
  return new_ptr;
}

char* xstrdup(const char *s)
{
	if (!s)
		return NULL;

  const size_t len = strlen(s) + 1;
  char* r = xmalloc(len);
  strlcpy(r, s, len);
  return r;
}

char* xstrndup(const char *s, size_t n)
{
  if (!s || !*s)
		return NULL;

  char* r = xmalloc(n + 1);
  strlcpy(r, s, n + 1);
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
