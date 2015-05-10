/*
 * cache.c -- remote directory cache
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

#include "ftp.h"
#include "xmalloc.h"
#include "strq.h"
#include "gvars.h"

void ftp_cache_list_contents(void)
{
  ftp_cache_flush();

  printf("Directory cache contents: (%u items)\n",
       list_numitem(ftp->cache));

  for (listitem* li = ftp->cache->first; li; li = li->next)
    printf("%s\n", ((rdirectory *)li->data)->path);
}

/* marks the directory PATH to be flushed in the
 * next call to ftp_cache_flush()
 */
void ftp_cache_flush_mark(const char *path)
{
  char *p = ftp_path_absolute(path);
  stripslash(p);

  if(list_search(ftp->dirs_to_flush, (listsearchfunc)strcmp, p) != 0)
  {
    free(p);
    /* already in the flush list */
    return;
  }

  list_additem(ftp->dirs_to_flush, p);
  ftp_trace("marked directory '%s' for flush\n", p);
}

/* marks the directory *containing* PATH to be flushed in
 * the next call to ftp_cache_flush()
 */
void ftp_cache_flush_mark_for(const char *path)
{
  char* e = base_dir_xptr(path);
  if (!e)
    e = xstrdup(ftp->curdir);
  ftp_cache_flush_mark(e);
  free(e);
}

static int cache_search(rdirectory *rdir, const char *arg)
{
  return strcmp(rdir->path, arg);
}

/* flushes directories marked by ftp_cache_flush_mark*()
 */
void ftp_cache_flush(void)
{
  for (listitem* li = ftp->dirs_to_flush->first; li; li = li->next) {
    char* dir = li->data;
    listitem* e = list_search(ftp->cache, (listsearchfunc)cache_search, dir);
    if (e) {
      ftp_trace("flushed directory '%s'\n", dir);

      list_delitem(ftp->cache, e);
    } else
      ftp_trace("error flushing directory '%s' (not cached)\n", dir);
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
  char *dir_to_search_for = NULL;
  if(path)
    dir_to_search_for = ftp_path_absolute(path);
  else
    dir_to_search_for = xstrdup(ftp->curdir);

  listitem* li = list_search(ftp->cache, (listsearchfunc)cache_search,
           dir_to_search_for);

  if(li && gvCacheTimeout &&
     ((rdirectory *)li->data)->timestamp + gvCacheTimeout <= time(0))
  {
    /* directory cache has timed out */

    ftp_trace("Directory cache for '%s' has timed out\n",
          dir_to_search_for);
    ftp_cache_flush_mark(dir_to_search_for);
  }
  free(dir_to_search_for);

  if(!li)
    return NULL;

  return li->data;
}

/* returns a file from the cache, or 0 if not found
 */
rfile *ftp_cache_get_file(const char *path)
{
  if (!path)
    return NULL;

  char* dir = base_dir_xptr(path);
  rdirectory* rdir = ftp_cache_get_directory(dir);
  free(dir);

  if (rdir)
    return rdir_get_file(rdir, base_name_ptr(path));

  return NULL;
}
