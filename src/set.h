/* $Id: set.h,v 1.3 2001/05/12 18:44:37 mhe Exp $
 *
 * set.h -- sets and shows variables from within Yafc
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _set_h_included
#define _set_h_included

#define ARG_BOOL 1
#define ARG_INT 2
#define ARG_CHAR 3
#define ARG_STR 4

struct _setvar {
	char *name;
	int argtype;
	void (*setfunc)(void *val);
};

extern struct _setvar setvariables[];

#endif
