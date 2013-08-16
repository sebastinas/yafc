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

#include <stddef.h>

int b64_encode(const void* data, size_t size, char** str);
int b64_decode(const char* str, void* data);

#endif
