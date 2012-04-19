/*
 * rc.h -- config file parser + autologin lookup
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _rc_h_
#define _rc_h_

int parse_rc(const char *file, bool warn);
url_t *get_autologin_url(const char *host);

#endif
