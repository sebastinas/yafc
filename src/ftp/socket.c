/*
 * socket.c --
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 * Copyright (C) 2012-2013, Sebastian Ramacher <sebastian+dev@ramacher.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#include "syshdr.h"

#include "ftp.h"
#include "socket.h"
#include "xmalloc.h"

struct Socket_
{
  int handle;
  FILE *sin, *sout;

  struct sockaddr_storage local_addr, remote_addr;
  bool connected;
};

static bool create_streams(Socket* sock, const char* inmode,
                           const char* outmode)
{
  if (!sock || sock->handle == -1)
    return false;

  if (sock->sin || sock->sout)
    return true;

  /* create streams */
  int tfd = dup(sock->handle);
  if (tfd == -1)
    return false;

  sock->sin = fdopen(tfd, inmode);
  if (!sock->sin)
  {
    close(tfd);
    return false;
  }

  tfd = dup(sock->handle);
  if (tfd == -1)
  {
    fclose(sock->sin);
    sock->sin = NULL;
    return false;
  }

  sock->sout = fdopen(tfd, outmode);
  if (!sock->sout)
  {
    close(tfd);
    fclose(sock->sin);
    sock->sin = NULL;
    return false;
  }

  return true;
}

static void destroy_streams(Socket* sockp)
{
  if (sockp->sin)
    fclose(sockp->sin);
  if (sockp->sin != sockp->sout && sockp->sout)
    fclose(sockp->sout);

  sockp->sin = sockp->sout = NULL;
}

Socket* sock_create(void)
{
  Socket* sock = xmalloc(sizeof(Socket));
  sock->handle = -1;
  return sock;
}

void sock_destroy(Socket* sockp)
{
  if (!sockp)
    return;

  destroy_streams(sockp);
  if (sockp->handle != -1)
    close(sockp->handle);
  free(sockp);
}

bool sock_connect_addr(Socket *sockp, const struct sockaddr* sa,
                       socklen_t salen)
{
  if (!sockp || sockp->handle != -1)
    return false;

  sockp->handle = socket(sa->sa_family, SOCK_STREAM, IPPROTO_TCP);
  if (sockp->handle == -1)
    return false;

  /* connect to the socket */
  if (connect(sockp->handle, sa, salen) == -1)
  {
    perror("connect()");
    close(sockp->handle);
    sockp->handle = -1;
    return false;
  }

  if (sock_getsockname(sockp, &sockp->local_addr) == -1)
  {
    perror("getsockname()");
    close(sockp->handle);
    sockp->handle = -1;
    return false;
  }
  memcpy(&sockp->remote_addr, sa, salen);

  if (!create_streams(sockp, "r", "w"))
  {
    close(sockp->handle);
    sockp->handle = -1;
    return false;
  }

  sockp->connected = true;
  return true;
}

bool sock_connect_host(Socket *sockp, Host *hp)
{
  if (!sockp || !hp || sockp->handle != -1)
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
  memcpy(&tosock->remote_addr, &fromsock->remote_addr,
       sizeof(tosock->remote_addr));
  memcpy(&tosock->local_addr, &fromsock->local_addr,
       sizeof(tosock->local_addr));
  tosock->connected = sock_connected(fromsock);
}

bool sock_connected(const Socket *sockp)
{
  return sockp && sockp->connected;
}

/* accept an incoming connection */
bool sock_accept(Socket *sockp, const char *mode, bool pasvmode)
{
  if (!sockp)
    return false;

  if (!pasvmode)
  {
    struct sockaddr_storage sa;
    socklen_t l = sizeof(sa);

    int s = accept(sockp->handle, (struct sockaddr*)&sa, &l);
    close(sockp->handle);
    if (s == -1)
    {
      perror("accept()");
      sockp->handle = -1;
      return false;
    }
    sockp->handle = s;
    memcpy(&sockp->local_addr, &sa, l);
  }

  if (!create_streams(sockp, mode, mode))
  {
    close(sockp->handle);
    sockp->handle = -1;
    return false;
  }

  return true;
}

bool sock_listen(Socket* sockp, int family)
{
  if (!sockp || sockp->handle != -1)
    return false;

  sockp->handle = socket(family, SOCK_STREAM, IPPROTO_TCP);
  if (sockp->handle == -1)
    return false;

  socklen_t len = sizeof(struct sockaddr_storage);
  /* let system pick port TODO */
  memset(&sockp->local_addr, 0, len);
  sockp->local_addr.ss_family = family;

  if (bind(sockp->handle, (struct sockaddr *)&sockp->local_addr, len) == -1)
  {
    perror("bind()");
    close(sockp->handle);
    sockp->handle = -1;
    return false;
  }

  /* wait for an incoming connection */
  if (listen(sockp->handle, 1) == -1)
  {
    perror("listen()");
    close(sockp->handle);
    sockp->handle = -1;
    return false;
  }

  if (sock_getsockname(sockp, &sockp->local_addr) == -1)
  {
    close(sockp->handle);
    sockp->handle = -1;
    return false;
  }

  return true;
}

