/* $Id: alias.h,v 1.3 2001/05/12 18:44:37 mhe Exp $
 *
 * alias.h -- define and undefine aliases
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _alias_h_included
#define _alias_h_included

#include "syshdr.h"
#include "linklist.h"
#include "args.h"

#define ALIAS_AMBIGUOUS (alias *)-1

typedef struct {
	char *name;
	args_t *value;
} alias;

alias *alias_create(void);
void alias_destroy(alias *ap);
alias *alias_search(const char *name);
void alias_clear(alias *ap);
void alias_define(const char *name, args_t *args);

#endif
