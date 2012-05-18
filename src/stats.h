/*
 * stats.h -- transfer stats
 *
 * Yet Another FTP Client
 * Copyright (C) 2012, Josh Heidenreich <josh.sickmate@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _stats_h_
#define _stats_h_

#include "syshdr.h"

typedef struct Stats
{
	unsigned int success;
	unsigned int skip;
	unsigned int fail;
	u_int64_t size;
	
} Stats;


/**
* Create, destroy, reset
**/
Stats *stats_create(void);
void stats_destroy(Stats *stats);
void stats_reset(Stats *stats);

/**
* Called for each file  uploaded.
* These operate on the global variable gvStatsTransfer
* One day we might keep more stats (whole connection), so then these will operate on that object too.
**/
void stats_file(int type, u_int64_t size);

#define STATS_SUCCESS 1
#define STATS_SKIP 2
#define STATS_FAIL 3


/**
* Display stats
**/
void stats_display(Stats *stats, unsigned int threshold);

#endif
