/*
 * tag.h -- tag files
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _tag_h_
#define _tag_h_

void save_taglist(const char *alt_filename);
void load_taglist(bool showerr, bool always_autoload,
				  const char *alt_filename);


#endif
