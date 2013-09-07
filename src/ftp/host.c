/*
 * host.c -- DNS lookups of hostnames
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
 * Copyright (C) 2012, Sebastian Ramacher <sebastian+dev@ramacher.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING for more details.
 */

#include "host.h"

// AI_ADDRCONFIG and AI_V4MAPPED are not defined on OpenBSD
#ifndef AI_ADDRCONFIG
#define AI_ADDRCONFIG 0
#endif

#ifndef AI_V4MAPPED
#define AI_V4MAPPED 0
#endif

struct Host_
{
  char* hostname;
  int port;
  int ret;

  struct addrinfo* addr;
  const struct addrinfo* connected_addr;
};

Host* host_create(const url_t* urlp)
{
  Host* hostp = xmalloc(sizeof(Host));
  hostp->hostname = xstrdup(urlp->hostname);
  hostp->port = urlp->port; /* host byte order */
  hostp->ret = 0;
  if(hostp->port <= 0)
    hostp->port = -1;
  else
    hostp->port = htons(hostp->port); /* to network byte order */
  hostp->addr = NULL;
  hostp->connected_addr = NULL;

  return hostp;
}

void host_destroy(Host *hostp)
{
  if (!hostp)
    return;

  free(hostp->hostname);
  if (hostp->addr)
    freeaddrinfo(hostp->addr);
  free(hostp);
}

bool host_lookup(Host* hostp)
{
  if (!hostp || !hostp->hostname)
    return false;

  char* service = NULL;
  if (hostp->port != -1)
  {
    if (asprintf(&service, "%d", ntohs(hostp->port)) == -1)
      return false;
  }
  else
    service = xstrdup("ftp");

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
#ifdef HAVE_IPV6
  hints.ai_family = AF_UNSPEC;
#else
  hints.ai_family = AF_INET;
#endif
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_ADDRCONFIG | AI_CANONNAME;
#ifdef HAVE_IPV6
  hints.ai_flags |= AI_V4MAPPED;
#endif
  if (hostp->port != -1)
    hints.ai_flags |= AI_NUMERICSERV;

  hostp->ret = getaddrinfo(hostp->hostname, service, &hints, &hostp->addr);
  free(service);
  if (hostp->ret < 0)
  {
    /* Lookup with "ftp" as service. Try again with 21. */
    if (hostp->port == -1)
      hostp->ret = getaddrinfo(hostp->hostname, "21", &hints, &hostp->addr);

    if (hostp->ret < 0)
      return false;
  }

  if (hostp->port == -1)
  {
    if (hostp->addr->ai_family == AF_INET)
      hostp->port = ((struct sockaddr_in*)hostp->addr->ai_addr)->sin_port;
#ifdef HAVE_IPV6
    else if (hostp->addr->ai_family == AF_INET6)
      hostp->port = ((struct sockaddr_in6*)hostp->addr->ai_addr)->sin6_port;
#endif
  }

  return true;
}

/* returns port in network byte order */
uint16_t host_getport(const Host *hostp)
{
  return hostp->port;
}

/* returns port in host byte order */
uint16_t host_gethport(const Host *hostp)
{
  return ntohs(hostp->port);
}

/* returns name as passed to host_set() */
const char *host_getname(const Host *hostp)
{
  if (!hostp)
    return NULL;

  return hostp->hostname;
}

const char* host_geterror(const Host* hostp)
{
  if (!hostp)
    return NULL;

  return gai_strerror(hostp->ret);
}

const char *host_getoname(const Host *hostp)
{
  if (!hostp || !hostp->addr)
    return NULL;

  return hostp->addr->ai_canonname;
}

const struct addrinfo* host_getaddrinfo(const Host* hostp)
{
  if (!hostp)
    return NULL;

  return hostp->addr;
}

char* host_getip(const Host* hostp)
{
  if (!hostp || !hostp->connected_addr)
    return NULL;

  return printable_address(hostp->connected_addr->ai_addr);
}

char* printable_address(const struct sockaddr* sa)
{
  char res[INET6_ADDRSTRLEN];
  if (sa->sa_family == AF_INET)
    inet_ntop(sa->sa_family, &((const struct sockaddr_in*)sa)->sin_addr, res, INET6_ADDRSTRLEN);
#ifdef HAVE_IPV6
  else if (sa->sa_family == AF_INET6)
    inet_ntop(sa->sa_family, &((const struct sockaddr_in6*)sa)->sin6_addr, res, INET6_ADDRSTRLEN);
#endif
  else
    return NULL;

  return xstrdup(res);
}

void host_connect_addr(Host* hostp, const struct addrinfo* info)
{
  if (!hostp)
    return;

  hostp->connected_addr = info;
}

