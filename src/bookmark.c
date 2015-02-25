/*
 * bookmark.c -- create bookmark(s)
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
#include "input.h"
#include "gvars.h"
#include "base64.h"
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
#define BM_TOGGLE_NOUPDATE 6

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
  if (url == gvLocalUrl)
    fprintf(fp, "local");
  else if (url == gvDefaultUrl)
    fprintf(fp, "default");
  else
  {
    fprintf(fp, "machine %s://%s",
        url->protocol ? url->protocol : "ftp", url->hostname);
    if (url->port != -1)
      fprintf(fp, ":%d", url->port);
    if (url->alias)
    {
      char *a = xquote_chars(url->alias, "\'\"");
      fprintf(fp, " alias '%s'", a);
      free(a);
    }
  }

  if (url_isanon(url) && url->password &&
      strcmp(url->password, gvAnonPasswd) == 0)
    fprintf(fp, "\n  anonymous");
  else if (url->username)
  {
    fprintf(fp, "\n  login %s", url->username);
    if (url->password)
    {
      if (url_isanon(url))
        fprintf(fp, " password %s", url->password);
      else
      {
        char *cq = NULL;
        if (b64_encode(url->password, strlen(url->password), &cq) != -1)
        {
          fprintf(fp, " password [base64]%s", cq);
          free(cq);
        }
      }
    }
  }

  if (url->directory)
    fprintf(fp, " cwd '%s'", url->directory);

  if (url->protlevel)
    fprintf(fp, " prot %s", url->protlevel);

  if (url->mech)
  {
    char *mech_string = stringify_list(url->mech);
    if (mech_string)
    {
      fprintf(fp, " mech '%s'", mech_string);
      free(mech_string);
    }
  }

  if (url->pasvmode != -1)
    fprintf(fp, " passive %s", url->pasvmode ? "true" : "false");

  if (url->sftp_server)
    fprintf(fp, " sftp %s", url->sftp_server);

  if (url->noupdate)
    fprintf(fp, " noupdate");

  fprintf(fp, "\n\n");
}

static int bookmark_save(const char *other_bmfile)
{
	char *bmfile = NULL;
	listitem *li;

	if(other_bmfile)
		bmfile = xstrdup(other_bmfile);
	else
		if (asprintf(&bmfile, "%s/bookmarks", gvWorkingDirectory) == -1)
    {
      fprintf(stderr, _("Failed to allocate memory.\n"));
      return -1;
    }
	FILE* fp = fopen(bmfile, "w");
	if(!fp) {
		perror(bmfile);
		free(bmfile);
		return -1;
	}

	fprintf(fp,
			"# this is an automagically created file\n"
			"# so don't bother placing comments here, they will be"
			" overwritten\n"
			"# make sure this file isn't world readable if passwords are"
			" stored here\n"
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
	free(bmfile);
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
	free(o);
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
		char *tmp = NULL;
		if (asprintf(&tmp, "%s/bookmarks", gvWorkingDirectory) == -1)
    {
      fprintf(stderr, _("Failed to allocate memory.\n"));
      return;
    }
		parse_rc(tmp, false);
		free(tmp);
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
	url_t *url;
	listitem *li;
	int a;
	char *alias;

	alias = xstrdup(guessed_alias);

	while(true) {
		if(!alias) {
			default_alias = guess_alias(ftp->url);

      force_completion_type = cpBookmark;
			if(default_alias)
				alias = input_read_string("alias (%s): ", default_alias);
			else
				alias = input_read_string("alias: ");

      if(!alias || !*alias)
				alias = default_alias;
			else
				free(default_alias);
		}

		url = url_clone(ftp->url);
		url_setalias(url, alias);
		free(alias);
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
	if(url->noupdate)
		return false;

	/* don't update bookmark if username has changed */
	if(url->username && strcmp(url->username, ftp->url->username) != 0)
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
	if(xstrcmp(url->protocol, ftp->url->protocol) != 0)
		return true;
	if(xstrcmp(url->sftp_server, ftp->url->sftp_server) != 0)
		return true;
	return false;
}

