/* utils.h -- small (generic) functions
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

#ifndef _utils_h_
#define _utils_h_

void listify_string(const char *str, list *lp);
char *stringify_list(list *lp);
char *make_unique_filename(const char *path);
char *human_size(long size);
char *human_time(unsigned int secs);
void print_xterm_title(void);
void reset_xterm_title(void);
char* get_mode_string(mode_t m);
listitem *ftplist_search(const char *str);
char *get_local_curdir(void);
void invoke_shell(char *cmdline);

#endif
