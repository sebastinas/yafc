/* list.c -- the remote cached 'ls' command
 *
 * This file is part of Yafc, an ftp client.
 * This program is Copyright (C) 1998-2001 martin HedenfaLk
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
#include "ftp.h"
#include "strq.h"
#include "commands.h"
#include "gvars.h"
#include "utils.h"

/* ls options */
#define LS_LONG 1
#define LS_ALL (1 << 1)
#define LS_ALMOST_ALL (1 << 2)
#define LS_NO_BACKUP (1 << 3)
#define LS_CLASSIFY (1 << 4)
#define LS_COLOR_AUTO (1 << 5)
#define LS_DIRECTORY (1 << 6)
#define LS_NO_GROUP (1 << 7)
#define LS_SORT_SIZE (1 << 8)
#define LS_SORT_EXT (1 << 9)
#define LS_SORT_REVERSE (1 << 10)
#define LS_SORT_DIRS_FIRST (1 << 11)
#define LS_SORT_MTIME (1 << 12)
#define LS_LIST_BY_LINES (1 << 13)
#define LS_ONE_PER_LINE (1 << 14)
#define LS_COLOR_ALWAYS (1 << 15)
#define LS_RECURSIVE (1 << 16)
#define LS_LITERAL (1 << 17)
#define LS_WPATH (1 << 18)
#define LS_HUMAN_READABLE (1 << 19)
#define LS_SORT_NAME (1 << 20)

static void print_ls_syntax(void)
{
	printf(_("List information about FILE (the current directory by default).  Usage:\n"
			 "  ls [options] [file]\n"
			 "Options:\n"
			 "  -a, --all                do not hide entries starting with .\n"
			 "  -A, --almost-all         as --all but do not list . and ..\n"
			 "  -B, --ignore-backups     do not list implied entries ending with ~\n"
			 "  -C                       list entries by columns\n"
			 "      --color[=WHEN]       control whether color is used to distinguish file\n"
			 "                             types. WHEN may be 'never', 'always' or 'auto'\n"
			 "  -d, --directory          list directory entries instead of contents\n"
			 "      --dirs-first         shows directories first (last with --reverse)\n"
			 "  -F, --classify           append a character for typing each entry\n"
			 "  -g                       (ignored)\n"
			 "  -G, --no-group           inhibit display of group information\n"
			 "  -h, --human-readable     print sizes in human readable format (e.g., 1K 234M)\n"
			 "  -l                       use a long listing format\n"
			 "  -N, --literal            print raw entry names (don't treat e.g. control\n"
			 "                             characters specially)\n"
			 "  -o                       use long listing format without group info\n"
			 "  -r, --reverse            reverse order while sorting\n"
			 "  -R, --recursive          list subdirectories recursively\n"
			 "  -S                       sort by file size\n"
			 "  -t                       sort by modification time\n"
			 "  -U                       do not sort; list entries in directory order\n"
			 "  -x                       list entries by lines instead of by columns\n"
			 "  -X                       sort alphabetically by entry extension\n"
			 "  -1                       list one file per line\n"
			 "      --help               display this help\n"));
}

static int dontlist_opt = 0;

static bool dontlist(rfile *f)
{
	const char *name = base_name_ptr(f->path);

	if(name[0] == '.' && !test(dontlist_opt, LS_ALL))
		return true;
	if((strcmp(name, ".")==0 || strcmp(name, "..")==0)
	   && test(dontlist_opt, LS_ALMOST_ALL))
		return true;
	if(test(dontlist_opt, LS_NO_BACKUP) && name[strlen(name)-1]=='~')
		return true;
	return false;
}

static int print_filename(rfile *fi, unsigned opt, bool doclr)
{
	int len = 0;

	if(doclr && fi->color)
		printf("%s", fi->color);
	if(test(opt, LS_LITERAL))
		len += printf("%s", base_name_ptr(fi->path));
	else {
		char *e = test(opt, LS_WPATH) ? xstrdup(fi->path)
			: xstrdup(base_name_ptr(fi->path));
		fix_unprintable(e);
		len += printf("%s", e);
		xfree(e);
	}
	if(doclr && fi->color)
		printf("%s", endcolor());
	if(test(opt, LS_CLASSIFY)) {
		char cc = rfile_classchar(fi);
		if(cc != '\0') {
			putchar(cc);
			len++;
		}
	}
	return len;
}

