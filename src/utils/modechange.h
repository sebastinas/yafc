/*
 * modechange.h -- re-implementation of mode_compile
 *
 * Yet Another FTP Client
 * Copyright (C) 2013, Sebastian Ramacher <sebastian+dev@ramacher.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef UTILS_MODECHANGE_H
#define UTILS_MODECHANGE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#define MODE_MASK_EQUALS 1
#define MODE_MASK_PLUS 2
#define MODE_MASK_MINUS 4
#define MODE_MASK_ALL MODE_MASK_EQUALS | MODE_MASK_PLUS | MODE_MASK_MINUS

typedef struct mode_change_s mode_change;

/* Compile mode string. mode can either be a mode in its octal representation or
 * a strings matching [ugoa]*[+-=][rwxXstugo]+ separated by commas. */
mode_change* mode_compile(const char* mode, unsigned int flags);
/* Apply compiled mode changes to oldmode */
mode_t mode_adjust(mode_t oldmode, const mode_change* mc);
void mode_free(mode_change* mc);

#endif
