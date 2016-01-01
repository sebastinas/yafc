/*
 * prompt.c -- expands the prompt
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#include "syshdr.h"
#include "ftp.h"
#include "strq.h"
#include "shortpath.h"
#include "gvars.h"

typedef struct string_buffer_s
{
  char* start;
  size_t current;
  size_t size;
} string_buffer_t;

static string_buffer_t* sb_new(void)
{
  string_buffer_t* sb = malloc(sizeof(string_buffer_t));
  if (!sb)
    return NULL;

  const size_t new_size = 128;
  sb->start = calloc(1, new_size);
  if (!sb->start)
  {
    free(sb);
    return NULL;
  }

  sb->current = 0;
  sb->size = new_size;

  return sb;
}

static bool sb_grow(string_buffer_t* sb, const size_t size)
{
  const size_t new_size = sb->size + ((size / 128) + 1) * 128;
  char* tmp = realloc(sb->start, new_size);
  if (!tmp)
    return false;

  memset(tmp + sb->current, '\0', new_size - sb->current);

  sb->start = tmp;
  sb->size = new_size;

  return true;
}

static bool sb_append_c(string_buffer_t* sb, char c)
{
  if (sb->current + 1 == sb->size)
    if (!sb_grow(sb, 1))
      return false;

  sb->start[sb->current] = c;
  ++sb->current;

  return true;
}

static bool sb_append_s(string_buffer_t* sb, const char* s)
{
  const size_t len = strlen(s);
  if (sb->current + len + 1 >= sb->size)
    if (!sb_grow(sb, len))
      return false;

  strcat(sb->start + sb->current, s);
  sb->current += len;

  return true;
}

static void sb_free(string_buffer_t* sb)
{
  free(sb->start);
  free(sb);
}

static int connection_number(void)
{
  int i = 1;

  for (listitem* li = gvFtpList->first; li; li=li->next, i++) {
    if (gvCurrentFtp == li)
      return i;
  }
  /* should not reach here! */
  return -1;
}

/*
 * %c number of connections open
 * %C number of the current connection
 * %u username
 * %h remote host name (as passed to open)
 * %H %h up to the first '.'
 * %m remote machine name (as returned by gethostbyname)
 * %M %m up to the first '.'
 * %n remote ip number
 * %[#]w current remote working directory
 * %W basename of %w
 * %[#]~ as %w but home directory is replaced with ~
 * %[#]l current local working directory
 * %% percent sign
 * %# a '#' if (local) user is root, else '$'
 * %{ begin sequence of non-printing chars, ie escape codes
 * %} end      -"-
 * \e escape (0x1B)
 * \x## character 0x## (hex)
 *
 * [#] means an optional (maximum) width specifier can be specified
 *  example: %32w
 */

static bool read_uint(const char** str, unsigned int* number)
{
  if (!str || !*str || !number) {
    return false;
  }

  char* endptr = NULL;
  errno = 0;

  const unsigned long value = strtol(*str, &endptr, 0);
  if ((errno == ERANGE && (value == ULONG_MAX || value == 0)) ||
    (errno != 0 && value == 0))
  {
    *str = endptr;
    return false;
  }

  if (*str != endptr)
    *number = MIN(value, UINT_MAX);

  *str = endptr;
  return true;
}

