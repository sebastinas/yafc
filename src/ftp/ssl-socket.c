/*
 * ssl-socket.c -- SSL socket implementation
 *
 * Yet Another FTP Client
 * Copyright (C) 2013, Sebastian Ramacher <sebastian+dev@ramacher.at>
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

#include <openssl/ssl.h>
#include <openssl/bio.h>

struct socket_impl_
{
  int handle;
  SSL_CTX* ctx;
  SSL* ssl;
  BIO* bio;
};

static void ssls_destroy(Socket* sockp)
{
  if (sockp->data->handle != -1)
    close(sockp->data->handle);
  free(sockp);
}

static bool ssls_getsockname(Socket *sockp, struct sockaddr_storage* sa)
{
  socklen_t len = sizeof(struct sockaddr_storage);
  if (getsockname(sockp->data->handle, (struct sockaddr*)sa, &len) == -1)
    return false;
  return true;
}

static bool ssls_connect_addr(Socket *sockp, const struct sockaddr* sa,
                            socklen_t salen)
{
  if (sockp->data->handle != -1)
    return false;

  sockp->data->handle = socket(sa->sa_family, SOCK_STREAM, IPPROTO_TCP);
  if (sockp->data->handle == -1)
    return false;

  // connect to the socket
  if (connect(sockp->data->handle, sa, salen) == -1)
  {
    perror("connect()");
    close(sockp->data->handle);
    sockp->data->handle = -1;
    return false;
  }

  if (!ssls_getsockname(sockp, &sockp->local_addr))
  {
    perror("getsockname()");
    close(sockp->data->handle);
    sockp->data->handle = -1;
    return false;
  }
  memcpy(&sockp->remote_addr, sa, salen);

  // init SSL context
  const SSL_METHOD* method = SSLv23_client_method();
  sockp->data->ctx = SSL_CTX_new(method);
  if (sockp->data->ctx == NULL)
  {
    close(sockp->data->handle);
    sockp->data->handle = -1;
    return false;
  }

  // disable SSLv2
  SSL_CTX_set_options(sockp->data->ctx, SSL_OP_NO_SSLv2);

  sockp->data->ssl = SSL_new(sockp->data->ctx);
  SSL_set_fd(sockp->data->ssl, sockp->data->handle);
  SSL_set_mode(sockp->data->ssl, SSL_MODE_AUTO_RETRY);

  /* BIO* bio = BIO_new_socket(sockp->data->handle, BIO_NOCLOSE);
  SSL_set_bio(ssl, bio, bio); */


  // SSL handshake
  if (SSL_connect(sockp->data->ssl) != 1)
  {
    SSL_free(sockp->data->ssl);
    sockp->data->ssl = NULL;
    SSL_CTX_free(sockp->data->ctx);
    close(sockp->data->handle);
    sockp->data->handle = -1;
  }

  sockp->data->bio = BIO_new(BIO_f_buffer());
  BIO* ssl_bio = BIO_new(BIO_f_ssl());
  BIO_set_ssl(ssl_bio, sockp->data->ssl, BIO_CLOSE);
  BIO_push(sockp->data->bio, ssl_bio);

  sockp->connected = true;
  return true;
}

static void ssls_throughput(Socket *sockp)
{
#if defined(IPTOS_THROUGHPUT) && defined(HAVE_SETSOCKOPT)
  /* set type of service */
  int tos = IPTOS_THROUGHPUT;
  if(setsockopt(sockp->data->handle, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) == -1)
    perror(_("setsockopt(THROUGHPUT)... ignored"));
#endif
}

static void ssls_lowdelay(Socket *sockp)
{
#if defined(IPTOS_LOWDELAY) && defined(HAVE_SETSOCKOPT)
  /* set type of service */
  int tos = IPTOS_LOWDELAY;
  if(setsockopt(sockp->data->handle, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) == -1)
    perror(_("setsockopt(LOWDELAY)... ignored"));
#endif
}

static ssize_t ssls_read(Socket *sockp, void *buf, size_t num)
{
  return BIO_read(sockp->data->bio, buf, num);
}

static ssize_t ssls_write(Socket *sockp, const void* buf, size_t num)
{
  return BIO_write(sockp->data->bio, buf, num);
}

static int ssls_get(Socket *sockp)
{
  unsigned char tmp = 0;
  const int ret = ssls_read(sockp, &tmp, sizeof(unsigned char));
  if (ret == 0)
    return EOF;
  else if (ret == -1)
    return -1;
  return tmp;
}

static int ssls_put(Socket *sockp, int c)
{
  const unsigned char tmp = c;
  const int ret = ssls_write(sockp, &tmp, sizeof(unsigned char));
  if (ret == 0)
    return EOF;
  return ret;
}

static int ssls_unget(Socket *sockp, int c)
{
  return 0; // TODO
}

static int ssls_vprintf(Socket *sockp, const char *str, va_list ap)
{
  char* tmp = NULL;
  if (vasprintf(&tmp, str, ap) == -1)
    return -1;

  int ret = BIO_puts(sockp->data->bio, tmp);
  free(tmp);
  return ret;
}

/* flushes output */
static int ssls_flush(Socket *sockp)
{
  return BIO_flush(sockp->data->bio);
}

static int ssls_error(Socket* sockp, bool inout)
{
  // return ferror(inout ? sockp->data->sout : sockp->data->sin);
  return 0;
}

static void ssls_fd_set(Socket* sockp, fd_set* fdset)
{
  FD_SET(sockp->data->handle, fdset);
}

static int ssls_select(Socket* sockp, fd_set* readfds, fd_set* writefds,
                     fd_set* errorfds, struct timeval* timeout)
{
  return select(sockp->data->handle + 1, readfds, writefds, errorfds, timeout);
}

static int ssls_eof(Socket* sockp)
{
  return BIO_eof(sockp->data->bio);
}

Socket* sock_ssl_create(void)
{
  Socket* sock = xmalloc(sizeof(Socket));
  memset(sock, 0, sizeof(Socket));

  sock->data = xmalloc(sizeof(socket_impl));
  memset(sock->data, 0, sizeof(socket_impl));
  sock->data->handle = -1;

  sock->destroy = ssls_destroy;
  sock->connect_addr = ssls_connect_addr;
  sock->copy = NULL;
  sock->accept = NULL;
  sock->listen = NULL;
  sock->throughput = ssls_throughput;
  sock->lowdelay = ssls_lowdelay;
  sock->read = ssls_read;
  sock->write = ssls_write;
  sock->get = ssls_get;
  sock->put = ssls_put;
  sock->unget = ssls_unget;
  sock->vprintf = ssls_vprintf;
  sock->krb_vprintf = ssls_vprintf;
  sock->flush = ssls_flush;
  sock->eof = ssls_eof;
  sock->telnet_interrupt = NULL;
  sock->fd_set = ssls_fd_set;
  sock->select = ssls_select;
  sock->clearerr = NULL;
  sock->error = ssls_error;

  return sock;
}
