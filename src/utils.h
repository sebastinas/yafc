/* $Id: utils.h,v 1.4 2004/05/20 11:10:52 mhe Exp $
 *
 * utils.h -- small (generic) functions
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _utils_h_
#define _utils_h_

void listify_string(const char *str, list *lp);
char *stringify_list(list *lp);
char *make_unique_filename(const char *path);
char *human_size(long long int size);
char *human_time(unsigned int secs);
void print_xterm_title(void);
void reset_xterm_title(void);
char* get_mode_string(mode_t m);
listitem *ftplist_search(const char *str);
char *get_local_curdir(void);
void invoke_shell(char *cmdline);

#endif
