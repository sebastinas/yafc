/* lglob.h -- local glob functions
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

#ifndef _lglob_h_included
#define _lglob_h_included

#include "linklist.h"

typedef bool (*lglobfunc)(char *f);

list *lglob_create(void);
void lglob_destroy(list *gl);
bool lglob_exclude_dotdirs(char *f);
int lglob_glob(list *gl, const char *mask, lglobfunc exclude_func);

#endif
