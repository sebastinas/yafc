/* bookmark.c -- create bookmark(s)
 *
 * This file is part of Yafc, an ftp client.
 * This program is Copyright (C) 1998, 1999, 2000 Martin Hedenfalk
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
#include "input.h"
#include "gvars.h"
#include "base64.h"
#include "completion.h"
#include "commands.h"
#include "shortpath.h"
#include "strq.h"
#include "rc.h"
#include "utils.h"

#define BM_BOOKMARK 0
#define BM_SAVE 1
#define BM_EDIT 2
#define BM_READ 3
#define BM_LIST 4
#define BM_DELETE 5

static char *xquote_chars(const char *str, const char *qchars)
{
	char *q, *oq;

	if(!str)
		return 0;

	oq = q = (char *)xmalloc(strlen(str)*2 + 1);

	while(str && *str) {
		if(strchr(qchars, *str) != 0)
			*q++ = '\\';
		*q++ = *str;
		str++;
	}
	*q++ = 0;

	return oq;
}

static void bookmark_save_one(FILE *fp, url_t *url)
{
	if(url == gvLocalUrl)
		fprintf(fp, "local");
	else if(url == gvDefaultUrl)
		fprintf(fp, "default");
	else {
		fprintf(fp, "machine %s:%d", url->hostname,
				url->port==-1 ? 21 : url->port);
		if(url->alias) {
			char *a = xquote_chars(url->alias, "\'\"");
			fprintf(fp, " alias '%s'", a);
			xfree(a);
		}
	}
	fprintf(fp, "\n  login %s", url->username);

	if(url->directory)
		fprintf(fp, " cwd '%s'", url->directory);

	if(url->protlevel)
		fprintf(fp, " prot %s", url->protlevel);
	if(url->mech) {
		char *mech_string = stringify_list(url->mech);
		if(mech_string) {
			fprintf(fp, " mech '%s'", mech_string);
			xfree(mech_string);
		}
	}
	if(url->pasvmode != -1)
		fprintf(fp, " passive %s", url->pasvmode ? "true" : "false");
	if(url->password) {
		char *cq;
		base64_encode(url->password, strlen(url->password), &cq);
		fprintf(fp, " password [base64]%s", cq);
		xfree(cq);
	}
	fprintf(fp, "\n\n");
}

static int bookmark_save(const char *other_bmfile)
{
	FILE *fp;
	char *bmfile;
	listitem *li;

	if(other_bmfile)
		bmfile = xstrdup(other_bmfile);
	else
		asprintf(&bmfile, "%s/bookmarks", gvWorkingDirectory);
	fp = fopen(bmfile, "w");
	if(fp == 0) {
		perror(bmfile);
		xfree(bmfile);
		return -1;
	}

	fprintf(fp,
			"# this is an automagically created file\n"
			"# so don't bother placing comments here, they will be overwritten\n"
			"# make sure this file isn't world readable if passwords are stored here\n"
			"\n");

	if(gvDefaultUrl)
		bookmark_save_one(fp, gvDefaultUrl);
	if(gvLocalUrl)
		bookmark_save_one(fp, gvLocalUrl);
	for(li=gvBookmarks->first; li; li=li->next)
		bookmark_save_one(fp, (url_t *)li->data);
	fprintf(fp, "# end of bookmark file\n");
	fclose(fp);
	chmod(bmfile, S_IRUSR | S_IWUSR);
	xfree(bmfile);
	return 0;
}

static char *guess_alias(url_t *url)
{
	char *e, *p, *o, *maxp;
	int max = 0, len;

	if(!url)
		return 0;

	if(url->alias)
		return xstrdup(url->alias);

	o = e = xstrdup(url->hostname);
	if(!e)
		return 0;

	/* IP address? */
	if(isdigit((int)e[0]))
	   return e;

	maxp = e;
	while(true) {
		p = strqsep(&e, '.');
		if(!p)
			break;

		if(*e == 0) {
			/* don't count the last part (the toplevel domain) */
			break;
		}

		len = strlen(p);
		if(len >= max) {
			max = len;
			maxp = p;
		}
	}
	p = xstrdup(maxp);
	xfree(o);
	return p;
}

/* alias and password should be set already in url */
static void create_the_bookmark(url_t *url)
{
	listitem *li;

	/* we don't want to be asked again */
	url_setalias(ftp->url, url->alias);

	if(strcmp(ftp->curdir, ftp->homedir) != 0)
		url_setdirectory(url, ftp->curdir);
	else
		url_setdirectory(url, 0);

	list_clear(gvBookmarks);
	{
		char *tmp;
		asprintf(&tmp, "%s/bookmarks", gvWorkingDirectory);
		parse_rc(tmp, false);
		xfree(tmp);
	}

	li = list_search(gvBookmarks, (listsearchfunc)urlcmp, url);
	if(li)
		list_delitem(gvBookmarks, li);
	list_additem(gvBookmarks, url);

	bookmark_save(0);
}

