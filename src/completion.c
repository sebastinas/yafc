/* $Id: completion.c,v 1.10 2003/07/12 10:25:41 mhe Exp $
 *
 * completion.c -- readline completion functions
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
#include "strq.h"
#include "alias.h"
#include "commands.h"
#include "input.h"
#include "gvars.h"
#include "set.h"
#include "cmd.h"
#include "url.h"
#include "bashline.h"

cpl_t force_completion_type = cpUnset;

#ifdef HAVE_LIBREADLINE

static bool remote_dir_only = false;

char *no_completion_function(const char *text, int state)
{
	return 0;
}

static char *alias_completion_function(const char *text, int state)
{
    static int len;
	alias *ap;
	static listitem *lip = 0;

	if(!gvAliases)
		return 0;

    if(!state) {
		len = strlen(text);
		lip = gvAliases->first;
	}

    while(lip) {
		ap = (alias *)lip->data;
		lip = lip->next;
		if(strncmp(ap->name, text, len) == 0)
			return xstrdup(ap->name);
    }
    return 0;
}

static char *bookmark_complete_internal(const char *text, int state,
										bool skip_domains)
{
    static int len;
	url_t *url;
	static listitem *lip = 0;

	if(!gvBookmarks)
		return 0;

    if(!state) {
		len = strlen(text);
		lip = gvBookmarks->first;
	}

    while(lip) {
		url = (url_t *)lip->data;
		lip = lip->next;
		if(skip_domains && url->hostname[0] == '.')
			continue;
		if(url->alias) {
			if(strncmp(url->alias, text, len) == 0)
				return xstrdup(url->alias);
		} else if(strncmp(url->hostname, text, len) == 0)
				return xstrdup(url->hostname);
    }
    return 0;
}

static char *bookmark_completion_function(const char *text, int state)
{
	return bookmark_complete_internal(text, state, false);
}


static char *hostname_completion_function(const char *text, int state)
{
	return bookmark_complete_internal(text, state, true);
}

/* Generator function for command completion.  STATE lets us know whether
 * to start from scratch; without any state (i.e. STATE == 0), then we
 * start at the top of the list.
 */
static char *command_completion_function(const char *text, int state)
{
    static int list_index, len;
	static int alias_state;
    char *e;

    if(!state) {
		list_index = 0;
		len = strlen(text);
		alias_state = 0;
    }

    /* Return the next name which partially matches from the command list. */
    while((e = cmds[list_index].cmd) != 0) {
		list_index++;
		if(strncasecmp(e, text, len) == 0)
			return xstrdup(e);
    }
	e = alias_completion_function(text, alias_state);
	alias_state = 1;
	return e;
}

static char *remote_completion_function(const char *text, int state)
{
    static int len;            /* length of unquoted */
	static char *dir;          /* any initial directory in text */
    const char *name;
	static char *unquoted = 0; /* the unquoted filename (or beginning of it) */
	rfile *fp = 0;
	static listitem *lip = 0;
	static rdirectory *rdir = 0; /* the cached remote directory */
	static char *merge_fmt = "%s/%s";

	if(!ftp_loggedin())
		return 0;

	/* this is not really true, this is for local filename completion,
	 * but it works here too (sort of), and it looks nicer, since
	 * the whole path is not printed by readline, ie
	 * only foo is printed and not /bar/fu/foo (if cwd == /bar/fu)
	 * readline appends a class character (ie /,@,*) in _local_ filenames
	 */
	rl_filename_completion_desired = 1;
#if (HAVE_LIBREADLINE >= 210)
	rl_filename_quoting_desired = 1;
#endif

	if(!state) {
		bool dir_is_cached;
		char *ap;

		dir = base_dir_xptr(text);
		if(dir) {
			char *e;

			stripslash(dir);
			e = strchr(dir, 0);
			if(e[-1]=='\"')
				e[-1] = 0;
			unquote(dir);
			if(strcmp(dir, "/") == 0)
				merge_fmt = "%s%s";
			else
				merge_fmt = "%s/%s";
		}
		if(gvWaitingDots) {
			rl_insert_text("..."); /* show dots while waiting, like ncftp */
			rl_redisplay();
		}
		
		ap = ftp_path_absolute(dir);
		rdir = ftp_cache_get_directory(ap);
		dir_is_cached = (rdir != 0);
		if(!rdir)
			rdir = ftp_read_directory(ap);
		free(ap);

		if(gvWaitingDots)
			rl_do_undo(); /* remove the dots */

		if(!dir_is_cached && ftp_get_verbosity() >= vbCommand)
			rl_forced_update_display();

		if(!rdir) {
			free(dir);
			return 0;
		}
		unquoted = bash_dequote_filename(base_name_ptr(text), 0);
		if(!unquoted)
			unquoted = (char *)xmalloc(1);
		len = strlen(unquoted);
		lip = rdir->files->first;
	}

	while(lip) {
		int isdir; /* 0 = not dir, 1 = dir, 2 = link (maybe dir) */

		fp = (rfile *)lip->data;
		lip = lip->next;

		isdir = ftp_maybe_isdir(fp);

		if(remote_dir_only && isdir == 0)
			continue;

		name = base_name_ptr(fp->path);

		/* skip dotdirs in completion */
		if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
			continue;

		if(strncmp(name, unquoted, len) == 0) {
			char *ret;
			if(dir)
				asprintf(&ret, merge_fmt, dir, name);
			else
				ret = xstrdup(name);
			if(isdir == 1) {
#if (HAVE_LIBREADLINE >= 210)
				rl_completion_append_character =  '/';
#else
				char *tmp;
				tmp = ret;
				asprintf(&ret, "%s/", tmp);
				free(tmp);
#endif
			} else {
#if (HAVE_LIBREADLINE >= 210)
				rl_completion_append_character =  ' ';
#endif
			}
			return ret;
		}
    }
	free(unquoted);
	free(dir);
	return 0;
}

