/* $Id: rglob.h,v 1.3 2001/05/12 18:44:04 mhe Exp $
 *
 * rglob.h -- remote glob functions
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _rglob_h_included
#define _rglob_h_included

#include "syshdr.h"
#include "rfile.h"
#include "linklist.h"

#define NODOTDIRS rglob_exclude_dotdirs

typedef bool (*rglobfunc)(rfile *f);

list *rglob_create(void);
void rglob_destroy(list *gl);

bool rglob_exclude_dotdirs(rfile *f);
int rglob_glob(list *gl, const char *mask, bool cpifnomatch,
			   bool ignore_multiples, rglobfunc exclude_func);
unsigned long rglob_size(list *gl);
unsigned long rglob_numdirs(list *gl);

#endif
