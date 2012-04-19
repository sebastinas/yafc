/*
 * redir.h --
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _REDIR_H_INCLUDED
#define _REDIR_H_INCLUDED

bool reject_ampersand(char *cmd);
int open_redirection(char *cmd);
int close_redirection(void);

#endif
