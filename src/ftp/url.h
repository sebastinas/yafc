/* $Id: url.h,v 1.8 2001/05/21 19:54:00 mhe Exp $
 *
 * url.h -- splits an URL into its components
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _url_h_included
#define _url_h_included

#include "linklist.h"

typedef struct url_t {
	char *protocol;   /* "ssh", "ftp", ... */
	char *hostname;   /* hostname to connect to */
	char *alias;      /* other name for this url */
	char *username;   /* username to login with */
	char *password;   /* password to login with */
	char *directory;  /* startup directory */
	char *protlevel;  /* security protection level */
	int port;         /* port in host byte order */
	list *mech;       /* requested security mechanisms to try */
	bool noproxy;     /* don't connect via the configured proxy */
	int pasvmode;    /* true if passive mode is requested */
	char *sftp_server; /* path to remote sftp_server program */
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
void url_setpassive(url_t *urlp, int passive);
void url_setsftp(url_t *urlp, const char *sftp_server);

bool url_isanon(const url_t *url);

/* returns 0 if a == b */
int urlcmp(const url_t *a, const url_t *b);
int urlcmp_name(const url_t *a, const char *name);

#endif
