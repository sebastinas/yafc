/* rglob.h -- remote glob functions
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
