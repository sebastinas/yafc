/* $Id: rc.c,v 1.19 2002/12/02 12:22:26 mhe Exp $
 *
 * rc.c -- config file parser + autologin lookup
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
#include "gvars.h"
#include "args.h"
#include "alias.h"
#include "commands.h"
#include "transfer.h"
#include "utils.h"

static int nerr = 0;

static char *current_rcfile = 0;

static void errp(char *str, ...)
{
	va_list ap;

	if(nerr == 0)
		fprintf(stderr, _("Error(s) while parsing '%s':\n"), current_rcfile);

	va_start(ap, str);
	vfprintf(stderr, str, ap);
	va_end(ap);
	nerr++;
}

static char *ungetstr = 0;

static char *_nextstr(FILE *fp)
{
	static char tmp[257];
	char *e;
	int c;

	if(ungetstr) {
		e = ungetstr;
		ungetstr = 0;
		return e;
	}

	if(fscanf(fp, "%256s", tmp) == EOF) {
		if(ferror(fp))
			perror(current_rcfile);
		return 0;
	}
	if(tmp[0] == '#') { /* skip comments */
		while((c = getc(fp)) != EOF) {
			if(c == '\n')
				break;
		}
		return _nextstr(fp);
	}
	return tmp;
}

static char *nextstr(FILE *fp)
{
	static char tmp[257];
	char *e;
	int i;

	e = _nextstr(fp);
	if(!e)
		return 0;
	if(*e=='\"' || *e=='\'') {
		strcpy(tmp, e+1);
		i = strlen(tmp);
		if(i >= 1 && (tmp[i-1] == '\'' || tmp[i-1] == '\"')) {
			tmp[i-1] = 0;
			return tmp;
		}
		while(true) {
			int c;

			tmp[i] = 0;

			c = fgetc(fp);
			if(c == EOF) {
				errp(_("unmatched quote\n"));
				break;
			}
			if((i == 0 || tmp[i-1] != '\\') && (c == '\"' || c == '\''))
				break;
			tmp[i++] = c;
			if(i == 256) {
				errp(_("string too long or unmatched quote, truncated\n"));
				break;
			}
		}
		return tmp;
	}
	return e;
}

static bool nextbool(FILE *fp)
{
	char *e;
	int b;

	if((e=nextstr(fp)) == NULL) {
		errp(_("Unexpected end of file encountered\n"));
		return false;
	}

	if((b=str2bool(e)) != -1)
		return (bool)b;
	else {
		errp(_("Expected boolean value, but got '%s'\n"), e);
		ungetstr = e;
		return false;
	}
}

#define NEXTSTR       if((e=nextstr(fp)) == 0) break

#define TRIG_MACHINE 1
#define TRIG_LOCAL 2
#define TRIG_DEFAULT 3

static void parse_host(int trig, FILE *fp)
{
	static bool has_warned_about_passwd = false;
	char *e;
	url_t *up;

	up = url_create();

	if(trig == TRIG_MACHINE) {
		if((e=nextstr(fp)) == 0)
			return;

		if(e[0] == 0) {
			errp(_("'machine' directive needs a hostname\n"));
			url_destroy(up);
			return;
		}
		url_parse(up, e);
	}

	while(!feof(fp)) {
		NEXTSTR;

		if(strcasecmp(e, "login") == 0) {
			NEXTSTR;
			url_setusername(up, e);
		} else if(strcasecmp(e, "alias") == 0) {
			NEXTSTR;
			if(up->hostname[0] == '.')
				printf(_("'alias' directive not useful with domains\n"));
			else {
				unquote(e);
				url_setalias(up, e);
			}
		} else if(strcasecmp(e, "password") == 0) {
			NEXTSTR;
			url_setpassword(up, e);
			if(!has_warned_about_passwd) {
				struct stat sb;
				has_warned_about_passwd = true;
				if(fstat(fileno(fp), &sb)==0 && (sb.st_mode & 077)!=0) {
					printf(_("WARNING! Config file contains passwords "
							 "but is readable by others (mode %03o)\n"),
						   sb.st_mode&0777);
					sleep(3);
				}
			}

		} else if(strcasecmp(e, "anonymous") == 0) {
			url_setusername(up, e);
			url_setpassword(up, gvAnonPasswd);    /* could be NULL */
		} else if(strcasecmp(e, "account") == 0) {
			NEXTSTR;
			/* FIXME: account skipped in parse_host() */
		} else if(strcasecmp(e, "cwd") == 0) {
			NEXTSTR;
			url_setdirectory(up, e);
		} else if(strcasecmp(e, "port") == 0) {
			NEXTSTR;
			url_setport(up, atoi(e));
		} else if(strcasecmp(e, "mech") == 0) {
			NEXTSTR;
			url_setmech(up, e);
		} else if(strcasecmp(e, "prot") == 0) {
			NEXTSTR;
			url_setprotlevel(up, e);
		} else if(strcasecmp(e, "passive") == 0) {
			bool b;
			NEXTSTR;
			b = str2bool(e);
			if(b != -1)
				url_setpassive(up, b);
		} else if(strcasecmp(e, "sftp") == 0) {
			NEXTSTR;
			url_setsftp(up, e);
		} else if(strcasecmp(e, "noupdate") == 0) {
			up->noupdate = true;
		} else if(strcasecmp(e, "macdef") == 0) {
			while(e) { /* FIXME: macdef: this is not really true */
				NEXTSTR;
			}
			break;
		} else {
			ungetstr = e;
			clearerr(fp);
			break;
		}
	}

	if(trig == TRIG_MACHINE) {
		listitem *li;
		li = list_search(gvBookmarks, (listsearchfunc)urlcmp, up);
		if(li)
			/* bookmark already exists, overwrite it (delete and create new) */
			list_delitem(gvBookmarks, li);
		list_additem(gvBookmarks, up);
	} else if(trig == TRIG_LOCAL) {
		if(gvLocalUrl)
			url_destroy(gvLocalUrl);
		gvLocalUrl = up;
	} else { /* trig == TRIG_DEFAULT */
		if(gvDefaultUrl)
			url_destroy(gvDefaultUrl);
		gvDefaultUrl = up;
	}
}

