/* $Id: rfile.c,v 1.8 2001/05/28 17:11:51 mhe Exp $
 *
 * rfile.c -- representation of a remote file
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
#include "strq.h"

rfile *rfile_create(void)
{
	rfile *f;

	f = (rfile *)xmalloc(sizeof(rfile));

	return f;
}

rfile *rfile_clone(const rfile *f)
{
	rfile *nf = rfile_create();
	nf->perm = xstrdup(f->perm);
	nf->owner = xstrdup(f->owner);
	nf->group = xstrdup(f->group);
	nf->color = xstrdup(f->color);
	nf->date = xstrdup(f->date);
	nf->link = xstrdup(f->link);
	nf->path = xstrdup(f->path);
	nf->size = f->size;
	nf->nhl = f->nhl;
	nf->mtime = f->mtime;

	return nf;
}

void rfile_clear(rfile *f)
{
	xfree(f->perm);
	xfree(f->owner);
	xfree(f->group);
	xfree(f->color);
	xfree(f->date);
	xfree(f->link);
	xfree(f->path);
	f->perm = f->owner = f->group = f->color = 0;
	f->date = f->link = f->path = 0;
	f->nhl = 0;
	f->size = 0L;
	f->mtime = (time_t)0;
}

void rfile_destroy(rfile *f)
{
	if(!f)
		return;
	rfile_clear(f);
	xfree(f);
}

bool risdir(const rfile *f)
{
	return (f->perm[0] == 'd');
}

bool risdotdir(const rfile *f)
{
	return (risdir(f)
			&& ((strcmp(base_name_ptr(f->path), ".") == 0
				 || strcmp(base_name_ptr(f->path), "..") == 0)));
}

bool risreg(const rfile *f)
{
	return !(risdir(f) || rispipe(f) || rischardev(f) || risblockdev(f));
}

bool rispipe(const rfile *f)
{
	return (f->perm[0] == 'p');
}

bool risexec(const rfile *f)
{
	return (strchr(f->perm, 'x') != 0);
}

bool rissock(const rfile *f)
{
	return (f->perm[0] == 's');
}

bool rischardev(const rfile *f)
{
	return (f->perm[0] == 'c');
}

bool risblockdev(const rfile *f)
{
	return (f->perm[0] == 'b');
}

bool rislink(const rfile *f)
{
	return (f->perm[0] == 'l');
}

void rfile_fake(rfile *f, const char *path)
{
	ftp_trace("faking file '%s'\n", path);

	xfree(f->perm);
	f->perm = xstrdup("-rw-r-r-");
	xfree(f->owner);
	f->owner = xstrdup("owner");
	xfree(f->group);
	f->group = xstrdup("group");
	xfree(f->link);
	f->link = 0;
	xfree(f->path);
	f->path = xstrdup(path);
	xfree(f->date);
	f->date = xstrdup("Jan  0  1900");
	f->mtime = 0;
	f->nhl = 0;
	f->size = (unsigned long)-1;
	rfile_parse_colors(f);
}

char rfile_classchar(const rfile *f)
{
	if(risdir(f)) return '/';
	if(rislink(f)) return '@';
	if(rissock(f)) return '=';
	if(rispipe(f)) return '|';
	if(risexec(f)) return '*';
	return 0;
}

mode_t str2mode_t(const char *p)
{
	mode_t m = 0;

	p++;
	if(*p++=='r') m|=S_IRUSR;
	if(*p++=='w') m|=S_IWUSR;
	if(*p=='x') m|=S_IXUSR;
	else if(*p=='S') m|=S_ISUID;
	else if(*p=='s') m|=(S_ISUID|S_IXUSR);
	p++;

	if(*p++=='r') m|=S_IRGRP;
	if(*p++=='w') m|=S_IWGRP;
	if(*p=='x') m|=S_IXGRP;
	else if(*p=='S') m|=S_ISGID;
	else if(*p=='s') m|=(S_ISGID|S_IXGRP);
	p++;

	if(*p++=='r') m|=S_IROTH;
	if(*p++=='w') m|=S_IWOTH;
	if(*p=='x') m|=S_IXOTH;
	else if(*p=='T') m|=S_ISVTX;
	else if(*p=='t') m|=(S_ISVTX|S_IXOTH);

	return m;
}

mode_t rfile_getmode(const rfile *f)
{
	if(!f || !f->perm)
		return (mode_t)-1;

	return str2mode_t(f->perm);
}

static const char *month_name[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/* returns month number if STR is a month abbreviation
 * else -1
 */
