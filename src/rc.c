/* rc.c -- config file parser + autologin lookup
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
#include "strq.h"
#include "gvars.h"
#include "args.h"
#include "alias.h"
#include "commands.h"
#include "transfer.h"
#include "cfglib/cfglib.h"

static cfglib_value_list_t *values = 0;

const char *rc_str(const char *optname)
{
	return cfglib_getstring(optname, values);
}

bool rc_bool(const char *optname)
{
	return cfglib_getbool(optname, values);
}

int rc_int(const char *optname)
{
	return cfglib_getint(optname, values);
}

cfglib_list_t *rc_list(const char *optname)
{
	return cfglib_getlist(optname, values);
}

#if 0
static void passwd_warn(const char *filename)
{
	if(!has_warned_about_passwd) {
		struct stat sb;
		has_warned_about_passwd = true;
		if(stat(filename, &sb) == 0 && (sb.st_mode & 077) != 0)
			printf(_("WARNING! Config file contains passwords "
					 "but is readable by others (mode %03o)\n"),
				   sb.st_mode&0777);
	}
}
#endif

int parse_rc(const char *file, bool warn)
{
	FILE *fp;
	char *e;

	cfglib_option_t machine_opts[] = {
		CFG_STR("host", 0, 0, 0, 0),
		CFG_STR("login", 0, 0, 1, 0),
		CFG_STR("alias",0, 0, 2, 0),
		CFG_STR("password",0,0, 3, 0),
		CFG_BOOL("passive", 0, 0, 4, 0),
		CFG_STR("cwd", 0, 0, 5, 0),
		CFG_TOGGLE("anonymous", 0, 0, 6, 0),
		CFG_STR("prot", 0, 0, 7, 0),
		CFG_STR("mechanism", CFGF_LIST, 0, 8, 0),
		CFG_STR("account", 0, 0, 9, 0),
		CFG_INT("port", 0, 0, 10, 0),
		CFG_ENDOPT
	};

	cfglib_option_t alias_opts[] = {
		CFG_STR(0 /* all names valid */, 0, 0, 0, 0),
		CFG_ENDOPT
	};

	cfglib_option_t proxy_opts[] = {
		CFG_INT("type", 0, 0, 0, 0),
		CFG_STR("host", 0, 0, 17, 0),
		CFG_STR("exclude", CFGF_LIST, 0, 0, 0),
		CFG_ENDOPT
	};

	cfglib_option_t bookmark_opts[] = {
		CFG_SUB(0 /* all names valid */, 0, 0, 0, machine_opts, ""),
		CFG_ENDOPT
	};

	cfglib_option_t opts[] = {
		/* boolean boolean */
		CFG_BOOL("autologin", 0, 0, 0,
				"attempt to login automagically, using the autologin " \
				"entries below"),
		CFG_BOOL("quit_on_eof", 0, 0, 1, "quit program if " \
				"received EOF (C-d)"),
		CFG_BOOL("use_passive_mode", 0, 0, 2,
				"use passive mode connections\n" \
				"if false, use sendport mode\n" \
				"if you get the message 'Host doesn't support passive " \
				"mode', set to false"),
		CFG_BOOL("read_netrc", 0, 0, 3, "read netrc"),
		CFG_BOOL("verbose", 0, 0, 4, ""),
		CFG_BOOL("debug", 0, 0, 5, ""),
		CFG_BOOL("trace", 0, 0, 6, ""),
		CFG_BOOL("inhibit_startup_syst", 0, 0, 7, ""),
		CFG_BOOL("use_env_string", 0, 0, 8, ""),
		CFG_BOOL("remote_completion", 0, 0, 9, ""),
		CFG_BOOL("auto_bookmark_save_passwd", 0, 0, 10, ""),
		CFG_BOOL("auto_bookmark_silent", 0, 0, 11, ""),
		CFG_BOOL("beep_after_long_command", 0, 0, 12, ""),
		CFG_BOOL("use_history", 0, 0, 13, ""),
		CFG_BOOL("tilde", 0, 0, 14, ""),

		CFG_STR("load_taglist", 0, 0, 15, ""),
		CFG_STR("auto_bookmark", 0, 0, 10, ""),

		/* integer options */
		CFG_INT("long_command_time", 0, 0, 0, ""),
		CFG_INT("history_max", 0, 0, 1, ""),
		CFG_INT("command_timeout", 0, 0, 2, ""),
		CFG_INT("connection_timeout", 0, 0, 3, ""),
		CFG_INT("connect_attempts", 0, 0, 4, ""),
		CFG_INT("connect_wait_time", 0, 0, 5, ""),
		CFG_INT("proxy_type", 0, 0, 6, ""),

		/* string options */
		CFG_STR("default_type", 0, 0, 0, ""),

		CFG_STR("default_mechanism", CFGF_LIST|CFGF_LIST_MERGE_MULTIPLES,
				0, 1, ""),
		CFG_STR("ascii_transfer_mask", CFGF_LIST|CFGF_LIST_MERGE_MULTIPLES,
				0, 2, ""),

		CFG_STR("prompt1", 0, 0, 0, ""),
		CFG_STR("prompt2", 0, 0, 1, ""),
		CFG_STR("prompt3", 0, 0, 2, ""),
		CFG_STR("xterm_title1", 0, 0, 3, ""),
		CFG_STR("xterm_title2", 0, 0, 4, ""),
		CFG_STR("xterm_title3", 0, 0, 5, ""),
		CFG_STR("transfer_begin_string", 0, 0, 6, ""),
		CFG_STR("transfer_string", 0, 0, 7, ""),
		CFG_STR("transfer_end_string", 0, 0, 8, ""),
		CFG_STR("anon_password", 0, 0, 9, ""),

		CFG_STR("xterm_title_terms", CFGF_LIST|CFGF_LIST_MERGE_MULTIPLES,
				0, 12, ""),

		/* suboptions */
		CFG_SUB("alias", 0, 0, 0, alias_opts,
				   "aliases (on the form [alias name value])\n" \
				   "can't make an alias of another alias"),
		CFG_SUB("proxy", 0, 0, 0, proxy_opts, "proxy settings"),

		CFG_SUB("bookmarks", CFGF_ALLOW_MULTIPLES, 0, 0, bookmark_opts, "bookmarks"),
		
		CFG_ENDOPT
	};

	e = tilde_expand_home(file, gvLocalHomeDir);

	fprintf(stderr, "cfglib_load(%s)\n", e);

	cfglib_load(e, opts, &values);

	return 0;
}