void auto_create_bookmark(void)
{
	listitem *li;
	int a;
	bool auto_create, had_passwd = false;
	bool update = false;

	if(gvAutoBookmark == 0 && gvAutoBookmarkUpdate == 0)
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
		if(gvAutoBookmarkUpdate == 2) {
			a = ask(ASKYES|ASKNO, ASKYES,
					_("Do you want to update the bookmark for this site?"));
			if(a == ASKNO)
				return;
		}

		update = true;
		auto_create = true;
		had_passwd = (((url_t *)li->data)->password != 0);
	}

	if(gvAutoBookmark == 2 && !auto_create) {
		a = ask(ASKYES|ASKNO, ASKYES,
				_("Do you want to create a bookmark for this site?"));
		if(a == ASKNO)
			return;
		create_bookmark(0);
	} else { /* auto autobookmark... */
		url_t* url = url_clone(ftp->url);
		char* a = guess_alias(ftp->url);
		url_setalias(url, a);
		free(a);

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

					update = false;
					char* par = strrchr(url->alias, '(');
          int ver = 1;
          if (par)
					{
						*par = 0;
						ver = atoi(par + 1);
					}
          if (asprintf(&a, "%s(%d)", url->alias, ver + 1) == -1)
          {
            fprintf(stderr, _("Failed to allocate memory.\n"));
            url_destroy(url);
            return;
          }
					url_setalias(url, a);
					free(a);
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
	show_help(_("Handle bookmarks."), "bookmark [options] [bookmark alias ...]",
	  _("  -s, --save[=FILE]      save bookmarks to bookmarks FILE\n"
			"  -e, --edit             edit bookmarks file with your $EDITOR\n"
			"  -r, --read[=FILE]      re-read bookmarks FILE\n"
			"  -d, --delete           delete specified bookmarks from file\n"
			"  -l, --list             list bookmarks in memory\n"
			"  -u, --noupdate         toggle noupdate flag\n"));
}

void cmd_bookmark(int argc, char **argv)
{
	struct option longopts[] = {
		{"save", optional_argument, 0, 's'},
		{"edit", no_argument, 0, 'e'},
		{"read", optional_argument, 0, 'r'},
		{"delete", no_argument, 0, 'd'},
		{"list", no_argument, 0, 'l'},
		{"noupdate", no_argument, 0, 'u'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};
	int action = BM_BOOKMARK;
	int c;
	char *bmfile = 0;

	optind = 0;
	while((c=getopt_long(argc, argv, "s::er::dluh", longopts, 0)) != EOF) {
		switch(c) {
			case 's':
				action = BM_SAVE;
				free(bmfile);
				bmfile = xstrdup(optarg);
				break;
			case 'e':
				action = BM_EDIT;
				break;
			case 'r':
				action = BM_READ;
				free(bmfile);
				bmfile = xstrdup(optarg);
				break;
			case 'd':
				action = BM_DELETE;
				break;
			case 'l':
				action = BM_LIST;
				break;
			case 'u':
				action = BM_TOGGLE_NOUPDATE;
				break;
			case 'h':
				print_bookmark_syntax();
				/* fall through */
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
			if (asprintf(&tmp, "%s/bookmarks", gvWorkingDirectory) == -1)
      {
        fprintf(stderr, _("Failed to allocate memory.\n"));
        return;
      }
			ret = parse_rc(tmp, false);
		}
		if(ret != -1)
			printf(_("bookmarks read from %s\n"), bmfile ? bmfile : tmp);
		free(tmp);
		return;
	}
	if(action == BM_EDIT) {
		invoke_shell("%s %s/bookmarks", gvEditor, gvWorkingDirectory);
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
			printf("%s://%s@%s", u->protocol ? u->protocol : "ftp",
				   u->username, u->hostname);
			if(u->directory)
			{
				char* sp = shortpath(u->directory, 30, 0);
				printf("/%s", sp);
				free(sp);
			}
			putchar('\n');
		}
		return;
	}
	if(action == BM_DELETE) {
		int i;
		bool del_done = false;

		minargs(optind);

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
	if(action == BM_TOGGLE_NOUPDATE) {
		int i;
		bool toggle_done = false;

		if(argc == optind) {
			listitem *li;

			need_connected();
			need_loggedin();

			li = list_search(gvBookmarks,
							 (listsearchfunc)urlcmp_name,
							 ftp->url->alias);
			if(li) {
				url_t *u = (url_t *)li->data;
				u->noupdate = !u->noupdate;
				printf(_("%s: noupdate: %s\n"),
					   u->alias ? u->alias : u->hostname,
					   u->noupdate ? _("yes") : _("no"));
				toggle_done = true;
			}

			ftp->url->noupdate = !ftp->url->noupdate;
			if(!toggle_done)
				printf(_("%s: noupdate: %s\n"),
					   ftp->url->alias ? ftp->url->alias : ftp->url->hostname,
					   ftp->url->noupdate ? _("yes") : _("no"));
		}

		for(i = optind; i < argc; i++) {
			listitem *li;

			li = list_search(gvBookmarks,
							 (listsearchfunc)urlcmp_name,
							 argv[i]);
			if(li) {
				url_t *u = (url_t *)li->data;
				u->noupdate = !u->noupdate;
				printf(_("%s: noupdate: %s\n"),
					   u->alias ? u->alias : u->hostname,
					   u->noupdate ? _("yes") : _("no"));
				toggle_done = true;
			}
		}

		if(toggle_done) {
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