static void create_bookmark(const char *guessed_alias)
{
	char *default_alias = 0;
	char *prompt;
	url_t *url;
	listitem *li;
	int a;
	char *alias;

	alias = xstrdup(guessed_alias);

	while(true) {
		if(!alias) {
			default_alias = guess_alias(ftp->url);

			if(default_alias)
				asprintf(&prompt, "alias (%s): ", default_alias);
			else
				prompt = xstrdup("alias: ");

			force_completion_type = cpBookmark;
			alias = input_read_string(prompt);
			xfree(prompt);
			if(!alias || !*alias)
				alias = default_alias;
			else
				xfree(default_alias);
		}

		url = url_clone(ftp->url);
		url_setalias(url, alias);
		xfree(alias);
		alias = 0;

		li = list_search(gvBookmarks, (listsearchfunc)urlcmp, url);
		if(li) {
			a = ask(ASKYES|ASKNO|ASKCANCEL, ASKYES,
					_("a bookmark named '%s' already exists, overwrite?"),
					url->alias ? url->alias : url->hostname);
			if(a == ASKCANCEL) {
				url_destroy(url);
				return;
			}
			if(a == ASKYES)
				break;
		} else
			break;
	}

	/* password is automatically saved if anonymous login */
	if(!url_isanon(url) && url->password) {
		a = ask(ASKYES|ASKNO, ASKNO, _("Save password?"));
		if(a == ASKNO)
			url_setpassword(url, 0);
	}

	create_the_bookmark(url);
	printf(_("created bookmark %s\n"), url->alias);
}

/* check if anything has changed for this bookmark */
static bool should_update_bookmark(url_t *url)
{
	/* don't update bookmark if username has changed */
	if(strcmp(url->username, ftp->url->username) != 0)
		return false;

	if(url->directory && strcmp(url->directory, ftp->curdir) != 0)
		return true;
	if(!url->directory && strcmp(ftp->curdir, ftp->homedir) != 0)
		return true;
	if(xstrcmp(url->hostname, ftp->url->hostname) != 0)
		return true;
	if(url->port != ftp->url->port)
		return true;
	if(!list_equal(url->mech, ftp->url->mech, (listsortfunc)strcasecmp) != 0)
		return true;
	if(xstrcmp(url->protlevel, ftp->url->protlevel) != 0)
		return true;
	if(url->pasvmode != ftp->url->pasvmode && ftp->url->pasvmode != -1)
		return true;
	return false;
}
	
void auto_create_bookmark(void)
{
	listitem *li;
	int a;
	bool auto_create, had_passwd = false;
	bool update = false;

	if(gvAutoBookmark == 0)
		return;

	if(!ftp_loggedin() || gvSighupReceived)
		return;

	auto_create = (gvAutoBookmark == 1);

	/* check if bookmark already exists */
	li = list_search(gvBookmarks, (listsearchfunc)urlcmp, ftp->url);
	if(li) {
		if(!should_update_bookmark((url_t *)li->data))
			return;

		/* bookmark already exist, update it */
		update = true;
		auto_create = true;
		had_passwd = (((url_t *)li->data)->password != 0);
	}

	if(!auto_create) {
		a = ask(ASKYES|ASKNO, ASKNO,
				_("Do you want to create a bookmark for this site?"));
		if(a == ASKNO)
			return;
		create_bookmark(0);
	} else { /* auto autobookmark... */
		url_t *url;
		char *a;

		url = url_clone(ftp->url);
		a = guess_alias(ftp->url);
		url_setalias(url, a);
		xfree(a);
		
		li = list_search(gvBookmarks, (listsearchfunc)urlcmp, url);
		if(li) {
			/* bookmark already exist, overwrite it */
			while(true) {
				url_t *xurl = (url_t *)li->data;

				if(xurl->alias && (strcmp(xurl->alias, url->alias) == 0)
				   && (strcmp(xurl->hostname, url->hostname) != 0))
				{
					/* create a new version of the alias, since guessed alias
					 * already exists but for another host
					 */
					
					char *par;
					update = false;
					par = strrchr(url->alias, '(');
					if(par == 0)
						asprintf(&a, "%s(2)", url->alias);
					else {
						int ver;
						*par = 0;
						ver = atoi(par + 1);
						asprintf(&a, "%s(%d)", url->alias, ver + 1);
					}
					url_setalias(url, a);
					xfree(a);
				} else {
					update = true;
					had_passwd = (xurl->password != 0);
					break;
				}
				li = list_search(gvBookmarks, (listsearchfunc)urlcmp, url);
				if(li == 0)
					break;
			}
			if(li) /* overwrite bookmark (ie, delete old and create a new) */
				list_delitem(gvBookmarks, li);
			if(!url_isanon(url) && gvAutoBookmarkSavePasswd == false
			   && !had_passwd)
				url_setpassword(url, 0);
		} else {
			if(!url_isanon(url) && gvAutoBookmarkSavePasswd == false)
				url_setpassword(url, 0);
		}
		create_the_bookmark(url);
		if(!gvAutoBookmarkSilent) {
			if(update)
				printf(_("updated bookmark %s\n"), url->alias);
			else
				printf(_("created bookmark %s\n"), url->alias);
		}
	}
}