int month_number(const char *str)
{
	int i;

	for(i=0; i<12; i++) {
		if(strcasecmp(str, month_name[i]) == 0)
			return i;
	}
	return -1;
}

void rfile_parse_time(rfile *f, const char *m, const char *d, const char *y)
{
	struct tm mt;
	int u;

	f->mtime = (time_t)-1;

	if(!m || !d || !y)
		return;

	mt.tm_sec = 0;
	mt.tm_mday = atoi(d);
	mt.tm_mon = month_number(m);
	if(mt.tm_mon == -1)
		return;

	if(strchr(y, ':') != 0) {
		/* date on form "MMM DD HH:MM" */
		time_t now, tmp;
		struct tm *tm_now;
		char *eh, *em;
		u = strtoul(y, &eh, 10);
		if(eh == y)
			return;
		mt.tm_hour = u;
		u = strtoul(eh+1, &em, 10);
		if(em == eh+1)
			return;
		mt.tm_min = u;

		time(&now);
		tm_now = localtime(&now);
		mt.tm_year = tm_now->tm_year;
		/* might be wrong year, +- 1
		 * filetime is not older than 6 months
		 * or newer than 1 hour
		 */
		tmp = mktime(&mt);
		if(tmp > now + 60L*60L)
			mt.tm_year--;
		if(tmp < now - 6L*30L*24L*60L*60L)
			mt.tm_year++;
	} else {
		/* date on form "MMM DD YYYY" */
		char *ey;
		u = strtoul(y, &ey, 10);
		if(ey == y)
			return;
		mt.tm_year = u - 1900;
		mt.tm_hour = 0;
		mt.tm_min = 0;
	}
	f->mtime = mktime(&mt);
}

#define NEXT_FIELD \
e = strqsep(&cf, ' '); \
if(!e || !*e) { \
    return -1; \
}

#define NEXT_FIELD2 \
e = strqsep(&cf, ' '); \
if(!e || !*e) { \
    xfree(m); \
    xfree(d); \
    xfree(y); \
    return -1; \
}

char *time_to_string(time_t t)
{
	char *ret;
	time_t now;
	char *fmt;

	now = time(0);
	if(now > t + 6L * 30L * 24L * 60L * 60L  /* Old. */
	   || now < t - 60L * 60L)   /* In the future. */
		{
			/* The file is fairly old or in the future.
			   POSIX says the cutoff is 6 months old;
			   approximate this by 6*30 days.
			   Allow a 1 hour slop factor for what is considered "the future",
			   to allow for NFS server/client clock disagreement.
			   Show the year instead of the time of day.  */
			fmt = "%b %e  %Y";
		}
	else
		fmt = "%b %e %H:%M";
	ret = (char *)xmalloc(42);
	strftime(ret, 42, fmt, localtime(&t));

	return ret;
}

static int rfile_parse_eplf(rfile *f, char *str, const char *dirpath)
{
	char *e;

	if(!str || str[0] != '+')
		return -1;

	str++;

	f->perm = xstrdup("-rw-r--r--");
	f->size = 0L;
	f->mtime = 0;
	f->link = 0;
	f->nhl = 0;
	f->owner = xstrdup("owner");
	f->group = xstrdup("group");
	f->date = xstrdup("Jan  0  1900");
	f->path = 0;

	while((e = strqsep(&str, ',')) != 0) {
		switch(*e) {
		case '/':
			f->perm[0] = 'd';
			break;
		case 'm':
			f->mtime = strtoul(e+1, 0, 10);
			xfree(f->date);
			f->date = time_to_string(f->mtime);
			break;
		case 's':
			f->size = strtoul(e+1, 0, 10);
			break;
		case '\t':
			asprintf(&f->path, "%s/%s",
					 strcmp(dirpath, "/") ? dirpath : "", e+1);
			break;
		}
	}
	if(f->path == 0)
		return -1;
	rfile_parse_colors(f);

	return 0;
}

