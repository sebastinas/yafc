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

struct Host_
{
  char* hostname;
  uint16_t port;

  struct addrinfo* addr;
};

Host* host_create(const url_t* urlp)
{
  Host* hostp = xmalloc(sizeof(Host));
  hostp->hostname = xstrdup(urlp->hostname);
  hostp->port = urlp->port; /* host byte order */
  if(hostp->port <= 0)
    hostp->port = 0;
  else
    hostp->port = htons(hostp->port); /* to network byte order */
  hostp->addr = NULL;

  return hostp;
}

void host_destroy(Host *hostp)
{
  if (!hostp)
    return;
  
  free(hostp->hostname);
  freeaddrinfo(hostp->addr);
}

bool host_lookup(Host* hostp)
{
  if (!hostp || !hostp->hostname)
    return false;

  const char* service = hostp->port == -1 ? "ftp" : NULL;
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
#ifdef HAVE_IPv6
  hints.ai_flags |= AI_V4MAPPED;
#endif

  if (getaddrinfo(hostp->hostname, service, &hints, &hostp->addr) < 0)
    return false;

  if (hostp->port == 0)
  {
    if (hostp->addr->ai_family == AF_INET)
      hostp->port = ((struct sockaddr_in*)hostp->addr->ai_addr)->sin_port;
#ifdef HAVE_IPV6
    else if (hostp->addr->ai_family == AF_INET6)
      hostp->port = ((struct sockaddr_in6*)hostp->addr->ai_addr)->sin6_port;
#endif
  }
  else
  {
    for (struct addrinfo* addr = hostp->addr; addr != NULL; addr = addr->ai_next)
    {
      if (addr->ai_family == AF_INET)
        ((struct sockaddr_in*)hostp->addr->ai_addr)->sin_port = hostp->port;
#ifdef HAVE_IPV6
      else if (hostp->addr->ai_family == AF_INET6)
        ((struct sockaddr_in6*)hostp->addr->ai_addr)->sin6_port = hostp->port; 
#endif
    }
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

