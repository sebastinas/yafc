/*
 * bashline.h --
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef _bashline_h_included
#define _bashline_h_included

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
