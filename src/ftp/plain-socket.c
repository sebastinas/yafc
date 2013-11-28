/*
 * plain-socket.c -- plain socket implementation
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
#include "socket-impl.h"
#include "xmalloc.h"

struct socket_impl_
{
  int handle;
  FILE *sin, *sout;
};

static bool create_streams(socket_impl* sock, const char* inmode,
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

static void destroy_streams(socket_impl* sockp)
{
  if (sockp->sin)
    fclose(sockp->sin);
  if (sockp->sin != sockp->sout && sockp->sout)
    fclose(sockp->sout);

  sockp->sin = sockp->sout = NULL;
}

static void ps_destroy(Socket* sockp)
{
  destroy_streams(sockp->data);
  if (sockp->data->handle != -1)
    close(sockp->data->handle);
  free(sockp);
}

static bool ps_getsockname(Socket *sockp, struct sockaddr_storage* sa)
{
  socklen_t len = sizeof(struct sockaddr_storage);
  if (getsockname(sockp->data->handle, (struct sockaddr*)sa, &len) == -1)
    return false;
  return true;
}

static bool ps_connect_addr(Socket *sockp, const struct sockaddr* sa,
                            socklen_t salen)
{
  if (sockp->data->handle != -1)
    return false;

  sockp->data->handle = socket(sa->sa_family, SOCK_STREAM, IPPROTO_TCP);
  if (sockp->data->handle == -1)
    return false;

  /* connect to the socket */
  if (connect(sockp->data->handle, sa, salen) == -1)
  {
    perror("connect()");
    close(sockp->data->handle);
    sockp->data->handle = -1;
    return false;
  }

  if (!ps_getsockname(sockp, &sockp->local_addr))
  {
    perror("getsockname()");
    close(sockp->data->handle);
    sockp->data->handle = -1;
    return false;
  }
  memcpy(&sockp->remote_addr, sa, salen);

  if (!create_streams(sockp->data, "r", "w"))
  {
    close(sockp->data->handle);
    sockp->data->handle = -1;
    return false;
  }

  sockp->connected = true;
  return true;
}

static bool ps_dup(const Socket* fromsock, Socket** tosock)
{
  Socket* tmp = sock_create();
  if (!tmp)
    return false;

  memcpy(&tmp->remote_addr, &fromsock->remote_addr,
       sizeof(fromsock->remote_addr));
  memcpy(&tmp->local_addr, &fromsock->local_addr,
       sizeof(fromsock->local_addr));
  // tmp->connected = fromsock->connected;

  *tosock = tmp;
  return true;
}

/* accept an incoming connection */
static bool ps_accept(Socket *sockp, const char *mode, bool pasvmode)
{
  if (!pasvmode)
  {
    struct sockaddr_storage sa;
    socklen_t l = sizeof(sa);

    int s = accept(sockp->data->handle, (struct sockaddr*)&sa, &l);
    close(sockp->data->handle);
    if (s == -1)
    {
      perror("accept()");
      sockp->data->handle = -1;
      return false;
    }
    sockp->data->handle = s;
    memcpy(&sockp->local_addr, &sa, l);
  }

  if (!create_streams(sockp->data, mode, mode))
  {
    close(sockp->data->handle);
    sockp->data->handle = -1;
    return false;
  }

  return true;
}

static bool ps_listen(Socket* sockp, int family)
{
  if (sockp->data->handle != -1)
    return false;

  sockp->data->handle = socket(family, SOCK_STREAM, IPPROTO_TCP);
  if (sockp->data->handle == -1)
    return false;

  socklen_t len = sizeof(struct sockaddr_storage);
  /* let system pick the port */
  if (family == AF_INET)
    ((struct sockaddr_in*) &sockp->local_addr)->sin_port = 0;
#ifdef HAVE_IPV6
  else if (family == AF_INET6)
    ((struct sockaddr_in6*) &sockp->local_addr)->sin6_port = 0;
#endif
  else
  {
    close(sockp->data->handle);
    sockp->data->handle = -1;
    return false;
  }
  sockp->local_addr.ss_family = family;

  if (bind(sockp->data->handle, (struct sockaddr *)&sockp->local_addr, len) == -1)
  {
    perror("bind()");
    close(sockp->data->handle);
    sockp->data->handle = -1;
    return false;
  }

  /* wait for an incoming connection */
  if (listen(sockp->data->handle, 1) == -1)
  {
    perror("listen()");
    close(sockp->data->handle);
    sockp->data->handle = -1;
    return false;
  }

  if (!ps_getsockname(sockp, &sockp->local_addr))
  {
    close(sockp->data->handle);
    sockp->data->handle = -1;
    return false;
  }

  return true;
}

