/* bashline.h --
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