static int rfile_parse_unix(rfile *f, char *str, const char *dirpath)
{
	char *cf;
	char *e;
	char *m=0, *d=0, *y=0;
	char *saved_field[5];

	/* real unix ls listing:
	 *
	 * saved_field[0] == nhl
	 * saved_field[1] == owner
	 * saved_field[2] == group
	 * saved_field[3] == size
	 * saved_field[4] == month
	 *
	 * unix w/o group listing:
	 *
	 * saved_field[0] == nhl
	 * saved_field[1] == owner
	 * saved_field[2] == size
	 * saved_field[3] == month
	 * saved_field[4] == date
	 *
	 * strange MacOS WebStar thingy:
	 *
	 * saved_field[0] == size or "folder"
	 * saved_field[1] == zero or date if [0]=="folder"
	 * saved_field[2] == size (again!?) or month
	 * saved_field[3] == month or date
	 * saved_field[4] == date or time (or year?)
	 *
	 */

	if(strncmp(str, "total ", 6) == 0)
		return 1;

	cf = str;

/*	NEXT_FIELD;
	f->perm = xstrdup(e);*/
	/* the above doesn't work here:
	 * drwxrwxr-x156 31       20          29696 Apr 26 19:00 gnu
	 * ----------^
	 * so we assume the permission string is exactly 10 characters
	 */
	f->perm = xstrndup(cf, 10);
	cf += 10;

	NEXT_FIELD;
	saved_field[0] = xstrdup(e);

	NEXT_FIELD;
	saved_field[1] = xstrdup(e);

	NEXT_FIELD;
	saved_field[2] = xstrdup(e);

	NEXT_FIELD;
	/* special device? */
	if(e[strlen(e)-1] == ',') {
		NEXT_FIELD;
	}
	saved_field[3] = xstrdup(e);

	NEXT_FIELD;
	saved_field[4] = xstrdup(e);

	/* distinguish the different ls variants by looking
	 * for the month field
	 */

	if(month_number(saved_field[4]) != -1)
	{
		/* ls -l */
		f->nhl = atoi(saved_field[0]);
		xfree(saved_field[0]);
		f->owner = saved_field[1];
		f->group = saved_field[2];
		f->size = atol(saved_field[3]);
		xfree(saved_field[3]);
		m = saved_field[4];
		NEXT_FIELD2;
		d = xstrdup(e);
		NEXT_FIELD2;
		y = xstrdup(e);
	} else if(month_number(saved_field[3]) != -1)
	{
		/* ls -lG */
		f->nhl = atoi(saved_field[0]);
		xfree(saved_field[0]);
		f->owner = saved_field[1];
		f->group = xstrdup("group");
		f->size = atol(saved_field[2]);
		xfree(saved_field[2]);
		m = saved_field[3];
		d = saved_field[4];
		NEXT_FIELD2;
		y = xstrdup(e);
	} else if(month_number(saved_field[2]) != -1)
	{
		xfree(saved_field[0]);
		f->nhl = 0;
		f->owner = xstrdup("owner");;
		f->group = xstrdup("group");
		f->size = atol(saved_field[1]);
		xfree(saved_field[1]);
		m = saved_field[2];
		d = saved_field[3];
		y = saved_field[4];
	} else {
		xfree(saved_field[0]);
		xfree(saved_field[1]);
		xfree(saved_field[2]);
		xfree(saved_field[3]);
		xfree(saved_field[4]);
		return -1;
	}

	asprintf(&f->date, "%s %2s %5s", m, d, y);
	rfile_parse_time(f, m, d, y);
	if(f->mtime == (time_t)-1)
		ftp_trace("rfile_parse_time failed! date == '%s'\n", f->date);
	xfree(m);
	xfree(d);
	xfree(y);

	if(!cf)
		return -1;

	e = strstr(cf, " -> ");
	if(e) {
		*e = 0;
		f->link = xstrdup(e+4);
	}

#if 0
	/* ftp.apple.com:
	 *
	 * drwxr-xr-x   8 0        system       512 Jan  1 22:51 dts
	 * d--x--x--x   2 0        system       512 Sep 12 1997  etc
	 *                                             --------^
	 * doesn't pad year, so assume filename doesn't start with a space
	 */
	/* I've dropped this workaround, 'cause ftp.apple.com seems to
	 * have fixed this, and this prevents a cd to directories with
	 * leading spaces
	 */
	while(*cf == ' ')
		cf++;
#endif

	asprintf(&f->path, "%s/%s", strcmp(dirpath, "/") ? dirpath : "", cf);

	rfile_parse_colors(f);

	return 0;
}