static void print_bookmark_syntax(void)
{
	printf(_("Handle bookmarks.  Usage:\n"
			 "  bookmark [options] [bookmark alias ...]\n"
			 "Options:\n"
			 "  -s, --save[=FILE]      save bookmarks to bookmarks FILE\n"
			 "  -e, --edit             edit bookmarks file with your $EDITOR\n"
			 "  -r, --read[=FILE]      re-read bookmarks FILE\n"
			 "  -d, --delete           delete specified bookmarks from file\n"
			 "  -l, --list             list bookmarks in memory\n"
			 "  -h, --help             show this help\n"));
}

void cmd_bookmark(int argc, char **argv)
{
	struct option longopts[] = {
		{"save", optional_argument, 0, 's'},
		{"edit", no_argument, 0, 'e'},
		{"read", optional_argument, 0, 'r'},
		{"delete", no_argument, 0, 'd'},
		{"list", no_argument, 0, 'l'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};
	int action = BM_BOOKMARK;
	int c;
	char *bmfile = 0;

	optind = 0;
	while((c=getopt_long(argc, argv, "s::er::dlh", longopts, 0)) != EOF) {
		switch(c) {
		  case 's':
			action = BM_SAVE;
			xfree(bmfile);
			bmfile = xstrdup(optarg);
			break;
		  case 'e':
			action = BM_EDIT;
			break;
		  case 'r':
			action = BM_READ;
			xfree(bmfile);
			bmfile = xstrdup(optarg);
			break;
		  case 'd':
			action = BM_DELETE;
			break;
		  case 'l':
			action = BM_LIST;
			break;
		  case 'h':
			print_bookmark_syntax();
		  default:
			return;
		}
	}

	if(action == BM_SAVE) {
		bookmark_save(bmfile);
		if(bmfile)
			printf(_("bookmarks saved in %s\n"), bmfile);
		else
			printf(_("bookmarks saved in %s/bookmarks\n"), gvWorkingDirectory);
		return;
	}
	if(action == BM_READ) {
		char *tmp = 0;
		int ret;
		list_clear(gvBookmarks);
		if(bmfile)
			ret = parse_rc(bmfile, true);
		else {
			asprintf(&tmp, "%s/bookmarks", gvWorkingDirectory);
			ret = parse_rc(tmp, false);
		}
		if(ret != -1)
			printf(_("bookmarks read from %s\n"), bmfile ? bmfile : tmp);
		xfree(tmp);
		return;
	}
	if(action == BM_EDIT) {
		char *cmdline;
		asprintf(&cmdline, "%s %s/bookmarks", gvEditor, gvWorkingDirectory);
		invoke_shell(cmdline);
		xfree(cmdline);
		return;
	}
	if(action == BM_LIST) {
		listitem *li;

		printf(_("%u bookmarks present in memory\n"),
			   list_numitem(gvBookmarks));
		for(li = gvBookmarks->first; li; li=li->next) {
			url_t *u = (url_t *)li->data;
			/* note: all info not printed */
			printf("%-20s", u->alias ? u->alias : u->hostname);
			printf("%s@%s", u->username, u->hostname);
			if(u->directory)
				printf("/%s", shortpath(u->directory, 30, 0));
			putchar('\n');
		}
		return;
	}
	if(action == BM_DELETE) {
		int i;
		bool del_done = false;

		for(i = optind; i < argc; i++) {
			listitem *li;

			while((li = list_search(gvBookmarks,
									(listsearchfunc)urlcmp_name,
									argv[i])) != 0)
			{
				url_t *u = (url_t *)li->data;
				printf(_("deleted bookmark %s\n"),
					   u->alias ? u->alias : u->hostname);
				list_delitem(gvBookmarks, li);
				del_done = true;
			}
		}
		
		if(del_done) {
			bookmark_save(0);
			printf(_("bookmarks saved in %s/bookmarks\n"), gvWorkingDirectory);
		}

		return;
	}

	maxargs(1);

	need_connected();
	need_loggedin();

	create_bookmark(argc > 1 ? argv[1] : 0);
}
