/* $Id: lglob.h,v 1.5 2002/11/06 11:58:34 mhe Exp $
 *
 * lglob.h -- local glob functions
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _lglob_h_included
#define _lglob_h_included

#include "linklist.h"

typedef bool (*lglobfunc)(char *f);

list *lglob_create(void);
void lglob_destroy(list *gl);
bool lglob_exclude_dotdirs(char *f);
int lglob_glob(list *gl, const char *mask, bool ignore_multiples,
			   lglobfunc exclude_func);

#endif