void sock_throughput(Socket *sockp)
{
#if defined(IPTOS_THROUGHPUT) && defined(HAVE_SETSOCKOPT)
  /* set type of service */
  int tos = IPTOS_THROUGHPUT;
  if(setsockopt(sockp->handle, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) == -1)
    perror(_("setsockopt(THROUGHPUT)... ignored"));
#endif
}

void sock_lowdelay(Socket *sockp)
{
#if defined(IPTOS_LOWDELAY) && defined(HAVE_SETSOCKOPT)
  /* set type of service */
  int tos = IPTOS_LOWDELAY;
  if(setsockopt(sockp->handle, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) == -1)
    perror(_("setsockopt(LOWDELAY)... ignored"));
#endif
}

const struct sockaddr* sock_local_addr(Socket *sockp)
{
  return (struct sockaddr*) &sockp->local_addr;
}

const struct sockaddr* sock_remote_addr(Socket *sockp)
{
  return (struct sockaddr*) &sockp->remote_addr;
}

ssize_t sock_read(Socket *sockp, void *buf, size_t num)
{
#ifdef SECFTP
  return sec_read(sockp->handle, buf, num);
#else
  return read(sockp->handle, buf, num);
#endif
}

ssize_t sock_write(Socket *sockp, void *buf, size_t num)
{
#ifdef SECFTP
  return sec_write(sockp->handle, buf, num);
#else
  return write(sockp->handle, buf, num);
#endif
}

int sock_get(Socket *sockp)
{
#ifdef SECFTP
	return sec_getc(sockp->sin);
#else
	return fgetc(sockp->sin);
#endif
}

int sock_put(Socket *sockp, int c)
{
  if (fputc(c, sockp->sout) == EOF)
    return EOF;
  return 0;
}

int sock_unget(Socket *sockp, int c)
{
  return ungetc(c, sockp->sin);
}

int sock_vprintf(Socket *sockp, const char *str, va_list ap)
{
  return vfprintf(sockp->sout, str, ap);
}

int sock_krb_vprintf(Socket *sockp, const char *str, va_list ap)
{
#ifdef SECFTP
  if (ftp->sec_complete)
    return sec_vfprintf(sockp->sout, str, ap);
  else
#endif
    return sock_vprintf(sockp, str, ap);
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
#ifdef SECFTP
  return sec_fflush(sockp->sout);
#else
  return fflush(sockp->sout);
#endif
}

int sock_telnet_interrupt(Socket *sockp)
{
  if (send(sockp->handle, "\xFF\xF4\xFF" /* IAC,IP,IAC */, 3, MSG_OOB) != 3)
    return -1;
  if (fputc(242 /*DM*/, sockp->sout) == EOF)
    return -1;
  return 0;
}

int sock_getsockname(Socket *sockp, struct sockaddr_storage* sa)
{
  socklen_t len = sizeof(struct sockaddr_storage);
  if (getsockname(sockp->handle, (struct sockaddr*)sa, &len) == -1)
    return -1;
  return 0;
}

int sock_handle(Socket* sockp)
{
  if (!sockp)
    return -1;

  return sockp->handle;
}

void sock_clearerr_in(Socket* sockp)
{
  if (!sockp)
    return;

  clearerr(sockp->sin);
}

void sock_clearerr_out(Socket* sockp)
{
  if (!sockp)
    return;

  clearerr(sockp->sout);
}

int sock_error_in(Socket* sockp)
{
  if (!sockp)
    return -1;

  return ferror(sockp->sin);
}

int sock_error_out(Socket* sockp)
{
  if (!sockp)
    return -1;

  return ferror(sockp->sout);
}

void sock_fd_set(Socket* sockp, fd_set* fdset)
{
  FD_SET(sockp->handle, fdset);
}

int sock_select(Socket* sockp, fd_set* readfds, fd_set* writefds,
                fd_set* errorfds, struct timeval* timeout)
{
  return select(sockp->handle + 1, readfds, writefds, errorfds, timeout);
}

int sock_eof(Socket* sockp) {
  if (!sockp)
    return -1;

  return feof(sockp->sin);
}
