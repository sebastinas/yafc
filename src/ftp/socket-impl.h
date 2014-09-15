/*
 * socket-impl.h --
 *
 * Yet Another FTP Client
 * Copyright (C) 2013, Sebastian Ramacher <sebastian+dev@ramacher.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#ifndef SOCKET_IMPL_H
#define SOCKET_IMPL_H

#include "socket.h"

typedef struct socket_impl_ socket_impl;

struct Socket_
{
  socket_impl* data;
  bool connected;
  struct sockaddr_storage local_addr, remote_addr;

  void (*destroy)(Socket *sockp);
  bool (*connect_addr)(Socket *sockp, const struct sockaddr* sa, socklen_t salen);
  bool (*dup)(const Socket *fromsock, Socket **tosock);
  bool (*accept)(Socket *sockp, const char *mode, bool pasvmode);
  bool (*listen)(Socket *sockp, int family);
  void (*throughput)(Socket *sockp);
  void (*lowdelay)(Socket *sockp);
  ssize_t (*read)(Socket *sockp, void *buf, size_t num);
  ssize_t (*write)(Socket *sockp, const void *buf, size_t num);
  int (*get)(Socket *sockp); /* get one character */
  int (*put)(Socket *sockp, int c); /* put one character */
  int (*vprintf)(Socket *sockp, const char *str, va_list ap);
  int (*krb_vprintf)(Socket *sockp, const char *str, va_list ap);
  int (*flush)(Socket *sockp);
  int (*eof)(Socket* sockp);
  int (*telnet_interrupt)(Socket *sockp);

  int (*check_pending)(Socket* sock, bool inout);

  void (*clear_error)(Socket* sockp, bool inout);
  int (*error)(Socket* sockp, bool input);
};

#endif
