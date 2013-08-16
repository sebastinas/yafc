/*
 * makepath.c -- recurcive mkdir
 *
 * Yet Another FTP Client
 * Copyright (C) 2013, Sebastian Ramacher <sebastian+dev@ramacher.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#include "utils.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_SYS_TYPES
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <errno.h>
#include <string.h>

bool make_path(const char* path)
{
  if (!path)
  {
    errno = EINVAL;
    return false;
  }

  struct stat stats;
  if (!stat(path, &stats))
  {
    /* path already exists */
    if (!S_ISDIR(stats.st_mode))
    {
      errno = EEXIST;
      return false;
    }
    else
      return true;
  }

  char* dirpath = strdup(path);
  if (!dirpath)
  {
    errno = ENOMEM;
    return false;
  }

  char* tmp = dirpath;
  while (*tmp == '/')
    ++tmp;

  while ((tmp = strchr(tmp, '/')))
  {
    *tmp = '\0';
    if (stat(dirpath, &stats))
    {
      if (mkdir(dirpath, S_IRWXU))
      {
        free(dirpath);
        return false;
      }
    }
    else if (!S_ISDIR(stats.st_mode))
    {
      free(dirpath);
      return false;
    }

    *tmp++ = '/';
    while (*tmp == '/')
      tmp++;
  }

  if (stat(dirpath, &stats) && mkdir(dirpath, S_IRWXU))
  {
    free(dirpath);
    return false;
  }

  free(dirpath);
  return true;
}
