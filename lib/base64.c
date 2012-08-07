/*
 * base64.c -- base64 support via OpenSSL
 *
 * Yet Another FTP Client
 * Copyright (C) 2012, Sebastian Ramacher <sebastian+dev@ramacher.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#include "base64.h"

#if defined(HAVE_BASE64_OPENSSL)
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <string.h>

int base64_encode(const void* data, size_t size, char** str)
{
  if (!str || !data)
    return -1;

  BIO* b64 = BIO_new(BIO_f_base64());
  BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

  BIO* mem = BIO_new(BIO_s_mem());
  b64 = BIO_push(b64, mem);

  while (BIO_write(b64, data, size) <= 0)
  {
    if (BIO_should_retry(b64))
      continue;
    BIO_free_all(b64);
    return -1;
  }
  BIO_flush(b64);

  char* tmp = NULL;
  size_t len = BIO_get_mem_data(mem, &tmp);
  *str = malloc(len);
  if (!*str)
  {
    BIO_free_all(b64);
    return -1;
  }

  strncpy(*str, tmp, len);
  if (len > 0)
    (*str)[len - 1] = '\0';

  BIO_free_all(b64);
  return len;
}

int base64_decode(const char* str, void* data)
{
  if (!str || !data)
    return -1;

  BIO* b64 = BIO_new(BIO_f_base64());
  BIO* mem = BIO_new_mem_buf((char*) str, -1);
  b64 = BIO_push(b64, mem);

  size_t len = strlen(str);
  int res = 0;
  while ((res = BIO_read(b64, data, len)) <= 0)
  {
    if (BIO_should_retry(b64))
      continue;
    BIO_free_all(b64);
    return -1;
  }

  BIO_free_all(b64);
  return res;
}
#else
#error no suitable base64 implementation available
#endif