int parse_rc(const char *file, bool warn)
{
	FILE *fp;
	char *e;

	e = tilde_expand_home(file, gvLocalHomeDir);
	fp = fopen(e, "r");
	if(!fp) {
		if(warn)
			perror(e);
		xfree(e);
		return -1;
	}
	current_rcfile = e;

	nerr = 0;

	while(!feof(fp)) {
		if(nerr>20) {
			errp(_("As a computer, I find your faith in technology amusing..."
				   "\nToo many errors\n"));
			fclose(fp);
			xfree(current_rcfile);
			return -1;
		}

		NEXTSTR;

		if(strcasecmp(e, "autologin") == 0)
			gvAutologin = nextbool(fp);
		else if(strcasecmp(e, "autoreconnect") == 0)
			gvAutoReconnect = nextbool(fp);
		else if(strcasecmp(e, "verbose") == 0)
			gvVerbose = nextbool(fp);
		else if(strcasecmp(e, "debug") == 0)
			gvDebug = nextbool(fp);
		else if(strcasecmp(e, "trace") == 0)
			gvTrace = nextbool(fp);
		else if(strcasecmp(e, "inhibit_startup_syst") == 0
				|| strcasecmp(e, "no_startup_syst") == 0)
			gvStartupSyst = !nextbool(fp);
		else if(strcasecmp(e, "prompt_on_disconnect") == 0)
			/* ignored for backward compat. */ nextbool(fp);
		else if(strcasecmp(e, "remote_completion") == 0)
			gvRemoteCompletion = nextbool(fp);
		else if(strcasecmp(e, "read_netrc") == 0)
			gvReadNetrc = nextbool(fp);
		else if(strcasecmp(e, "quit_on_eof") == 0)
			gvQuitOnEOF = nextbool(fp);
		else if(strcasecmp(e, "use_passive_mode") == 0)
			gvPasvmode = nextbool(fp);
		else if(strcasecmp(e, "use_history") == 0)
			gvUseHistory = nextbool(fp);
		else if(strcasecmp(e, "beep_after_long_command") == 0)
			gvBeepLongCommand = nextbool(fp);
		else if(strcasecmp(e, "auto_bookmark_save_passwd") == 0)
			gvAutoBookmarkSavePasswd = nextbool(fp);
		else if(strcasecmp(e, "auto_bookmark_silent") == 0)
			gvAutoBookmarkSilent = nextbool(fp);
		else if(strcasecmp(e, "tilde") == 0)
			gvTilde = nextbool(fp);
		else if(strcasecmp(e, "reverse_dns") == 0)
			gvReverseDNS = nextbool(fp);
		else if(strcasecmp(e, "waiting_dots") == 0)
			gvWaitingDots = nextbool(fp);
		else if(strcasecmp(e, "use_env_string") == 0)
			gvUseEnvString = nextbool(fp);
		else if(strcasecmp(e, "auto_bookmark") == 0) {
			NEXTSTR;

			if(strcasecmp(e, "ask") == 0)
				gvAutoBookmark = 2;
			else {
				gvAutoBookmark = str2bool(e);
				if(gvAutoBookmark == -1) {
					errp(_("Expected boolean value or 'ask', but got '%s'\n"),
						 e);
					gvAutoBookmark = 0;
				}
			}
		} else if(strcasecmp(e, "auto_bookmark_update") == 0) {
			NEXTSTR;

			if(strcasecmp(e, "ask") == 0)
				gvAutoBookmarkUpdate = 2;
			else {
				gvAutoBookmarkUpdate = str2bool(e);
				if(gvAutoBookmarkUpdate == -1) {
					errp(_("Expected boolean value or 'ask', but got '%s'\n"),
						 e);
					gvAutoBookmarkUpdate = 0;
				}
			}
		} else if(strcasecmp(e, "load_taglist") == 0) {
			NEXTSTR;

			if(strcasecmp(e, "ask") == 0)
				gvLoadTaglist = 2;
			else {
				gvLoadTaglist = str2bool(e);
				if(gvLoadTaglist == -1) {
					errp(_("Expected boolean value or 'ask', but got '%s'\n"),
						 e);
					gvLoadTaglist = 2;
				}
			}
		} else if(strcasecmp(e, "default_type") == 0) {
			NEXTSTR;
			if(strcasecmp(e, "binary") == 0 || strcasecmp(e, "I") == 0)
				gvDefaultType = tmBinary;
			else if(strcasecmp(e, "ascii") == 0 || strcasecmp(e, "A") == 0)
				gvDefaultType = tmAscii;
			else
				errp(_("Unknown default_type parameter '%s'..."
					   " (use 'ascii' or 'binary')\n"), e);
		} else if(strcasecmp(e, "default_mechanism") == 0) {
			NEXTSTR;
			list_free(gvDefaultMechanism);
			gvDefaultMechanism = list_new((listfunc)xfree);
			listify_string(e, gvDefaultMechanism);
		} else if(strcasecmp(e, "anon_password") == 0) {
			NEXTSTR;
			xfree(gvAnonPasswd);
			gvAnonPasswd = xstrdup(e);
		} else if(strcasecmp(e, "long_command_time") == 0) {
			NEXTSTR;
			gvLongCommandTime = atoi(e);
			if(gvLongCommandTime < 0) {
				errp(_("Invalid value for long_command_time: %d\n"),
					 gvLongCommandTime);
				gvLongCommandTime = 30;
			}
		} else if(strcasecmp(e, "connect_wait_time") == 0) {
			NEXTSTR;
			gvConnectWaitTime = atoi(e);
			if(gvConnectWaitTime < 0) {
				errp(_("Invalid value for connect_wait_time: %d\n"),
					 gvConnectWaitTime);
				gvConnectWaitTime = 30;
			}
		} else if(strcasecmp(e, "cache_timeout") == 0) {
			NEXTSTR;
			gvCacheTimeout = atoi(e);
			if(gvCacheTimeout < 0) {
				errp(_("Invalid value for cache_timeout: %d\n"),
					 gvCacheTimeout);
				gvCacheTimeout = 0;
			}
		} else if(strcasecmp(e, "connect_attempts") == 0) {
			NEXTSTR;
			gvConnectAttempts = (unsigned)atoi(e);
			if(gvConnectAttempts == 0)
				gvConnectAttempts = 1;
		} else if(strcasecmp(e, "command_timeout") == 0) {
			NEXTSTR;
			gvCommandTimeout = (unsigned)atoi(e);
		} else if(strcasecmp(e, "connection_timeout") == 0) {
			NEXTSTR;
			gvConnectionTimeout = (unsigned)atoi(e);
		} else if(strcasecmp(e, "include") == 0) {
			char *rcfile;
			NEXTSTR;
			rcfile = tilde_expand_home(e, gvLocalHomeDir);
			if(strcmp(rcfile, current_rcfile) == 0) {
				xfree(rcfile);
				errp(_("Skipping circular include statement: %s\n"), e);
			} else {
				xfree(current_rcfile);
				parse_rc(e, true);
				current_rcfile = rcfile;
			}
		} else if(strcasecmp(e, "prompt1") == 0) {
			NEXTSTR;
			xfree(gvPrompt1);
			gvPrompt1 = xstrdup(e);
		} else if(strcasecmp(e, "prompt2") == 0) {
			NEXTSTR;
			xfree(gvPrompt2);
			gvPrompt2 = xstrdup(e);
		} else if(strcasecmp(e, "prompt3") == 0) {
			NEXTSTR;
			xfree(gvPrompt3);
			gvPrompt3 = xstrdup(e);
		} else if(strcasecmp(e, "ssh_program") == 0) {
			NEXTSTR;
			xfree(gvSSHProgram);
			gvSSHProgram = xstrdup(e);
		} else if(strcasecmp(e, "sftp_server_program") == 0) {
			NEXTSTR;
			xfree(gvSFTPServerProgram);
			gvSFTPServerProgram = xstrdup(e);
		} else if(strcasecmp(e, "xterm_title_terms") == 0) {
			NEXTSTR;
			xfree(gvXtermTitleTerms);
			gvXtermTitleTerms = xstrdup(e);
		} else if(strcasecmp(e, "xterm_title1") == 0) {
			NEXTSTR;
			xfree(gvXtermTitle1);
			gvXtermTitle1 = xstrdup(e);
		} else if(strcasecmp(e, "xterm_title2") == 0) {
			NEXTSTR;
			xfree(gvXtermTitle2);
			gvXtermTitle2 = xstrdup(e);
		} else if(strcasecmp(e, "xterm_title3") == 0) {
			NEXTSTR;
			xfree(gvXtermTitle3);
			gvXtermTitle3 = xstrdup(e);
		} else if(strcasecmp(e, "transfer_begin_string") == 0) {
			NEXTSTR;
			xfree(gvTransferBeginString);
			gvTransferBeginString = xstrdup(e);
			unquote_escapes(gvTransferBeginString);
		} else if(strcasecmp(e, "transfer_string") == 0) {
			NEXTSTR;
			xfree(gvTransferString);
			gvTransferString = xstrdup(e);
			unquote_escapes(gvTransferString);
		} else if(strcasecmp(e, "transfer_xterm_string") == 0) {
			NEXTSTR;
			xfree(gvTransferXtermString);
			gvTransferXtermString = xstrdup(e);
			unquote_escapes(gvTransferXtermString);
		} else if(strcasecmp(e, "transfer_end_string") == 0) {
			NEXTSTR;
			xfree(gvTransferEndString);
			gvTransferEndString = xstrdup(e);
			unquote_escapes(gvTransferEndString);
		} else if(strcasecmp(e, "nohup_mailaddress") == 0) {
			NEXTSTR;
			xfree(gvNohupMailAddress);
			gvNohupMailAddress = xstrdup(e);
		} else if(strcasecmp(e, "sendmail_path") == 0) {
			NEXTSTR;
			xfree(gvSendmailPath);
			gvSendmailPath = xstrdup(e);
		} else if(strcasecmp(e, "history_max") == 0) {
			NEXTSTR;
			gvHistoryMax = atoi(e);
			if(gvHistoryMax <= 0) {
				errp(_("Invalid value for history_max: %d\n"), gvHistoryMax);
				gvHistoryMax = 256;
			}
		} else if(strcasecmp(e, "ascii_transfer_mask") == 0) {
			NEXTSTR;
			listify_string(e, gvAsciiMasks);
		} else if(strcasecmp(e, "transfer_first_mask") == 0) {
			NEXTSTR;
			listify_string(e, gvTransferFirstMasks);
		} else if(strcasecmp(e, "startup_local_directory") == 0) {
			NEXTSTR;
			e = tilde_expand_home(e, gvLocalHomeDir);
			if(chdir(e) == -1)
				perror(e);
			else
				cmd_lpwd(0, 0);
			xfree(e);
		} else if(strcasecmp(e, "alias") == 0) {
			args_t *args;
			char *name;

			NEXTSTR;
			name = xstrdup(e);

			NEXTSTR;

			args = args_create();
			args_push_back(args, e);
			alias_define(name, args);
			xfree(name);
		}
		else if(strcasecmp(e, "proxy_type") == 0) {
			NEXTSTR;

			gvProxyType = atoi(e);
			if(gvProxyType < 0 || gvProxyType > 6) {
				errp(_("Invalid value for proxy_type: %d\n"), gvProxyType);
				gvProxyType = 0;
			}
		} else if(strcasecmp(e, "proxy_host") == 0) {
			NEXTSTR;
			url_destroy(gvProxyUrl);
			gvProxyUrl = url_init(e);
		} else if(strcasecmp(e, "proxy_exclude") == 0) {
			NEXTSTR;
			listify_string(e, gvProxyExclude);
		}
		else if(strcasecmp(e, "machine") == 0)
			parse_host(TRIG_MACHINE, fp);
		else if(strcasecmp(e, "default") == 0)
			parse_host(TRIG_DEFAULT, fp);
		else if(strcasecmp(e, "local") == 0)
			parse_host(TRIG_LOCAL, fp);
		else
			errp(_("Config parse error: '%s'\n"), e);
	}
	fclose(fp);
	xfree(current_rcfile);
	return 0;
}

static url_t *get_autologin_url_short(const char *host)
{
	url_t *x, *found = 0;
	listitem *li;

	li = gvBookmarks->first;
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
	li = gvBookmarks->first;
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
		li = gvBookmarks->first;
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