char* expand_prompt(const char* fmt)
{
  if (!fmt)
    return NULL;

  string_buffer_t* sb = sb_new();
  if (!sb)
  {
    fprintf(stderr, _("Failed to allocate memory.\n"));
    return NULL;
  }

  while (fmt && *fmt)
  {
    if (*fmt == '%')
    {
      fmt++;

      char* ins = NULL;
      const char* cins = NULL;
      unsigned int maxlen = UINT_MAX;

      read_uint(&fmt, &maxlen);

      switch (*fmt)
      {
        case 'c':
        {
          if (asprintf(&ins, "%zu", list_numitem(gvFtpList)) == -1)
          {
            fprintf(stderr, _("Failed to allocate memory.\n"));
            sb_free(sb);
            return NULL;
          }
          break;
        }

        case 'C':
        {
          if (asprintf(&ins, "%u", connection_number()) == -1)
          {
            fprintf(stderr, _("Failed to allocate memory.\n"));
            sb_free(sb);
            return NULL;
          }
          break;
        }

        case 'u': /* username */
        {
          char* user = ftp_connected_user();
          if (user)
            ins = user;
          else
            cins = "?";
          break;
        }

        case 'h': /* remote hostname (as passed to cmd_open()) */
        {
          cins = ftp_connected() ? ftp->url->hostname : "?";
          break;
        }

        case 'H': /* %h up to first '.' */
        {
          if (!ftp_connected()) {
            cins = "?";
            break;
          }

          char* e = strchr(ftp->url->hostname, '.');
          if (e)
            ins = xstrndup(ftp->url->hostname, e - ftp->url->hostname);
          else
            cins = ftp->url->hostname;
          break;
        }

        case 'm': /* remote machine name (as returned from gethostbynmame) */
        {
          ins = xstrdup(ftp_connected() ? host_getoname(ftp->host) : "?");
          break;
        }

        case 'M': /* %m up to first '.' */
        {
          if (!ftp_connected()) {
            cins = "?";
            break;
          }

          char* e = strchr(host_getoname(ftp->host), '.');
          if (e)
            ins = xstrndup(host_getoname(ftp->host),
                     e - host_getoname(ftp->host));
          else
            ins = xstrdup(host_getoname(ftp->host));
          break;
        }

        case 'n': /* remote ip number */
        {
          ins = xstrdup(ftp_connected() ? host_getoname(ftp->host) : "?");
          break;
        }

        case 'w': /* current remote directory */
        {
          if (!ftp_loggedin())
            cins = "?";
          else
            ins = shortpath(ftp->curdir, maxlen, 0);
          break;
        }

        case 'W': /* basename(%w) */
        {
          if (!ftp_loggedin())
            cins = "?";
          else
            ins = (char *)base_name_ptr(ftp->curdir);
          break;
        }

        case '~': /* %w but homedir replaced with '~' */
        {
          if (!ftp_loggedin())
            cins = "?";
          else
            ins = shortpath(ftp->curdir, maxlen, ftp->homedir);
          break;
        }

        case 'l': /* current local directory */
        {
          char* tmp = getcwd(NULL, 0);
          if (tmp)
          {
            ins = shortpath(tmp, maxlen, 0);
            free(tmp);
          }
          break;
        }

        case 'L': /* basename(%l) */
        {
          char* tmp = getcwd(NULL, 0);
          if (tmp)
          {
            ins = base_name_xptr(tmp);
            free(tmp);
          }
          break;
        }

        case '%': /* percent sign */
        {
          cins = "%";
          break;
        }

        case '#': /* local user == root ? '#' : '$' */
        {
          cins = getuid() == 0 ? "#" : "$";
          break;
        }

#ifdef HAVE_LIBREADLINE
        case '{': /* begin non-printable character string */
        {
          cins = "\001"; /* RL_PROMPT_START_IGNORE */
          break;
        }

        case '}': /* end non-printable character string */
        {
          cins = "\002"; /* RL_PROMPT_END_IGNORE */
          break;
        }
#endif

        case 'e': /* escape (0x1B) */
        {
          cins = "\x1B";
          break;
        }
      }

      if (ins || cins)
      {
        const bool ret = ins ? sb_append_s(sb, ins) : sb_append_s(sb, cins);
        free(ins);

        if (!ret)
        {
          fprintf(stderr, _("Failed to allocate memory.\n"));
          sb_free(sb);
          return NULL;
        }
      }
    }
    else
    {
      if (!sb_append_c(sb, *fmt))
      {
        fprintf(stderr, _("Failed to allocate memory.\n"));
        sb_free(sb);
        return NULL;
      }
    }

    fmt++;
  }

  char* prompt = xstrdup(sb->start);
  sb_free(sb);

  unquote_escapes(prompt);
  return prompt;
}