static void ls_long(list *gl, unsigned opt, bool doclr)
{
	listitem *li;
	rfile *fi;

	printf(_("total %lu\n"), rglob_size(gl));

	for(li=gl->first; li; li=li->next) {

		if(gvInterrupted)
			break;

		fi = (rfile *)li->data;
		printf("%s %3u %-8s ", fi->perm, fi->nhl, fi->owner);
		if(!test(opt, LS_NO_GROUP))
			printf("%-8s ", fi->group);

		if(test(opt, LS_HUMAN_READABLE))
			printf("%8s %s ", human_size(fi->size), fi->date);
		else
			printf("%8lu %s ", fi->size, fi->date);

		if(rislink(fi) && fi->link) {
			char *fipath = base_dir_xptr(fi->path);
			char *lnpath = path_absolute(fi->link, fipath, ftp->homedir);
			rfile *lnfi = ftp_cache_get_file(lnpath);
			print_filename(fi, opt & ~LS_CLASSIFY, doclr);
			xfree(lnpath);
			xfree(fipath);
			printf(" -> ");
			if(lnfi) {
				lnfi = rfile_clone(lnfi);
				xfree(lnfi->path);
				lnfi->path = xstrdup(fi->link);
				print_filename(lnfi, opt|LS_WPATH, doclr);
			} else {
				lnfi = rfile_create();
				rfile_fake(lnfi, fi->link);
				print_filename(lnfi, opt|LS_WPATH, doclr);
			}
			rfile_destroy(lnfi);
		} else
			print_filename(fi, opt, doclr);
		putchar('\n');
	}
}

static void ls_wide(list *gl, unsigned opt, bool doclr)
{
	listitem *li;
	rfile *fi;
	unsigned int maxlen = 1;
	unsigned int columns;
	unsigned int numfiles;
	unsigned int files_per_line;
	unsigned int lines;

	/* determine max length of filenames */
	for(li=gl->first; li; li=li->next) {
		unsigned int len;
		fi = (rfile *)li->data;
		len = strlen(base_name_ptr(fi->path)) + 3;
		if(len > maxlen)
			maxlen = len;
	}

	columns = 80;
#ifdef TIOCGWINSZ
	{
		struct winsize ws;
		if(ioctl(1, TIOCGWINSZ, &ws) != -1 && ws.ws_col != 0)
			columns = ws.ws_col;
	}
#endif

	/* determine number of filenames */
	numfiles = list_numitem(gl);
	files_per_line = columns / maxlen;
	if(files_per_line == 0)
		files_per_line = 1;
	lines = numfiles / files_per_line;
	if((numfiles % files_per_line) != 0)
		lines++;
	if(lines == 0)
		lines++;
	files_per_line = numfiles / lines + (numfiles % lines != 0);

	if(test(opt, LS_LIST_BY_LINES)) {
		unsigned cc = 0;
		for(li=gl->first; li; li=li->next) {
			unsigned left;

			if(gvInterrupted)
				break;

			fi = (rfile *)li->data;
			left = maxlen - print_filename(fi, opt, doclr);
			if(++cc == files_per_line) {
				putchar('\n');
				cc = 0;
			} else {
				while(left--)
					putchar(' ');
			}
		}
		if(cc != 0 && ++cc < files_per_line)
			putchar('\n');

	} else {
		unsigned cl;
		for(cl=0; cl<lines; cl++) {
			unsigned i = cl;
			li = gl->first;
			while(i--)
				li = li->next;

			if(gvInterrupted)
				break;

			while(true) {
				unsigned left;
				bool br;

				if(gvInterrupted)
					break;

				left = maxlen - print_filename((rfile *)li->data, opt, doclr);

				i = lines;
				br = false;
				while(i--) {
					li = li->next;
					if(li == 0) {
						br = true;
						break;
					}
				}
				if(br)
					break;

				while(left--)
					putchar(' ');
			}
			putchar('\n');
		}
	}
}

/* option -1, one file per line */
static void ls_opl(list *gl, unsigned opt, bool doclr)
{
	listitem *li;

	for(li=gl->first; li; li=li->next) {

		if(gvInterrupted)
			break;

		print_filename((rfile *)li->data, opt, doclr);
		putchar('\n');
	}
}

static int ls_sort_dirs(rfile *a, rfile *b)
{
	if(risdir(a) && !risdir(b))
		return -1;
	if(!risdir(a) && risdir(b))
		return 1;
	return 0;
}

static int ls_sort_size(rfile *a, rfile *b)
{
	if(a->size > b->size)
		return -1;
	if(a->size < b->size)
		return 1;
	return 0;
}

