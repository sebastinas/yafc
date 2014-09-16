/*
 * strq.h -- string functions, handles quoted text
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 * Copyright (C) 2013, Sebastian Ramacher <sebastian+dev@ramacher.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
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
char *base_name_xptr(const char* s);
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(HAVE_LIBREADLINE) && !defined(HAVE_LIBEDIT)
/* Quote a filename using double quotes, single quotes, or backslashes
   depending on the value of completion_quoting_style.  If we're
   completing using backslashes, we need to quote some additional
   characters (those that readline treats as word breaks), so we call
   quote_word_break_chars on the result. */
char* quote_filename(const char* s, int rtype, const char* qcp);
#endif

/* A function to strip quotes that are not protected by backquotes.  It
   allows single quotes to appear within double quotes, and vice versa.
   It should be smarter. */
char* dequote_filename(const char* text, int quote_char);
/* Quote special characters in STRING using backslashes.  Return a new
   string. */
char* backslash_quote(const char* string);
/* Return 1 if the portion of STRING ending at EINDEX is quoted (there is
   an unclosed quoted string), or if the character at EINDEX is quoted
   by a backslash. */
int char_is_quoted(const char* string, int eindex);

#endif
