/*
 * socket.c --
 *
 * Yet Another FTP Client
 * Copyright (C) 2013, Sebastian Ramacher <sebastian+dev@ramacher.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#include "socket-impl.h"
#include "xmalloc.h"

void sock_destroy(Socket* sockp)
{
  if (!sockp)
    return;

  if (sockp->destroy)
    sockp->destroy(sockp);
}

bool sock_connect_addr(Socket *sockp, const struct sockaddr* sa,
                       socklen_t salen)
{
  if (!sockp || !sockp->connect_addr)
    return false;

  return sockp->connect_addr(sockp, sa, salen);
}

bool sock_connect_host(Socket *sockp, Host *hp)
{
  if (!sockp || !hp || !sockp->connect_addr || sockp->connected)
    return false;

  const struct addrinfo* addr = host_getaddrinfo(hp);
  for (; addr != NULL; addr = addr->ai_next)
  {
    if (sock_connect_addr(sockp, addr->ai_addr, addr->ai_addrlen))
    {
      host_connect_addr(hp, addr);
      return true;
    }
  }

  return false;
}

void sock_copy(Socket *tosock, const Socket *fromsock)
{
  if (fromsock && tosock && fromsock->copy)
    fromsock->copy(tosock, fromsock);
}

bool sock_connected(const Socket *sockp)
{
  return sockp && sockp->connected;
}

bool sock_accept(Socket *sockp, const char *mode, bool pasvmode)
{
  if (!sockp || !sockp->accept)
    return false;

  return sockp->accept(sockp, mode, pasvmode);
}

bool sock_listen(Socket* sockp, int family)
{
  if (!sockp || !sockp->listen)
    return false;

  return sockp->listen(sockp, family);
}

void sock_throughput(Socket *sockp)
{
  if (sockp && sockp->throughput)
    sockp->throughput(sockp);
}

void sock_lowdelay(Socket *sockp)
{
  if (sockp && sockp->lowdelay)
    sockp->lowdelay(sockp);
}

const struct sockaddr* sock_local_addr(Socket *sockp)
{
  if (!sockp)
    return NULL;

  return (const struct sockaddr*) &sockp->local_addr;
}

const struct sockaddr* sock_remote_addr(Socket *sockp)
{
  if (!sockp)
    return NULL;

  return (const struct sockaddr*) &sockp->remote_addr;
}

ssize_t sock_read(Socket *sockp, void *buf, size_t num)
{
  if (!sockp || !sockp->read)
    return -1;

  return sockp->read(sockp, buf, num);
}

ssize_t sock_write(Socket *sockp, const void* buf, size_t num)
{
  if (!sockp || !sockp->write)
    return -1;

  return sockp->write(sockp, buf, num);
}

int sock_get(Socket *sockp)
{
  if (!sockp || !sockp->get)
    return -1;

  return sockp->get(sockp);
}

int sock_put(Socket *sockp, int c)
{
  if (!sockp || !sockp->put)
    return -1;

  return sockp->put(sockp, c);
}

int sock_vprintf(Socket *sockp, const char *str, va_list ap)
{
  if (!sockp || !sockp->vprintf)
    return -1;

  return sockp->vprintf(sockp, str, ap);
}

int sock_krb_vprintf(Socket *sockp, const char *str, va_list ap)
{
  if (!sockp || !sockp->krb_vprintf)
    return -1;

  return sockp->krb_vprintf(sockp, str, ap);
}

int sock_printf(Socket *sockp, const char *str, ...)
{
  va_list ap;
  va_start(ap, str);
  const int r = sock_vprintf(sockp, str, ap);
  va_end(ap);
  return r;
}

int sock_krb_printf(Socket *sockp, const char *str, ...)
{
  va_list ap;
  va_start(ap, str);
  const int r = sock_krb_vprintf(sockp, str, ap);
  va_end(ap);
  return r;
}

/* flushes output */
int sock_flush(Socket *sockp)
{
  if (!sockp || !sockp->flush)
    return -1;

  return sockp->flush(sockp);
}

int sock_telnet_interrupt(Socket *sockp)
{
  if (!sockp || !sockp->telnet_interrupt)
    return -1;

  return sockp->telnet_interrupt(sockp);
}

void sock_clearerr_in(Socket* sockp)
{
  if (sockp && sockp->clearerr)
    sockp->clearerr(sockp, false);
}

void sock_clearerr_out(Socket* sockp)
{
  if (sockp && sockp->clearerr)
    sockp->clearerr(sockp, true);
}

int sock_error_in(Socket* sockp)
{
  if (!sockp || !sockp->error)
    return -1;

  return sockp->error(sockp, false);
}

int sock_error_out(Socket* sockp)
{
  if (!sockp || !sockp->error)
    return -1;

  return sockp->error(sockp, false);
}

void sock_fd_set(Socket* sockp, fd_set* fdset)
{
  if (sockp && sockp->fd_set && fdset)
    sockp->fd_set(sockp, fdset);
}

int sock_select(Socket* sockp, fd_set* readfds, fd_set* writefds,
                fd_set* errorfds, struct timeval* timeout)
{
  if (!sockp || !sockp->select)
    return -1;

  return sockp->select(sockp, readfds, writefds, errorfds, timeout);
}

int sock_eof(Socket* sockp) {
  if (!sockp || !sockp->eof)
    return -1;

  return sockp->eof(sockp);
}
