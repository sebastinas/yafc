/* $Id: prompt.h,v 1.3 2001/05/12 18:44:37 mhe Exp $
 *
 * prompt.h -- expands the prompt
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _prompt_h_
#define _prompt_h_

char *expand_prompt(const char *fmt);

#endif
