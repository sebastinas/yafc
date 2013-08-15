/*
 * modechange.c -- re-implementation of mode_compile
 *
 * Yet Another FTP Client
 * Copyright (C) 2013, Sebastian Ramacher <sebastian+dev@ramacher.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

/* Stricter re-implementation of mode_compile in C99 with less magic values.
 * mode_compile was originally written by David MacKenzie. */

#include "modechange.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

/* Only modifiy executable bit if any is set or it's a directory. */
#define MF_X_IF_ANY 1
/* Copy existing bits from u, g or o to the other groups. */
#define MF_COPY_EXISTING 2

#define MAX_MODE 07777
#define ALLOWED_AFFECTED "ugoa"
#define ALLOWED_OPERATIONS "+-="
#define ALLOWED_BITS "rwxXstugo"
#define SEPARATOR ','

/* Allowed operations. */
typedef enum
{
  MO_ASSIGN,
  MO_PLUS,
  MO_MINUS
} mode_operation;

/* Internal representation of the mode changes. */
struct mode_change_s
{
  mode_change* next;
  /* Affected bits. */
  mode_t affected;
  /* Permissions to add or remove. */
  mode_t value;
  /* Selected operation. */
  mode_operation op:2;
  /* For MF_X_IF_ANY and MF_COPY_EXISTING. */
  unsigned int flags:2;
};

static mode_change*
mode_parse(const char* mode, size_t* offset, const mode_t umask_value,
    unsigned int flags)
{
  if (!mode || !offset)
    return NULL;

  /* Check if the mode is valid: [ugoa]*[+-=][rwxXstugo]+ */
  const size_t affected_modifiers = strspn(mode, ALLOWED_AFFECTED);
  const size_t operations = strspn(mode + affected_modifiers,
      ALLOWED_OPERATIONS);
  if (operations != 1)
    return NULL;
  const size_t bits = strspn(mode + affected_modifiers + operations,
      ALLOWED_BITS);
  if (!bits)
    return NULL;

  mode_change* ret = calloc(1, sizeof(mode_change));
  if (!ret)
    return NULL;

  /* Parse affected bits. */
  for (size_t j = 0; j != affected_modifiers; ++j)
  {
    switch (mode[j])
    {
      case 'u':
        ret->affected |= S_ISUID | S_IRWXU;
        break;
      case 'g':
        ret->affected |= S_ISGID | S_IRWXG;
        break;
      case 'o':
        ret->affected |= S_ISVTX | S_IRWXO;
        break;
      case 'a':
        ret->affected |= MAX_MODE;
        break;
    }
  }
  if (!ret->affected)
    ret->affected = MAX_MODE;
  else
    flags = 0;

  /* Parse operation */
  bool mask = false;
  switch (mode[affected_modifiers])
  {
    case '+':
      ret->op = MO_PLUS;
      mask = flags & MODE_MASK_PLUS;
      break;
    case '-':
      ret->op = MO_MINUS;
      mask = flags & MODE_MASK_MINUS;
      break;
    case '=':
      ret->op = MO_ASSIGN;
      mask = flags & MODE_MASK_EQUALS;
      break;
    default:
      mode_free(ret);
      return NULL;
  }
  if (mask)
    ret->affected &= ~umask_value;

  /* Parse values */
  const size_t end = affected_modifiers + operations + bits;
  for (size_t j = affected_modifiers + operations; j != end; ++j)
  {
    switch (mode[j])
    {
      case 'r':
        ret->value |= (S_IRUSR | S_IRGRP | S_IROTH) & ret->affected;
        break;
      case 'w':
        ret->value |= (S_IWUSR | S_IWGRP | S_IWOTH) & ret->affected;
        break;
      case 'X':
        ret->flags |= MF_X_IF_ANY;
      case 'x':
        ret->value |= (S_IXUSR | S_IXGRP | S_IXOTH) & ret->affected;
        break;
      case 's':
        ret->value |= (S_ISUID | S_ISGID) & ret->affected;
        break;
      case 't':
        ret->value |= S_ISVTX & ret->affected;
        break;
      case 'u':
        if (ret->value)
        {
          mode_free(ret);
          return NULL;
        }
        ret->value = S_IRWXU;
        ret->flags |= MF_COPY_EXISTING;
        break;
      case 'g':
        if (ret->value)
        {
          mode_free(ret);
          return NULL;
        }
        ret->value = S_IRWXG;
        ret->flags |= MF_COPY_EXISTING;
        break;
      case 'o':
        if (ret->value)
        {
          mode_free(ret);
          return NULL;
        }
        ret->value = S_IRWXO;
        ret->flags |= MF_COPY_EXISTING;
        break;
      default:
        mode_free(ret);
        return NULL;
    }
  }

  *offset = end;
  return ret;
}

mode_change*
mode_compile(const char* mode, unsigned int flags)
{
  if (!mode || !*mode)
    return NULL;

  {
    /* Check if mode contains a valid octal mode. */
    char* endptr = NULL;
    const long modei = strtol(mode, &endptr, 8);

    if (mode != endptr) {
      if (modei < 0 || modei > MAX_MODE)
        return NULL;

      mode_change* list = calloc(1, sizeof(mode_change));
      if (!list)
        return NULL;

      list->value = modei;
      list->affected = MAX_MODE;
      return list;
    }
  }

  const mode_t umask_value = umask(0);
  umask(umask_value);

  /* Compile operations into mode_change instances */
  mode_change* list = NULL;
  do
  {
    size_t offset = 0;
    mode_change* next = mode_parse(mode, &offset, umask_value, flags);
    if (!next)
    {
      mode_free(list);
      return NULL;
    }

    if (!list)
      list = next;
    else
      list->next = next;

    mode += offset;
    if (*mode == SEPARATOR)
      mode += 1;
    else if (*mode)
    {
      mode_free(list);
      return NULL;
    }
  } while (*mode);
  return list;
}

mode_t
mode_adjust(mode_t oldmode, const mode_change* mc)
{
  mode_t newmode = oldmode & MAX_MODE;
  for (; mc; mc = mc->next)
  {
    mode_t value = 0;
    if (mc->flags & MF_COPY_EXISTING)
    {
      /* Copy permissions from specified group to the other groups. */
      value = newmode & mc->value;
      if (mc->value & S_IRWXU)
        value |= (value >> 3) | (value >> 6);
      else if (mc->value & S_IRWXG)
        value |= (value << 3) | (value >> 3);
      else
        value |= (value << 3) | (value << 6);
      value &= mc->affected;
    }
    else
    {
      value = mc->value;
      if ((mc->flags & MF_X_IF_ANY) && !S_ISDIR(oldmode)
          && !(newmode & (S_IXUSR | S_IXGRP | S_IXOTH)))
        value &= ~(S_IXUSR | S_IXGRP | S_IXOTH);
    }

    switch (mc->op)
    {
      case MO_ASSIGN:
        newmode = (newmode & ~mc->affected) | value;
        break;
      case MO_PLUS:
        newmode |= value;
        break;
      case MO_MINUS:
        newmode &= ~value;
        break;
    }
  }
  return newmode;
}

void mode_free(mode_change* mc)
{
  while (mc)
  {
    mode_change* old = mc;
    mc = mc->next;
    free(old);
  }
}