static int rfile_parse_dos(rfile *f, char *str, const char *dirpath)
{
	char *e, *cf = str;
	char ampm[3]="xx";
	int m, d, y, h, mm;

	NEXT_FIELD;
	if(sscanf(e, "%d-%d-%d", &m, &d, &y) != 3)
		return -1;
	m--;

	if(y < 70)
		y += 100;

	NEXT_FIELD;
	if(sscanf(e, "%d:%2d%2s", &h, &mm, ampm) != 3)
		return -1;

	if(strcasecmp(ampm, "PM") == 0)
		h += 12;

	{
		struct tm mt;

		f->mtime = (time_t)-1;

		mt.tm_sec = 0;
		mt.tm_min = mm;
		mt.tm_hour = h;
		mt.tm_mday = d;
		mt.tm_mon = m;
		mt.tm_year = y;
		mt.tm_isdst = -1;

		f->mtime = mktime(&mt);
	}

	{
		time_t now;
		time(&now);

		if(f->mtime != (time_t)-1 && (now > f->mtime + 6L * 30L * 24L * 60L * 60L  /* Old. */
									  || now < f->mtime - 60L * 60L))   /* In the future. */
		{
			asprintf(&f->date, "%s %2d %5d", month_name[m], d, y + 1900);
		} else {
			asprintf(&f->date, "%s %2d %02d:%02d", month_name[m], d, h, mm);
		}
	}

	NEXT_FIELD;
	if(strcasecmp(e, "<DIR>") == 0) {
		f->perm[0] = 'd';
		f->size = 0L;
	} else {
		f->size = (unsigned long)atol(e);
	}

	if(f->perm[0] == 'd')
		f->perm = xstrdup("drwxr-xr-x");
	else
		f->perm = xstrdup("-rw-r--r--");
	f->nhl = 1;
	f->owner = xstrdup("owner");
	f->group = xstrdup("group");
	f->link = 0;

	while(cf && *cf == ' ')
		++cf;

	asprintf(&f->path, "%s/%s", strcmp(dirpath, "/") ? dirpath : "", cf);

	rfile_parse_colors(f);

	return 0;
}

/* type=cdir;sizd=4096;modify=20010528094249;UNIX.mode=0700;UNIX.uid=1000;UNIX.gid=1000;unique=1642g7c81 .
 */

