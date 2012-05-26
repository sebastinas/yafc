/*
 * prompt.c -- expands the prompt
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
#include "shortpath.h"
#include "gvars.h"

static int connection_number(void)
{
	int i = 1;
	listitem *li;

	for(li=gvFtpList->first; li; li=li->next, i++) {
		if(gvCurrentFtp == li)
			return i;
	}
	/* should not reach here! */
	return -1;
}

/*
 * %c number of connections open
 * %C number of the current connection
 * %u username
 * %h remote host name (as passed to open)
 * %H %h up to the first '.'
 * %m remote machine name (as returned by gethostbyname)
 * %M %m up to the first '.'
 * %n remote ip number
 * %[#]w current remote working directory
 * %W basename of %w
 * %[#]~ as %w but home directory is replaced with ~
 * %[#]l current local working directory
 * %% percent sign
 * %# a '#' if (local) user is root, else '$'
 * %{ begin sequence of non-printing chars, ie escape codes
 * %} end      -"-
 * \e escape (0x1B)
 * \x## character 0x## (hex)
 *
 * [#] means an optional (maximum) width specifier can be specified
 *  example: %32w
 */

char *expand_prompt(const char *fmt)
{
	char *prompt, *cp;
	unsigned maxlen;
	char *ins, *e;
	bool freeins;
	char *tmp;


	if(!fmt)
		return 0;

	prompt = (char *)xmalloc(strlen(fmt)+1);
	cp = prompt;

	while(fmt && *fmt) {
		if(*fmt == '%') {
			fmt++;
			ins = 0;
			freeins = false;
			maxlen = (unsigned)-1;
			if(isdigit((int)*fmt)) {
				maxlen = (unsigned)atoi(fmt);
				while(isdigit((int)*fmt))
					fmt++;
			}
			switch(*fmt) {
			  case 'c':
				asprintf(&ins, "%u", list_numitem(gvFtpList));
				freeins = true;
				break;
			  case 'C':
				asprintf(&ins, "%u", connection_number());
				freeins = true;
				break;
			  case 'u': /* username */
				ins = ftp_loggedin() ? ftp->url->username : "?";
				break;
			  case 'h': /* remote hostname (as passed to cmd_open()) */
				ins = ftp_connected() ? ftp->url->hostname : "?";
				break;
			  case 'H': /* %h up to first '.' */
				if(!ftp_connected()) {
					ins = "?";
					break;
				}
				e = strchr(ftp->url->hostname, '.');
				if(e) {
					ins = xstrndup(ftp->url->hostname, e - ftp->url->hostname);
					freeins = true;
				} else
					ins = ftp->url->hostname;
				break;
			  case 'm':
				/* remote machine name (as returned from gethostbynmame) */
				ins = xstrdup(ftp_connected() ? host_getoname(ftp->host) : "?");
        freeins = true;
				break;
			  case 'M': /* %m up to first '.' */
				if(!ftp_connected()) {
					ins = "?";
					break;
				}
				e = strchr(host_getoname(ftp->host), '.');
				if(e) {
					ins = xstrndup(host_getoname(ftp->host),
								   e - host_getoname(ftp->host));
				} else
					ins = xstrdup(host_getoname(ftp->host));
        freeins = true;
				break;
			case 'n': /* remote ip number */
				ins = xstrdup(ftp_connected() ? host_getoname(ftp->host) : "?");
        freeins = true;
				break;
			case 'w': /* current remote directory */
				if(!ftp_loggedin()) {
					ins = "?";
					break;
				}
				ins = shortpath(ftp->curdir, maxlen, 0);
				freeins = true;
				break;
			case 'W': /* basename(%w) */
				if(!ftp_loggedin()) {
					ins = "?";
					break;
				}
				ins = (char *)base_name_ptr(ftp->curdir);
				break;
			case '~': /* %w but homedir replaced with '~' */
				if(!ftp_loggedin()) {
					ins = "?";
					break;
				}
				ins = shortpath(ftp->curdir, maxlen, ftp->homedir);
				freeins = true;
				break;
			case 'l': /* current local directory */
				tmp = getcwd(NULL, 0);
				ins = shortpath(tmp, maxlen, 0);
				freeins = true;
				free(tmp);
				break;
			case 'L': /* basename(%l) */
				tmp = getcwd(NULL, 0);
				ins = (char *)base_name_ptr(tmp);
				free(tmp);
				break;
			case '%': /* percent sign */
				ins = "%";
				break;
			case '#': /* local user == root ? '#' : '$' */
				ins = getuid() == 0 ? "#" : "$";
				break;
			case '{': /* begin non-printable character string */
#ifdef HAVE_LIBREADLINE
				ins = "\001\001"; /* \001 + RL_PROMPT_START_IGNORE */
#endif
				break;
			case '}': /* end non-printable character string */
#ifdef HAVE_LIBREADLINE
				ins = "\001\002"; /* \001 + RL_PROMPT_END_IGNORE */
#endif
				break;
			case 'e': /* escape (0x1B) */
				ins = "\x1B";
				break;
			default: /* illegal format specifier */
				break;
			}

			if(ins) {
				char *tmp;
				tmp = (char *)xmalloc(strlen(prompt) + strlen(ins) +
									  strlen(fmt+1) + 1);
				strcpy(tmp, prompt);
				strcat(tmp, ins);
				cp = tmp + strlen(prompt) + strlen(ins);
				free(prompt);
				prompt = tmp;
				if(freeins)
					free(ins);
			}
		} else
			*cp++ = *fmt;

		fmt++;
	}
	unquote_escapes(prompt);
	return prompt;
}