static int ls_sort_ext(rfile *a, rfile *b)
{
	const char *ab = base_name_ptr(a->path);
	const char *bb = base_name_ptr(b->path);
	char *ae = strrchr(ab, '.');
	char *be = strrchr(bb, '.');
	int c;

	if(!ae && !be)
		return strcmp(ab, bb);
	if(!ae)
		return -1;
	if(!be)
		return 1;
	c = strcmp(ae+1, be+1);
	if(c == 0)
		return strcmp(ab, bb);
	return c;
}

static int ls_sort_mtime(rfile *a, rfile *b)
{
	if(a->mtime > b->mtime)
		return -1;
	if(a->mtime < b->mtime)
		return 1;
	return 0;
}

static int ls_sort_name(rfile *a, rfile *b)
{
	return strcmp(a->path, b->path);
}

static void ls_all(list *gl, unsigned opt, bool doclr)
{
	if(list_numitem(gl) == 0)
		return;

	if(test(opt, LS_SORT_NAME))
		list_sort(gl, (listsortfunc)ls_sort_name, test(opt, LS_SORT_REVERSE));
	else if(test(opt, LS_SORT_DIRS_FIRST))
		list_sort(gl, (listsortfunc)ls_sort_dirs, test(opt, LS_SORT_REVERSE));
	else if(test(opt, LS_SORT_SIZE))
		list_sort(gl, (listsortfunc)ls_sort_size, test(opt, LS_SORT_REVERSE));
	else if(test(opt, LS_SORT_EXT))
		list_sort(gl, (listsortfunc)ls_sort_ext, test(opt, LS_SORT_REVERSE));
	else if(test(opt, LS_SORT_MTIME))
		list_sort(gl, (listsortfunc)ls_sort_mtime, test(opt, LS_SORT_REVERSE));

	if(test(opt, LS_LONG))
		ls_long(gl, opt, doclr);
	else if(!test(opt, LS_ONE_PER_LINE))
		ls_wide(gl, opt, doclr);
	else
		ls_opl(gl, opt, doclr);
}

static void ls_recursive(const list *gl, unsigned opt, bool doclr)
{
	listitem *li;

	for(li=gl->first; li; li=li->next) {
		rfile *f = (rfile *)li->data;

		if(gvInterrupted)
			break;

/*		if(risdotdir(f))
			continue;*/

		if(ftp_maybe_isdir(f)) {
			list *recurs_gl = rglob_create();
			char *recurs_mask;

			asprintf(&recurs_mask, "%s/*", f->path);
			rglob_glob(recurs_gl, recurs_mask, false, false, dontlist);

			if(list_numitem(recurs_gl) > 0) {
				printf("\n%s:\n", f->path);
				ls_all(recurs_gl, opt, doclr);
				ls_recursive(recurs_gl, opt, doclr);
			}
		}
	}
}

static void lsfiles(list *gl, unsigned opt)
{
	bool doclr = false;

	if(test(opt, LS_COLOR_AUTO))
		doclr = isatty(fileno(stdout));
	else if(test(opt, LS_COLOR_ALWAYS))
		doclr = true;

	ls_all(gl, opt, doclr);

	if(test(opt, LS_RECURSIVE))
		ls_recursive(gl, opt, doclr);
}