static int rfile_parse_mlsd(rfile *f, char *str, const char *dirpath)
{
	char *e;
	bool isdir = false;

	if(!str)
		return -1;

	e = strchr(str, ' ');
	if(e) {
		asprintf(&f->path, "%s/%s",
				 strcmp(dirpath, "/") ? dirpath : "", base_name_ptr(e+1));
		*e = 0;
	} else
		return -1;

	f->perm = 0;
	f->size = 0L;
	f->mtime = 0;
	f->link = 0;
	f->nhl = 0;
	f->owner = xstrdup("owner");
	f->group = xstrdup("group");
	f->date = xstrdup("Jan  0  1900");

	while((e = strqsep(&str, ';')) != 0) {
		char *factname, *value;

		factname = strqsep(&e, '=');
		value = e;

		if(!factname || !value) {
			return -1;
		}

		if(strcasecmp(factname, "size") == 0 ||
			strcasecmp(factname, "sizd") == 0)
			/* the "sizd" fact is not standardized in "Extension to
			 * FTP" Internet draft, but PureFTPd uses it for some
			 * reason for size of directories
			 */
			f->size = atol(value);
		else if(strcasecmp(factname, "type") == 0) {
			if(strcasecmp(value, "file") == 0)
				isdir = false;
			else if(strcasecmp(value, "dir") == 0 ||
					strcasecmp(value, "cdir") == 0 ||
					strcasecmp(value, "pdir") == 0)
				isdir = true;
		} else if(strcasecmp(factname, "modify") == 0) {
			struct tm ts;

			sscanf(value, "%04d%02d%02d%02d%02d%02d",
				   &ts.tm_year, &ts.tm_mon, &ts.tm_mday,
				   &ts.tm_hour, &ts.tm_min, &ts.tm_sec);
			ts.tm_year -= 1900;
			ts.tm_mon--;
			f->mtime = gmt_mktime(&ts);
			xfree(f->date);
			f->date = time_to_string(f->mtime);
		} else if(strcasecmp(factname, "UNIX.mode") == 0) {
			xfree(f->perm);
			f->perm = perm2string(strtoul(value, 0, 8));
		} else if(strcasecmp(factname, "UNIX.gid") == 0) {
			xfree(f->group);
			f->group = xstrdup(value);
		} else if(strcasecmp(factname, "UNIX.uid") == 0) {
			xfree(f->owner);
			f->owner = xstrdup(value);
		}
	}

	if(!f->perm)
		f->perm = xstrdup("-rw-r--r--");

	if(isdir)
		f->perm[0] = 'd';

	rfile_parse_colors(f);

	return 0;
}

int rfile_parse(rfile *f, char *str, const char *dirpath)
{
	int i;
	int r = -1;

	if(ftp->LIST_type == ltUnknown)
		ftp->LIST_type = ltUnix;

	for(i=0;i<4;i++) {
		char *tmp = xstrdup(str);

		if(ftp->LIST_type == ltUnix)
			r = rfile_parse_unix(f, tmp, dirpath);
		else if(ftp->LIST_type == ltDos)
			r = rfile_parse_dos(f, tmp, dirpath);
		else if(ftp->LIST_type == ltEplf)
			r = rfile_parse_eplf(f, tmp, dirpath);
		else if(ftp->LIST_type == ltMlsd)
			r = rfile_parse_mlsd(f, tmp, dirpath);

		xfree(tmp);

		if(r == -1) {
			rfile_clear(f);

			if(ftp->LIST_type == ltUnix) {
				ftp->LIST_type = ltDos;
				ftp_trace("UNIX output parsing failed, trying DOS\n");
			} else if(ftp->LIST_type == ltDos) {
				ftp->LIST_type = ltEplf;
				ftp_trace("DOS output parsing failed, trying EPLF\n");
			} else if(ftp->LIST_type == ltEplf) {
				ftp->LIST_type = ltMlsd;
				ftp_trace("EPLF output parsing failed, trying MLSD\n");
			} else if(ftp->LIST_type == ltMlsd) {
				ftp->LIST_type = ltUnix;
				ftp_trace("MLSD output parsing failed, trying UNIX\n");
			}
		} else
			return r;
	}

	return -1;
}

int rfile_search_filename(rfile *f, const char *filename)
{
	return strcmp(base_name_ptr(f->path), filename);
}

int rfile_search_path(rfile *f, const char *path)
{
	return strcmp(f->path, path);
}
