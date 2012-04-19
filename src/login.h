/*
 * login.h -- connect and login
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _login_h_included
#define _login_h_included

#define OP_NOKRB 1
#define OP_YESKRB 2
#define OP_ANON 4
#define OP_NOAUTO 8
#define OP_NOALIAS 16
#define OP_NOPROXY 32

void yafc_open(const char *host, unsigned int opt,
			   const char *mech, const char *sftp_server);

#endif
