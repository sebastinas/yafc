/*
 * rfile.h -- representation of a remote file
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _rfile_h_included
#define _rfile_h_included

#include "syshdr.h"

typedef struct rfile
{
  char *perm;
  char *owner;
  char *group;
  char *color;
  char *date;         /* date and time as a string */
  time_t mtime;       /* modification time */
  unsigned int nhl;   /* number of hard links */
  char *link;         /* target of link */
  char *path;         /* filename with absolute path */
  unsigned long long int size;
} rfile;

rfile* rfile_create(void);
void rfile_clear(rfile *f);
void rfile_destroy(rfile *f);
rfile* rfile_clone(const rfile *f);

bool risdir(const rfile *f);
bool risdotdir(const rfile *f);
bool risreg(const rfile *f);
bool rispipe(const rfile *f);
bool risexec(const rfile *f);
bool rissock(const rfile *f);
bool rischardev(const rfile *f);
bool risblockdev(const rfile *f);
bool rislink(const rfile *f);

void rfile_fake(rfile *f, const char *path);
int rfile_parse(rfile *f, char *str, const char *dirpath, bool is_mlsd);
void rfile_parse_colors(rfile *f);

int month_number(const char *str);
void rfile_parse_time(rfile *f, const char *m, const char *d, const char *y);

char rfile_classchar(const rfile *f);
mode_t rfile_getmode(const rfile *f);

char *endcolor(void);

mode_t str2mode_t(const char *p);

int rfile_search_filename(rfile *f, const char *filename);
int rfile_search_path(rfile *f, const char *path);

char *time_to_string(time_t t);

#endif
