/* strq.h -- string functions, handles quoted text
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

#ifndef _strq_h_included
#define _strq_h_included

void strpush(char *s, int n);
void strpull(char *s, int n);
int strqnchr(const char *str, char c);
char *strqchr(char *s, int ch);
char *strqsep(char **s, char delim);
void unquote(char *str);
void unquote_escapes(char *str);

char *stripslash(char *p);
const char *base_name_ptr(const char *s);
char *base_dir_xptr(const char *path);
char *strip_blanks(char *str);
void fix_unprintable(char *str);

char *tilde_expand_home(const char *str, const char *home);
char *path_absolute(const char *path,
					const char *curdir, const char *homedir);
char *path_collapse(char *path);
char *path_dos2unix(char *path);

int str2bool(const char *e);

char *encode_rfc1738(const char *str, const char *xtra);
char *decode_rfc1738(const char *str);
char *encode_url_directory(const char *str);
#define encode_url_username(str)   encode_rfc1738(str, ":@/")
#define encode_url_password(str)   encode_rfc1738(str, ":@/")

void strip_trailing_chars(char *str, const char *chrs);

#endif
