/* $Id: url.c,v 1.10 2003/07/12 10:25:41 mhe Exp $
 *
 * url.c -- splits an URL into its components
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

#include "url.h"
#include "xmalloc.h"
#include "strq.h"
#include "base64.h"

void listify_string(const char *str, list *lp);

url_t *url_create(void)
{
	url_t *urlp;

	urlp = (url_t *)xmalloc(sizeof(url_t));
	urlp->port = -1;
	urlp->pasvmode = -1;
	urlp->mech = list_new((listfunc)free);
	urlp->noupdate = false;

	return urlp;
}

url_t *url_init(const char *str)
{
	url_t *urlp = url_create();
	url_parse(urlp, str);
	return urlp;
}

url_t *url_clone(const url_t *urlp)
{
	url_t *cloned;

	cloned = (url_t *)xmalloc(sizeof(url_t));

	if(cloned) {
		cloned->protocol = xstrdup(urlp->protocol);
		cloned->hostname = xstrdup(urlp->hostname);
		cloned->alias = xstrdup(urlp->alias);
		cloned->username = xstrdup(urlp->username);
		cloned->password = xstrdup(urlp->password);
		cloned->directory = xstrdup(urlp->directory);
		cloned->protlevel = xstrdup(urlp->protlevel);
		cloned->port = urlp->port;
		cloned->noproxy = urlp->noproxy;
		cloned->mech = list_clone(urlp->mech, (listclonefunc)xstrdup);
		cloned->pasvmode = urlp->pasvmode;
		cloned->sftp_server = xstrdup(urlp->sftp_server);
		cloned->noupdate = urlp->noupdate;
	}

	return cloned;
}

void url_destroy(url_t *urlp)
{
	if(urlp) {
		free(urlp->protocol);
		free(urlp->username);
		if(urlp->password)
			memset(urlp->password, 0, strlen(urlp->password));
		free(urlp->password);
		free(urlp->hostname);
		free(urlp->alias);
		free(urlp->directory);
		free(urlp->protlevel);
		list_free(urlp->mech);
		free(urlp->sftp_server);
		free(urlp);
	}
}

void url_parse(url_t *urlp, const char *str)
{
	char *e, *p, *adr, *_adr;

	adr = _adr = xstrdup(str);

	urlp->port = -1;
	urlp->mech = 0;

	e = strstr(adr, "://");
	if(e) {
		*e = '\0';
		unquote(adr);
		url_setprotocol(urlp, adr);
		adr = e+3;
	} else if(strncmp(adr, "//", 2) == 0)
		adr += 2;
	e = strqchr(adr, '@');
	if(e) {
		*e = '\0';
		p = strqchr(adr, ':');
		if(p) {
			*p = '\0';
			unquote(p+1);
			if(p[1])
				url_setpassword(urlp, p+1);
		}
		unquote(adr);
		url_setusername(urlp, adr);
		adr = e+1;
	}
	p = strqchr(adr, '/');
	if(p) {
		unquote(p);
		url_setdirectory(urlp, p+1);
		*p = '\0';
	}
	p = strqchr(adr, ':');
	if(p) {
		*p = '\0';
		url_setport(urlp, (int)strtol(p+1, NULL, 0));
	}
	unquote(adr);
	url_sethostname(urlp, adr);

	free(_adr);
}

void url_setprotocol(url_t *urlp, const char *protocol)
{
	int i;

	free(urlp->protocol);
	urlp->protocol = xstrdup(protocol);
	for(i=0;urlp->protocol && urlp->protocol[i];i++)
		/* RFC 1738 says this should be lowercase */
		urlp->protocol[i] = tolower((int)urlp->protocol[i]);
}

void url_sethostname(url_t *urlp, const char *hostname)
{
	free(urlp->hostname);
	urlp->hostname = decode_rfc1738(hostname);
}

void url_setalias(url_t *urlp, const char *alias)
{
	free(urlp->alias);
	urlp->alias = decode_rfc1738(alias);
}

void url_setusername(url_t *urlp, const char *username)
{
	free(urlp->username);
	urlp->username = decode_rfc1738(username);
}

void url_setpassword(url_t *urlp, const char *password)
{
	free(urlp->password);
	if(password && strncmp(password, "[base64]", 8) == 0) {
		urlp->password = (char *)xmalloc(strlen(password+8)*2+1);
		if(base64_decode(password+8, urlp->password) < 0) {
			fprintf(stderr, "failed to decode password\n");
			free(urlp->password);
			urlp->password = 0;
		}
	}
	else
		urlp->password = decode_rfc1738(password);
}

void url_setdirectory(url_t *urlp, const char *directory)
{
	free(urlp->directory);
	urlp->directory = decode_rfc1738(directory);
}

void url_setprotlevel(url_t *urlp, const char *protlevel)
{
	free(urlp->protlevel);
	urlp->protlevel = decode_rfc1738(protlevel);
}

void url_setport(url_t *urlp, int port)
{
	if(port <= 0)
		port = -1;
	urlp->port = port;
}

void url_setpassive(url_t *urlp, int passive)
{
	urlp->pasvmode = passive;
}

void url_setsftp(url_t *urlp, const char *sftp_server)
{
	free(urlp->sftp_server);
	urlp->sftp_server = decode_rfc1738(sftp_server);
}

void url_setmech(url_t *urlp, const char *mech_string)
{
	list_free(urlp->mech);
	urlp->mech = list_new((listfunc)free);
	listify_string(mech_string, urlp->mech);
}

bool url_isanon(const url_t *url)
{
	if(url && url->username)
		return (strcmp(url->username, "ftp") == 0
				|| strcmp(url->username, "anonymous") == 0);
	return false;
}

int urlcmp(const url_t *a, const url_t *b)
{
	if(a->alias && !b->alias)
		return strcmp(a->alias, b->hostname);
	if(!a->alias && b->alias)
		return strcmp(a->hostname, b->alias);
	if(a->alias && b->alias)
		return strcmp(a->alias, b->alias);
	return strcmp(a->hostname, b->hostname);
}

int urlcmp_name(const url_t *a, const char *name)
{
	if(a->alias)
		return (fnmatch(name, a->alias, 0) == FNM_NOMATCH) ? 1 : 0;
/*		return strcmp(a->alias, name);*/
	return (fnmatch(name, a->hostname, 0) == FNM_NOMATCH) ? 1 : 0;
/*	return strcmp(a->hostname, name);*/
}