static char *variable_completion_function(const char *text, int state)
{
    static int list_index, len;
    char *name;

    if(!state) {
		list_index = 0;
		len = strlen(text);
    }

    while((name = setvariables[list_index].name) != 0) {
		list_index++;
		if(strncmp(name, text, len) == 0)
			return xstrdup(name);
    }
    return 0;
}

static char *ftplist_completion_function(const char *text, int state)
{
	static int len;
	static listitem *lip = 0;

	if(!state) {
		len = strlen(text);
		lip = gvFtpList->first;
	}

	while(lip) {
		char *name;
		Ftp *f = (Ftp *)lip->data;
		lip = lip->next;
		if(f->url == 0)
			/* Ftp not connected */
			continue;
		name = f->url->alias ? f->url->alias : f->url->hostname;
		if(strncmp(name, text, len) == 0)
			return bash_backslash_quote(name);
	}
	return 0;
}

static char *taglist_completion_function(const char *text, int state)
{
	static int len;
	static listitem *lip = 0;

	if(!ftp->taglist)
		return 0;

	if(!state) {
		len = strlen(text);
		lip = ftp->taglist->first;
	}

	rl_filename_completion_desired = 1;
#if (HAVE_LIBREADLINE >= 210)
	rl_filename_quoting_desired = 1;
#endif

	while(lip) {
		rfile *f = (rfile *)lip->data;
		lip = lip->next;
		if(strncmp(f->path, text, len) == 0)
			return xstrdup(f->path);
	}
	return 0;
}

static char *local_taglist_completion_function(const char *text, int state)
{
	static int len;
	static listitem *lip = 0;

	if(!gvLocalTagList)
		return 0;

	if(!state) {
		len = strlen(text);
		lip = gvLocalTagList->first;
	}

	rl_filename_completion_desired = 1;
#if (HAVE_LIBREADLINE >= 210)
	rl_filename_quoting_desired = 1;
#endif

	while(lip) {
		char *p = (char *)lip->data;
		lip = lip->next;
		if(strncmp(p, text, len) == 0)
			return xstrdup(p); /* bash_backslash_quote(p); */
	}
	return 0;
}

/* Attempt to complete on the contents of TEXT.  START and END bound the
 * region of rl_line_buffer that contains the word to complete.  TEXT is
 * the word to complete.  We can use the entire contents of rl_line_buffer
 * in case we want to do some simple parsing.  Return the array of matches,
 * or NULL if there aren't any.
 */
char **the_complete_function(char *text, int start, int end)
{
    char **matches = 0;
	cmd_t *cp;
	char *e, *c, *orgc;
	cpl_t cpl;
	char quoted;

#if (HAVE_LIBREADLINE >= 210)
	rl_completion_append_character = ' ';
#endif

    if (start == end)
        return NULL;

	if(force_completion_type == cpUnset) {
		int i;
		int cmd_start;

		orgc = c = xstrdup(rl_line_buffer);

		if(c) {
			/* find the start index of the command to complete */
			i = start;
			while(c[i] != ';' && i>0)
				i--;
			cmd_start = i;
			if(c[i] == ';')
				cmd_start++;
		} else
			/* rl_line_buffer is empty, no command yet */
			cmd_start = start;

		/* is this an argument to a command? */
		if(start != cmd_start) {
			if(c[cmd_start] == '!')
				cpl = cpLocalFile;
			else {
				c += cmd_start;
				e = strqsep(&c, ' ');
				cp = find_func(e, false);
				if(!cp) {
					free(orgc);
					return 0;
				}
				cpl = cp->cpltype;
			}
		} else
			cpl = cpCommand;
		free(orgc);
	} else
		cpl = force_completion_type;

	quoted = ((char_is_quoted(rl_line_buffer, start) &&
			   strchr(rl_completer_quote_characters, rl_line_buffer[start-1]))
			  ? rl_line_buffer[start-1] : 0);
	text = bash_dequote_filename(text, quoted);

	remote_dir_only = false;
	switch(cpl) {
	  case cpUnset:
	  case cpNone:
		break;
	  case cpCommand:
		matches = rl_completion_matches(text, command_completion_function);
		break;
	  case cpLocalFile:
		matches = rl_completion_matches(text, rl_filename_completion_function);
		break;
	  case cpRemoteDir:
		remote_dir_only = true;
		/* fall through */
	  case cpRemoteFile:
		if(!gvRemoteCompletion)
			break;
		matches = rl_completion_matches(text, remote_completion_function);
		break;
	  case cpHostname:
		matches = rl_completion_matches(text, hostname_completion_function);
		break;
	  case cpBookmark:
		matches = rl_completion_matches(text, bookmark_completion_function);
		break;
	  case cpAlias:
		matches = rl_completion_matches(text, alias_completion_function);
		break;
	  case cpVariable:
		matches = rl_completion_matches(text, variable_completion_function);
		break;
	  case cpFtpList:
		matches = rl_completion_matches(text, ftplist_completion_function);
		break;
	  case cpTaglist:
		matches = rl_completion_matches(text, taglist_completion_function);
		break;
	  case cpLocalTaglist:
		matches = rl_completion_matches(text,
										local_taglist_completion_function);
		break;
	}
	free(text);
    return matches;
}

#endif /* HAVE_LIBREADLINE */
