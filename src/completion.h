/* $Id: completion.h,v 1.4 2001/05/12 18:44:37 mhe Exp $
 *
 * completion.h -- readline completion functions
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _COMPLETION_H_INCLUDED
#define _COMPLETION_H_INCLUDED

char **the_complete_function(char *text, int start, int end);
char *no_completion_function(char *text, int state);

#endif
