/* url.c -- splits an URL into its components
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

#include "url.h"
#include "xmalloc.h"
#include "strq.h"
#include "base64.h"

url_t *url_create(void)
{
	url_t *urlp;

	urlp = (url_t *)xmalloc(sizeof(url_t));
	urlp->port = -1;

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
		cloned->mech = urlp->mech;
		cloned->noproxy = urlp->noproxy;
	}

	return cloned;
}

void url_destroy(url_t *urlp)
{
	if(urlp) {
		xfree(urlp->protocol);
		xfree(urlp->username);
		if(urlp->password)
			memset(urlp->password, 0, strlen(urlp->password));
		xfree(urlp->password);
		xfree(urlp->hostname);
		xfree(urlp->alias);
		xfree(urlp->directory);
		xfree(urlp->protlevel);
		xfree(urlp->mech);
		xfree(urlp);
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

	xfree(_adr);
}

void url_setprotocol(url_t *urlp, const char *protocol)
{
	int i;

	xfree(urlp->protocol);
	urlp->protocol = xstrdup(protocol);
	for(i=0;urlp->protocol && urlp->protocol[i];i++)
		/* RFC 1738 says this should be lowercase */
		urlp->protocol[i] = tolower((int)urlp->protocol[i]);
}

void url_sethostname(url_t *urlp, const char *hostname)
{
	xfree(urlp->hostname);
	urlp->hostname = decode_rfc1738(hostname);
}

void url_setalias(url_t *urlp, const char *alias)
{
	xfree(urlp->alias);
	urlp->alias = decode_rfc1738(alias);
}

void url_setusername(url_t *urlp, const char *username)
{
	xfree(urlp->username);
	urlp->username = decode_rfc1738(username);
}

void url_setpassword(url_t *urlp, const char *password)
{
	xfree(urlp->password);
	if(password && strncmp(password, "[base64]", 8) == 0) {
		urlp->password = (char *)xmalloc(strlen(password+8)*2+1);
		if(base64_decode(password+8, urlp->password) < 0) {
			fprintf(stderr, "failed to decode password\n");
			xfree(urlp->password);
			urlp->password = 0;
		}
	}
	else
		urlp->password = decode_rfc1738(password);
}

void url_setdirectory(url_t *urlp, const char *directory)
{
	xfree(urlp->directory);
	urlp->directory = decode_rfc1738(directory);
}

void url_setprotlevel(url_t *urlp, const char *protlevel)
{
	xfree(urlp->protlevel);
	urlp->protlevel = decode_rfc1738(protlevel);
}

void url_setmech(url_t *urlp, const char *mech)
{
	xfree(urlp->mech);
	urlp->mech = xstrdup(mech);
}

void url_setport(url_t *urlp, int port)
{
	if(port <= 0)
		port = -1;
	urlp->port = port;
}

void url_setpassive(url_t *urlp, bool passive)
{
	urlp->passive = passive;
}


bool url_isanon(const url_t *url)
{
	return (strcmp(url->username, "ftp") == 0
			|| strcmp(url->username, "anonymous") == 0);
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