static url_t *get_autologin_url_short(const char *host)
{
	url_t *x, *found = 0;
	listitem *li;

	li = gvHostCompletion->first;
	while(li) {
		x = (url_t *)li->data;
		li = li->next;
		/* compare only strlen(host) chars, allowing aliases
		 * to be shortened, as long as they're not ambiguous
		 */
		if(x->alias && strncasecmp(x->alias, host, strlen(host)) == 0) {
			if(strlen(x->alias) == strlen(host))
				/* exact match */
				return x;
			if(found)
				found = (url_t *)-1;
			else
				found = x;
		}
	}
	if(found)
		return found;

	/* now do the same for hostnames (skip aliases) */
	li = gvHostCompletion->first;
	while(li) {
		x = (url_t *)li->data;
		li = li->next;
		/* compare only strlen(host) chars, allowing hostnames
		 * to be shortened, as long as they're not ambiguous
		 */
		if(!x->alias && strncasecmp(x->hostname, host, strlen(host)) == 0) {
			if(strlen(x->hostname) == strlen(host))
				/* exact match */
				return x;
			if(found)
				found = (url_t *)-1;
			else
				found = x;
		}
	}

	return found;
}

/* returns a copy of the found URL, should be deleted
 * returns 0 if not found, and -1 if ambiguous
 */
url_t *get_autologin_url(const char *host)
{
	listitem *li;
	const char *dot = host;
	url_t *x;

	x = get_autologin_url_short(host);
	if(x == (url_t *)-1)
		return x;
	if(x)
		return url_clone(x);

	/* try to match the 'local' directive */
	if(gvLocalUrl && strchr(host, '.') == 0) {
		x = url_clone(gvLocalUrl);
		url_sethostname(x, host);
		return x;
	}

	/* try to match as much as possible of the domain name */
	while((dot = strchr(dot, '.')) != 0) {
		li = gvHostCompletion->first;
		while(li) {
			x = (url_t *)li->data;

			if(x->hostname && strcasecmp(dot, x->hostname) == 0) {
				x = url_clone(x);
				url_sethostname(x, host);
				return x;
			}
			li = li->next;
		}
		dot++;
	}

	/* return default, or NULL if no default */
	if(gvDefaultUrl) {
		x = url_clone(gvDefaultUrl);
		url_sethostname(x, host);
		return x;
	}
	return 0;
}
