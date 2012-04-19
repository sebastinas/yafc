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

char *bash_quote_filename (char *s, int rtype, char *qcp);
char *bash_dequote_filename (const char *text, int quote_char);
char *bash_backslash_quote (char *string);
char *bash_single_quote (char *string);
char *bash_double_quote (char *string);
int char_is_quoted (char *string, int eindex);
char *quote_word_break_chars (char *text);

#endif
