/*
 * vasprintf.c -- v(a)sprintf implenmentation based on C99 functions
 *
 * Copyright (C) 2013, Sebastian Ramacher <sebastian+dev@ramacher.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>

int vasprintf(char** result, const char* format, va_list args)
{
  if (!result)
    return -1;

  va_list copy;
  va_copy(copy, args);
  const int s = vsnprintf(NULL, 0, format, copy);
  va_end(copy);
  if (s < 0 || s >= SIZE_MAX)
    return -1;

  char* r = malloc(s + 1);
  if (!r)
    return -1;

  *result = r;
  return vsnprintf(r, s + 1, format, args);
}

int asprintf(char** result, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  const int res = vasprintf(result, format, args);
  va_end(args);

  return res;
}
