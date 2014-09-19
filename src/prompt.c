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

static bool read_uint(const char** str, unsigned int* number) {
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

  char* prompt = xmalloc(strlen(fmt) + 1);
  char* cp = prompt;

  while (fmt && *fmt)
  {
    if (*fmt == '%')
    {
      fmt++;

      char* ins = NULL;
      bool freeins = false;
      unsigned int maxlen = UINT_MAX;

      read_uint(&fmt, &maxlen);

      switch (*fmt)
      {
        case 'c':
        {
          if (asprintf(&ins, "%u", list_numitem(gvFtpList)) == -1)
          {
            fprintf(stderr, _("Failed to allocate memory.\n"));
            free(prompt);
            return NULL;
          }
          freeins = true;
          break;
        }

        case 'C':
        {
          if (asprintf(&ins, "%u", connection_number()) == -1)
          {
            fprintf(stderr, _("Failed to allocate memory.\n"));
            free(prompt);
            return NULL;
          }
          freeins = true;
          break;
        }

        case 'u': /* username */
        {
          char* user = ftp_connected_user();
          if (user)
          {
            ins = user;
            freeins = true;
          }
          else
            ins = "?";
          break;
        }

        case 'h': /* remote hostname (as passed to cmd_open()) */
        {
          ins = ftp_connected() ? ftp->url->hostname : "?";
          break;
        }

        case 'H': /* %h up to first '.' */
        {
          if (!ftp_connected()) {
            ins = "?";
            break;
          }

          char* e = strchr(ftp->url->hostname, '.');
          if (e)
          {
            ins = xstrndup(ftp->url->hostname, e - ftp->url->hostname);
            freeins = true;
          }
          else
            ins = ftp->url->hostname;
          break;
        }

        case 'm': /* remote machine name (as returned from gethostbynmame) */
        {
          ins = xstrdup(ftp_connected() ? host_getoname(ftp->host) : "?");
          freeins = true;
          break;
        }

        case 'M': /* %m up to first '.' */
        {
          if (!ftp_connected()) {
            ins = "?";
            break;
          }

          char* e = strchr(host_getoname(ftp->host), '.');
          if (e)
            ins = xstrndup(host_getoname(ftp->host),
                     e - host_getoname(ftp->host));
          else
            ins = xstrdup(host_getoname(ftp->host));
          freeins = true;
          break;
        }

        case 'n': /* remote ip number */
        {
          ins = xstrdup(ftp_connected() ? host_getoname(ftp->host) : "?");
          freeins = true;
          break;
        }

        case 'w': /* current remote directory */
        {
          if (!ftp_loggedin())
            ins = "?";
          else
          {
            ins = shortpath(ftp->curdir, maxlen, 0);
            freeins = true;
          }
          break;
        }

        case 'W': /* basename(%w) */
        {
          if (!ftp_loggedin())
            ins = "?";
          else
            ins = (char *)base_name_ptr(ftp->curdir);
          break;
        }

        case '~': /* %w but homedir replaced with '~' */
        {
          if (!ftp_loggedin())
            ins = "?";
          else
          {
            ins = shortpath(ftp->curdir, maxlen, ftp->homedir);
            freeins = true;
          }
          break;
        }

        case 'l': /* current local directory */
        {
          char* tmp = getcwd(NULL, 0);
          if (tmp)
          {
            ins = shortpath(tmp, maxlen, 0);
            freeins = true;
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
            freeins = true;
            free(tmp);
          }
          break;
        }

        case '%': /* percent sign */
        {
          ins = "%";
          break;
        }

        case '#': /* local user == root ? '#' : '$' */
        {
          ins = getuid() == 0 ? "#" : "$";
          break;
        }

#ifdef HAVE_LIBREADLINE
        case '{': /* begin non-printable character string */
        {
          ins = "\001"; /* RL_PROMPT_START_IGNORE */
          break;
        }

        case '}': /* end non-printable character string */
        {
          ins = "\002"; /* RL_PROMPT_END_IGNORE */
          break;
        }
#endif

        case 'e': /* escape (0x1B) */
        {
          ins = "\x1B";
          break;
        }
      }

      if (ins)
      {
        const size_t len = strlen(prompt) + strlen(ins) + strlen(fmt+1) + 1;
        char* tmp = xmalloc(len);
        strlcpy(tmp, prompt, len);
        strlcat(tmp, ins, len);
        cp = tmp + strlen(prompt) + strlen(ins);
        free(prompt);
        prompt = tmp;
        if (freeins)
          free(ins);
      }
    }
    else
      *cp++ = *fmt;

    fmt++;
  }

  unquote_escapes(prompt);
  return prompt;
}
