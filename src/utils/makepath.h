/*
 * makepath.h -- recurcive mkdir
 *
 * Yet Another FTP Client
 * Copyright (C) 2013, Sebastian Ramacher <sebastian+dev@ramacher.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef UTILS_MAKEPATH_H
#define UTILS_MAKEPATH_H

#include <stdbool.h>

/* Create directory recursively */
bool make_path(const char* path);

#endif
