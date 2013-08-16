/* bashline.c -- Bash's interface to the readline library. */

/* modified for use in Yafc by Martin Hedenfalk <mhe@home.se>
 * this is not the complete bashline.c, see the bash source,
 * available at ftp://ftp.gnu.org/pub/
 * 
 * latest modified 1999-07-22, 2000-11-04
 */

/* Copyright (C) 1987,1991 Free Software Foundation, Inc.

   This file is part of GNU Bash, the Bourne Again SHell.

   Bash is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   Bash is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with Bash; see the file COPYING.  If not, write to the Free
   Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

#include "bashline.h"
#include "xmalloc.h"
#include <string.h>
#ifdef HAVE_BSD_STRING
#include <bsd/string.h>
#endif
#include <stdbool.h>
#include <stdlib.h>
#include "strq.h"  /* for tilde_expand_home */
#include "gvars.h"

typedef enum
{
  COMPLETE_DQUOTE,
  COMPLETE_SQUOTE,
  COMPLETE_BSQUOTE
} completion_mode;

#ifdef HAVE_LIBREADLINE
static int completion_quoting_style = COMPLETE_BSQUOTE;
#endif

#if defined(HAVE_LIBREADLINE) && !defined(HAVE_LIBEDIT)
/* Quote characters that the readline completion code would treat as
   word break characters with backslashes. */
static char* quote_word_break_chars(const char* text);
/* Double quoted version of string. */
static char* double_quote(const char* string);
/* Single quoted version of string. */
static char* single_quote(const char* string);
#endif


char*
dequote_filename(const char* text, int quote_char)
{
  const size_t len = strlen(text);
  char* result = xmalloc(len + 1);
  memset(result, 0, len + 1);

  int next = 0;
  for (char* tmp = result; text && *text; ++text)
  {
    if (*text == '\\')
	  {
	    *tmp++ = *++text;
	    if (*text == '\0')
	      break;
	  }
    else if (next && *text == next)
      next = 0;
    else if (!next && (*text == '\'' || *text == '"'))
      next = *text;
    else
      *tmp++ = *text;
  }

  return result;
}

#if defined(HAVE_LIBREADLINE) && !defined(HAVE_LIBEDIT)
/* these should really be defined in readline/readline.h */
#ifndef SINGLE_MATCH
# define SINGLE_MATCH 1
#endif
#ifndef MULT_MATCH
# define MULT_MATCH 2
#endif

char*
quote_filename(const char* string, int rtype, const char* qcp)
{
  char* tmp = NULL;
  if (*string == '~' && rtype == SINGLE_MATCH)
    tmp = tilde_expand_home(string, gvLocalHomeDir);
  else
    tmp = strdup(string);

  completion_mode cs = completion_quoting_style;
  if (*qcp == '"')
    cs = COMPLETE_DQUOTE;
  else if (*qcp == '\'')
    cs = COMPLETE_SQUOTE;

  char* result = NULL;
  switch (cs)
  {
    case COMPLETE_DQUOTE:
      result = double_quote(tmp);
      break;
    case COMPLETE_SQUOTE:
      result = single_quote(tmp);
      break;
    case COMPLETE_BSQUOTE:
      result = backslash_quote(tmp);
      free(tmp);
      tmp = quote_word_break_chars(result);
      free(result);
      result = tmp;
      break;
  }
  free(tmp);

  if (rtype == MULT_MATCH && cs != COMPLETE_BSQUOTE)
    result[strlen(result) - 1] = '\0';
  return result;
}
#endif

static size_t
skip_until(const char* string, size_t idx, char c)
{
  for (; string[idx] && string[idx] != c; ++idx)
    ;
  if (string[idx])
    ++idx;
  return idx;
}

int
char_is_quoted(const char* string, int eindex)
{
  bool pass_next = false;
  for (size_t idx = 0; idx <= eindex; ++idx)
  {
    if (pass_next)
	  {
	    pass_next = 0;
	    if (idx >= eindex)
	      return 1;
	  }
    else if (string[idx] == '\'' || string[idx] == '"')
	  {
      idx = skip_until(string, idx + 1, string[idx]);
	    if (idx > eindex)
	      return 1;
	    idx--;
	  }
    else if (string[idx] == '\\')
	    pass_next = true;
  }

  return 0;
}

#if defined(HAVE_LIBREADLINE) && !defined(HAVE_LIBEDIT)
/* Quote STRING using double quotes.  Return a new string. */
static char*
double_quote(const char* string)
{
  if (!string || !*string)
    return strdup("\"\"");

  size_t cnt = 0;
  {
    const char* tmp = string;
    while ((tmp = strpbrk(tmp, "\"$`\\")))
    {
      ++tmp;
      ++cnt;
    }
  }

  const size_t size = 3 + strlen(string) + 2 * cnt;
  char* result = xmalloc(size);
  memset(result, 0, size);
  result[0] = '"';

  char* tmp = result + 1;
  for (; *string; ++string)
  {
    switch (*string)
    {
	    case '"':
	    case '$':
	    case '`':
	    case '\\':
	      *tmp++ = '\\';
	    default:
	      *tmp++ = *string;
    }
  }

  *tmp++ = '"';
  return result;
}

static char*
single_quote(const char* string)
{
  if (!string || !*string)
    return strdup("''");

  size_t cnt = 0;
  {
    const char* tmp = string;
    while ((tmp = strchr(tmp, '\'')))
    {
      ++tmp;
      ++cnt;
    }
  }

  const size_t size = 3 + strlen(string) + 3 * cnt;
  char* result = xmalloc(size);
  memset(result, 0, size);
  result[0] = '\'';

  char* tmp = result + 1;
  for (; *string; ++string)
  {
    if (*string == '\'')
    {
      *tmp++ = '\'';
      *tmp++ = '\\';
      *tmp++ = '\'';
      *tmp++ = '\'';
    }
    else
      *tmp++ = *string;
  }

  *tmp++ = '\'';
  return result;
}
#endif

static char*
backslash_quote_impl(const char* string, const char* set)
{
  if (!set)
    return NULL;
  if (!string || !*string)
    return strdup("");

  size_t cnt = 0;
  {
    const char* tmp = string;
    while ((tmp = strpbrk(tmp, set)))
    {
      ++tmp;
      ++cnt;
    }
  }

  const size_t size = 1 + strlen(string) + 2 * cnt;
  char* result = xmalloc(size);
  memset(result, 0, size);

  for (char* tmp = result; *string; ++string)
  {
    if (strchr(set, *string))
        *tmp++ = '\\';
    *tmp++ = *string;
  }

  return result;
}

char*
backslash_quote(const char* string)
{
  return backslash_quote_impl(string, " \t\n'\"\\|&;()<>!{}*[]?^$`#");
}

#if defined(HAVE_LIBREADLINE) && !defined(HAVE_LIBEDIT)
static char*
quote_word_break_chars(const char* string)
{
  const size_t len = strlen(rl_completer_word_break_characters) + 2;
  char* set = xmalloc(len);
  memset(set, 0, len);
  strlcat(set, "\\", len);
  strlcat(set, rl_completer_word_break_characters, len);

  char* ret = backslash_quote_impl(string, set);
  free(set);
  return ret;
}
#endif
