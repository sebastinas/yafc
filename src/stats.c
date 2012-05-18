/*
 * stats.c -- transfer stats
 *
 * Yet Another FTP Client
 * Copyright (C) 2012, Josh Heidenreich <josh.sickmate@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#include "syshdr.h"
#include "stats.h"
#include "gvars.h"


Stats *stats_create(void)
{
	Stats *stats = (Stats *)xmalloc(sizeof(Stats));
	stats_reset(stats);
	return stats;
}

void stats_destroy(Stats *stats)
{
	free(stats);
}

void stats_reset(Stats *stats)
{
	stats->success = 0;
	stats->skip = 0;
	stats->fail = 0;
	stats->size = 0;
}

void stats_file(int type, u_int64_t size)
{
	gvStatsTransfer->size += size;

	switch(type) {
		case STATS_SUCCESS: gvStatsTransfer->success++; break;
		case STATS_SKIP: gvStatsTransfer->skip++; break;
		case STATS_FAIL: gvStatsTransfer->fail++; break;
	}
}

void stats_display(Stats *s, unsigned int threshold)
{
	if ((s->success + s->skip + s->fail) < threshold) return;

	printf("\n");
	if (s->success > 0)
		printf(_("Transferred %u files, "), s->success);
	if (s->skip > 0)
		printf(_("Skipped %u files, "), s->skip);
	if (s->fail > 0)
		printf(_("%u failures, "), s->fail);

	printf(_("total size %u bytes.\n\n"), s->size);
}

