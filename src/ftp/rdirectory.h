/* $Id: rdirectory.h,v 1.4 2002/11/04 14:02:39 mhe Exp $
 *
 * rdirectory.h -- representation of a remote directory
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _rdirectory_h_included
#define _rdirectory_h_included

#include "syshdr.h"
#include "rfile.h"
#include "linklist.h"

typedef struct rdirectory
{
	char *path;        /* directory path */
	list *files;       /* linked list of rfiles */
	time_t timestamp;  /* time of creation */
} rdirectory;

rdirectory *rdir_create(void);
void rdir_destroy(rdirectory *rdir);
int rdir_parse(rdirectory *rdir, FILE *fp, const char *path);
rfile *rdir_get_file(rdirectory *rdir, const char *filename);
unsigned long int rdir_size(rdirectory *rdir);


#endif