static void ps_throughput(Socket *sockp)
{
#if defined(IPTOS_THROUGHPUT) && defined(HAVE_SETSOCKOPT)
  /* set type of service */
  int tos = IPTOS_THROUGHPUT;
  if(setsockopt(sockp->data->handle, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) == -1)
    perror(_("setsockopt(THROUGHPUT)... ignored"));
#endif
}

static void ps_lowdelay(Socket *sockp)
{
#if defined(IPTOS_LOWDELAY) && defined(HAVE_SETSOCKOPT)
  /* set type of service */
  int tos = IPTOS_LOWDELAY;
  if(setsockopt(sockp->data->handle, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) == -1)
    perror(_("setsockopt(LOWDELAY)... ignored"));
#endif
}

static ssize_t ps_read(Socket *sockp, void *buf, size_t num)
{
#ifdef SECFTP
  return sec_read(sockp->data->handle, buf, num);
#else
  return read(sockp->data->handle, buf, num);
#endif
}

static ssize_t ps_write(Socket *sockp, const void* buf, size_t num)
{
#ifdef SECFTP
  return sec_write(sockp->data->handle, buf, num);
#else
  return write(sockp->data->handle, buf, num);
#endif
}

static int ps_get(Socket *sockp)
{
#ifdef SECFTP
	return sec_getc(sockp->data->sin);
#else
	return fgetc(sockp->data->sin);
#endif
}

static int ps_put(Socket *sockp, int c)
{
  if (fputc(c, sockp->data->sout) == EOF)
    return EOF;
  return 0;
}

static int ps_vprintf(Socket *sockp, const char *str, va_list ap)
{
  return vfprintf(sockp->data->sout, str, ap);
}

static int ps_krb_vprintf(Socket *sockp, const char *str, va_list ap)
{
#ifdef SECFTP
  if (ftp->sec_complete)
    return sec_vfprintf(sockp->data->sout, str, ap);
  else
#endif
    return ps_vprintf(sockp, str, ap);
}

/* flushes output */
static int ps_flush(Socket *sockp)
{
#ifdef SECFTP
  return sec_fflush(sockp->data->sout);
#else
  return fflush(sockp->data->sout);
#endif
}

static int ps_telnet_interrupt(Socket *sockp)
{
  if (send(sockp->data->handle, "\xFF\xF4\xFF" /* IAC,IP,IAC */, 3, MSG_OOB) != 3)
    return -1;
  if (fputc(242 /*DM*/, sockp->data->sout) == EOF)
    return -1;
  return 0;
}

static void ps_clearerr(Socket* sockp, bool inout)
{
  clearerr(inout ? sockp->data->sout : sockp->data->sin);
}

static int ps_error(Socket* sockp, bool inout)
{
  return ferror(inout ? sockp->data->sout : sockp->data->sin);
}

static int ps_check_pending(Socket* sockp, bool inout)
{
  struct timeval tv;
  fd_set fds;

  	/* watch fd to see if it has input */
	FD_ZERO(&fds);
  FD_SET(sockp->data->handle, &fds);
	/* wait max 0.5 second */
	tv.tv_sec = 0;
	tv.tv_usec = 500;

	if (!inout) /* wait for read */
		return select(sockp->data->handle + 1, &fds, NULL, NULL, &tv);
	else /* wait for write */
		return select(sockp->data->handle + 1, NULL, &fds, NULL, &tv);
}

static int ps_eof(Socket* sockp)
{
  return feof(sockp->data->sin);
}

Socket* sock_create(void)
{
  Socket* sock = xmalloc(sizeof(Socket));
  memset(sock, 0, sizeof(Socket));

  sock->data = xmalloc(sizeof(socket_impl));
  memset(sock->data, 0, sizeof(socket_impl));
  sock->data->handle = -1;

  sock->destroy = ps_destroy;
  sock->connect_addr = ps_connect_addr;
  sock->dup = ps_dup;
  sock->accept = ps_accept;
  sock->listen = ps_listen;
  sock->throughput = ps_throughput;
  sock->lowdelay = ps_lowdelay;
  sock->read = ps_read;
  sock->write = ps_write;
  sock->get = ps_get;
  sock->put = ps_put;
  sock->vprintf = ps_vprintf;
  sock->krb_vprintf = ps_krb_vprintf;
  sock->flush = ps_flush;
  sock->eof = ps_eof;
  sock->telnet_interrupt = ps_telnet_interrupt;
  sock->check_pending = ps_check_pending;
  sock->clearerr = ps_clearerr;
  sock->error = ps_error;

  return sock;
}
