/*
 * base64.h -- base64 support
 *
 * Yet Another FTP Client
 * Copyright (C) 2012, Sebastian Ramacher <sebastian+dev@ramacher.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef YAFC_BASE64_H
#define YAFC_BASE64_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_BASE64_HEIMDAL
#include <heimdal/base64.h>
#else
#include <stddef.h>

int base64_encode(const void* data, size_t size, char** str);
int base64_decode(const char* str, void* data);
#endif

#endif
