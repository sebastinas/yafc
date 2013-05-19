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

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

struct socket_impl_
{
  int handle;
  gnutls_session_t session;
  bool handshake_on_connect;
};

static void ssls_destroy(Socket* sockp)
{
  if (sockp->data->handle != -1)
  {
    gnutls_bye(sockp->data->session, GNUTLS_SHUT_RDWR);
    shutdown(sockp->data->handle, SHUT_RDWR);
    close(sockp->data->handle);
  }
  gnutls_deinit(sockp->data->session);
  free(sockp);
}

static int verify_callback(gnutls_session_t session)
{
  return 0;
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

  // gnutls_transport_set_int(sockp->data->session, sockp->data->handle);
  gnutls_transport_set_ptr(sockp->data->session,(gnutls_transport_ptr_t)sockp->data->handle);
  // gnutls_handshake_set_timeout(sockp->data->session, GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);

  if (sockp->data->handshake_on_connect)
  {
    // perform TLS handshake
    int ret = 0;
    do
    {
      ret = gnutls_handshake(sockp->data->session);
    } while (ret < 0 && gnutls_error_is_fatal(ret) == 0);

    if (ret < 0)
    {
      gnutls_perror(ret);
      close(sockp->data->handle);
      sockp->data->handle = -1;
      return false;
    }
  }

  sockp->connected = true;
  return true;
}

static bool ssls_dup(const Socket* fromsock, Socket** tosock)
{
  Socket* tmp = sock_ssl_create();
  if (!tmp)
    return false;

  memcpy(&tmp->remote_addr, &fromsock->remote_addr,
       sizeof(fromsock->remote_addr));
  memcpy(&tmp->local_addr, &fromsock->local_addr,
       sizeof(fromsock->local_addr));
  // tmp->connected = fromsock->connected;
  tmp->data->handshake_on_connect = false;

  size_t size = 0;
  int res = gnutls_session_get_data(fromsock->data->session, NULL, &size);
  if (res && res != GNUTLS_E_SHORT_MEMORY_BUFFER)
  {
    ssls_destroy(tmp);
    return false;
  }

  char* data = xmalloc(size);
  if (gnutls_session_get_data(fromsock->data->session, data, &size))
  {
    free(data);
    ssls_destroy(tmp);
    return false;
  }

  res = gnutls_session_set_data(tmp->data->session, data, size);
  free(data);
  if (res)
  {
    ssls_destroy(tmp);
    return false;
  }

  *tosock = tmp;
  return true;
}

static bool ssls_accept(Socket* sockp, const char* mode, bool pasvmod)
{
  if (!pasvmod)
    return false;

  if (!sockp->data->handshake_on_connect)
  {
    // perform TLS handshake
    int ret = 0;
    do
    {
      ret = gnutls_handshake(sockp->data->session);
    } while (ret < 0 && gnutls_error_is_fatal(ret) == 0);

    if (ret < 0)
    {
      gnutls_perror(ret);
      close(sockp->data->handle);
      sockp->data->handle = -1;
      return false;
    }
  }

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
  int ret = gnutls_record_recv(sockp->data->session, buf, num);
  return ret;
}

static ssize_t ssls_write(Socket *sockp, const void* buf, size_t num)
{
  int ret = gnutls_record_send(sockp->data->session, buf, num);
  return ret;
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

static int ssls_vprintf(Socket *sockp, const char *str, va_list ap)
{
  char* tmp = NULL;
  const int size = vasprintf(&tmp, str, ap);
  if (size == -1)
    return -1;

  int ret = ssls_write(sockp, tmp, size);
  free(tmp);
  return ret;
}

/* flushes output */
static int ssls_flush(Socket *sockp)
{
  return 0; // gnutls_record_uncork(sockp->data->session, GNUTLS_RECORD_WAIT);
}

static int ssls_error(Socket* sockp, bool inout)
{
  // return ferror(inout ? sockp->data->sout : sockp->data->sin);
  return 0;
}

static int ssls_check_pending(Socket* sockp, bool inout)
{
  if (inout)
    return 1;

  size_t r = gnutls_record_check_pending(sockp->data->session);
  if (r > 0)
    return 1;

  struct timeval tv;
  fd_set fds;
  /* watch fd to see if it has input */
  FD_ZERO(&fds);
  FD_SET(sockp->data->handle, &fds);
  /* wait max 0.5 second */
  tv.tv_sec = 0;
  tv.tv_usec = 500;

  return select(sockp->data->handle + 1, &fds, NULL, NULL, &tv);
}

static int ssls_eof(Socket* sockp)
{
  return 0; // BIO_eof(sockp->data->bio);
}

static Socket* sock_ssl_create_impl(bool create_ssl_ctx)
{
  Socket* sock = xmalloc(sizeof(Socket));
  memset(sock, 0, sizeof(Socket));

  sock->data = xmalloc(sizeof(socket_impl));
  memset(sock->data, 0, sizeof(socket_impl));
  sock->data->handle = -1;
  sock->data->handshake_on_connect = true;

  sock->destroy = ssls_destroy;
  sock->connect_addr = ssls_connect_addr;
  sock->dup = ssls_dup;
  sock->accept = ssls_accept;
  sock->listen = NULL;
  sock->throughput = ssls_throughput;
  sock->lowdelay = ssls_lowdelay;
  sock->read = ssls_read;
  sock->write = ssls_write;
  sock->get = ssls_get;
  sock->put = ssls_put;
  sock->vprintf = ssls_vprintf;
  sock->krb_vprintf = ssls_vprintf;
  sock->flush = ssls_flush;
  sock->eof = ssls_eof;
  sock->telnet_interrupt = NULL;
  sock->clearerr = NULL;
  sock->error = ssls_error;
  sock->check_pending = ssls_check_pending;

  gnutls_init(&sock->data->session, GNUTLS_CLIENT);
  gnutls_set_default_priority(sock->data->session);

  gnutls_certificate_credentials_t cred;
  gnutls_certificate_allocate_credentials(&cred);
  gnutls_certificate_set_x509_trust_file(cred, "/etc/ssl/certs/ca-certificates.crt", GNUTLS_X509_FMT_PEM);
  gnutls_certificate_set_verify_function(cred, verify_callback);
  gnutls_credentials_set(sock->data->session, GNUTLS_CRD_CERTIFICATE, cred);

  return sock;
}

Socket* sock_ssl_create(void)
{
  return sock_ssl_create_impl(true);
}