/* cached ls */
void cmd_ls(int argc, char **argv)
{
	int opt=0, c;
	list *gl;
	struct option longopts[] = {
		{"dirs-first", no_argument, 0, '2'},
		{"all", no_argument, 0, 'a'},
		{"almost-all", no_argument, 0, 'A'},
		{"classify", no_argument, 0, 'F'},
		{"color", optional_argument, 0, 'c'},
		{"no-group", no_argument, 0, 'G'},
		{"human-readable", no_argument, 0, 'h'},
		{"ignore-backups", no_argument, 0, 'B'},
		{"directory", no_argument, 0, 'd'},
		{"literal", no_argument, 0, 'N'},
		{"reverse", no_argument, 0, 'r'},
		{"recursive", no_argument, 0, 'R'},
		{"help", no_argument, 0, '3'},
		{0, 0, 0, 0},
	};

	opt |= LS_SORT_NAME;

	optind = 0; /* force getopt() to re-initialize */
	while((c = getopt_long(argc, argv, "aAFCdGhlNogBrRStUxX12",
						   longopts, NULL)) != EOF) {
		switch(c) {
		  case 'a':
			opt |= LS_ALL;
			break;
		  case 'A':
			opt |= LS_ALL;
			opt |= LS_ALMOST_ALL;
			break;
		  case 'F':
			opt |= LS_CLASSIFY;
			break;
		  case 'C':
			opt &= ~LS_LONG;
			break;
		  case 'd':
			opt |= LS_DIRECTORY;
			break;
		  case 'G':
			opt |= LS_NO_GROUP;
			break;
		case 'h':
			opt |= LS_HUMAN_READABLE;
			break;
		  case 'l':
			opt |= LS_LONG;
			opt &= ~LS_NO_GROUP;
			break;
		  case 'N':
			opt |= LS_LITERAL;
			break;
		  case 'o':
			opt |= LS_LONG;
			opt |= LS_NO_GROUP;
			break;
		  case 'g':
			/* ignored */
			break;
		  case 'B':
			opt |= LS_NO_BACKUP;
			break;
		  case 'S':
			opt |= LS_SORT_SIZE;
			opt &= ~(LS_SORT_EXT | LS_SORT_DIRS_FIRST | LS_SORT_MTIME);
			break;
		  case 'U':
			opt &= ~(LS_SORT_NAME | LS_SORT_SIZE | LS_SORT_EXT
					 | LS_SORT_DIRS_FIRST | LS_SORT_MTIME);
			break;
		  case 'x':
			opt |= LS_LIST_BY_LINES;
			break;
		  case 'R':
			opt |= LS_RECURSIVE;
			break;
		  case 'r':
			opt |= LS_SORT_REVERSE;
			break;
		  case 't':
			opt |= LS_SORT_MTIME;
			opt &= ~(LS_SORT_SIZE | LS_SORT_EXT | LS_SORT_DIRS_FIRST);
			break;
		  case 'X':
			opt |= LS_SORT_EXT;
			opt &= ~(LS_SORT_SIZE | LS_SORT_DIRS_FIRST | LS_SORT_MTIME);
			break;
		  case 'c':
			if(optarg == 0 || strcmp(optarg, "always") == 0) {
				opt &= ~LS_COLOR_AUTO;
				opt |= LS_COLOR_ALWAYS;
			} else if(strcmp(optarg, "auto") == 0)
				opt |= LS_COLOR_AUTO;
			else if(strcmp(optarg, "never") == 0)
				opt &= ~(LS_COLOR_AUTO | LS_COLOR_ALWAYS);
			break;
		  case '1':
			opt |= LS_ONE_PER_LINE;
			break;
		case '2': /* --dirs-first */
			opt |= LS_SORT_DIRS_FIRST;
			opt &= ~(LS_SORT_SIZE | LS_SORT_EXT | LS_SORT_MTIME);
			break;
		case '3': /* --help */
			print_ls_syntax();
			return;
		  case '?':
			return;
		}
	}

	need_connected();
	need_loggedin();

	gl = rglob_create();
	dontlist_opt = opt;
	if(optind >= argc) {
		rglob_glob(gl, "*", false, false, dontlist);
	} else {
		/* strategy:
		 * if listing directory contents:
		 * if parameter ends in '/', append '*' and glob...
		 * else read basepath and find out if parameter is dir
		 * if dir, append '*' and glob, else glob that file
		 * if not listing directory contents just glob that file
		 */

		while(optind < argc) {
			char *e, *f;
			bool isdir = false;

			e = argv[optind++];
			if(!test(opt, LS_DIRECTORY)) {
				f = strrchr(e, 0) - 1;
				if(*f == '/') {
					*f = 0; /* we'll put it back below */
					isdir = true;
				} else {
					rdirectory *rdir;
					rfile *rf;
					char *q = xstrdup(e);
					unquote(q);
					rdir = ftp_get_directory(q);
					if(rdir)
						isdir = true;
					else {
						rf = ftp_get_file(q);
						xfree(q);
						if(rf && ftp_maybe_isdir(rf) == 1)
							isdir = true;
					}
				}
			} else
				/* prevent rglob_glob() from appending "*" */
				stripslash(e);

			if(isdir) {
				asprintf(&f, "%s/*", e);
				rglob_glob(gl, f, false, false, dontlist);
				xfree(f);
			} else
				rglob_glob(gl, e, false, false, dontlist);
		}
	}

	if(list_numitem(gl) == 0) {
		if(test(opt, LS_LONG))
			printf(_("total 0\n"));
		return;
	}

	lsfiles(gl, opt);
}
