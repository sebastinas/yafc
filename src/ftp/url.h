/* url.h -- splits an URL into its components
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

#ifndef _url_h_included
#define _url_h_included

#include "linklist.h"

typedef struct url_t {
	char *protocol;   /* "http", "ftp", ... (only ftp supported) */
	char *hostname;   /* hostname to connect to */
	char *alias;      /* other name for this url */
	char *username;   /* username to login with */
	char *password;   /* password to login with */
	char *directory;  /* startup directory */
	char *protlevel;  /* security protection level */
	int port;         /* port in host byte order */
	list *mech;       /* requested security mechanisms to try */
	bool noproxy;     /* don't connect via the configured proxy */
	bool passive;     /* true if passive mode is requested */
} url_t;

url_t *url_create(void);
url_t *url_init(const char *str);
void url_destroy(url_t *urlp);
url_t *url_clone(const url_t *urlp);

void url_parse(url_t *urlp, const char *str);
void url_setprotocol(url_t *urlp, const char *protocol);
void url_sethostname(url_t *urlp, const char *hostname);
void url_setalias(url_t *urlp, const char *alias);
void url_setusername(url_t *urlp, const char *username);
void url_setpassword(url_t *urlp, const char *password);
void url_setdirectory(url_t *urlp, const char *directory);
void url_setprotlevel(url_t *urlp, const char *protlevel);
void url_setport(url_t *urlp, int port);
void url_setmech(url_t *urlp, const char *mech_string);
void url_setpassive(url_t *urlp, bool passive);

bool url_isanon(const url_t *url);

/* returns 0 if a == b */
int urlcmp(const url_t *a, const url_t *b);
int urlcmp_name(const url_t *a, const char *name);

#endif
