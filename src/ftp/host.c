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

#include "syshdr.h"
#include "host.h"
#include "gvars.h"

/* saved reply from getservent */
static int ftp_servent_port = -1; /* -1 == not initialized */

Host *host_create(const url_t *urlp)
{
	Host *hostp;

	hostp = (Host *)xmalloc(sizeof(Host));

	hostp->hep = 0;
	hostp->hostname = xstrdup(urlp->hostname);
	hostp->port = urlp->port; /* host byte order */
	if(hostp->port <= 0)
		hostp->port = -1;
	else
		hostp->port = htons(hostp->port); /* to network byte order */

	return hostp;
}

void host_destroy(Host *hostp)
{
	if(hostp) {
		free(hostp->ipnum);
		free(hostp->hostname);
		free(hostp->ohostname);
		free(hostp);
	}
}

static int inet_pton_(const char* hostname, struct sockaddr_storage* in)
{
  if (in->ss_family == AF_INET)
    return inet_pton(in->ss_family, hostname,
        &((struct sockaddr_in*)in)->sin_addr);
#ifdef HAVE_IPV6
  else if (in->ss_family == AF_INET6)
    return inet_pton(in->ss_family, hostname,
        &((struct sockaddr_in6*)in)->sin6_addr);
#endif
  return 0;
}

static struct hostent* gethostbyaddr_(struct sockaddr_storage* in)
{
  if (in->ss_family == AF_INET)
    return gethostbyaddr(&SOCK_IN4(IN),
        sizeof(struct in_addr), in->ss_family);
#ifdef HAVE_IPV6
  else if (in->ss_family == AF_INET6)
    return gethostbyaddr(&SOCK_IN6(in),
        sizeof(struct in6_addr), in->ss_family);
#endif
  return NULL;
}


/* returns 0 on success, -1 on failure, (sets h_errno)
 */
int host_lookup(Host *hostp)
{
  if (!hostp->hostname)
		return -1;

	struct sockaddr_storage ia;
  int family = AF_INET;
#ifdef HAVE_IPV6
  if (strchr(hostp->hostname, ':'))
    family = AF_INET6;
#endif
  ia.ss_family = family;

	/* Check if host is given in dotted-decimal format (IPv4) or a valid IPv6
   * address. */
	if (inet_pton_(hostp->hostname, &ia))
  {
		if (gvReverseDNS)
			hostp->hep = gethostbyaddr_(&ia);
		if (!hostp->hep)
    {
      if (ia.ss_family == AF_INET)
      {
			  hostp->alt_h_length = sizeof(struct in_addr);
        memcpy(&hostp->alt_h_addr.in4, &SOCK_IN4(&ia),
              hostp->alt_h_length);
      }
#ifdef HAVE_IPV6
      else if (ia.ss_family == AF_INET6)
      {
        hostp->alt_h_length = sizeof(struct in6_addr);
        memcpy(&hostp->alt_h_addr.in6, &SOCK_IN6(&ia),
              hostp->alt_h_length);
      }
#endif
      else
        return -1;
			
			hostp->ipnum = xstrdup(hostp->hostname);
			hostp->ohostname = xstrdup(hostp->ipnum);
		}
	}
	else
  {
		hostp->hep = gethostbyname(hostp->hostname);
		if (!hostp->hep)
			return -1;
	}

	if (hostp->hep)
  {
    if (hostp->hep->h_addrtype == AF_INET)
    {
		  struct in_addr tmp;
		  memcpy(&tmp, hostp->hep->h_addr, hostp->hep->h_length);
		  hostp->ipnum = xstrdup(inet_ntoa(tmp));
    }
#ifdef HAVE_IPV6
    else if (hostp->hep->h_addrtype == AF_INET6)
    {
      struct in6_addr addr;
      memcpy(&addr, hostp->hep->h_addr, hostp->hep->h_length);

      char straddr[INET6_ADDRSTRLEN];
      inet_ntop(AF_INET6, &addr, straddr, sizeof(straddr));
      hostp->ipnum = xstrdup(straddr);
    }
#endif
    else
      return -1;

    hostp->ohostname = xstrdup(hostp->hep->h_name); /* official name of host */
	}

	/* let system pick port */
	if(hostp->port == -1) {
		if(ftp_servent_port == -1) {
			struct servent *sep;

			sep = getservbyname("ftp", "tcp");
			if(sep == 0)
				ftp_servent_port = 21;
			else
				ftp_servent_port = sep->s_port;
		}
		hostp->port = ftp_servent_port;
	}

	return 0;
}

/* returns port in network byte order */
unsigned short int host_getport(const Host *hostp)
{
	return hostp->port;
}

/* returns port in host byte order */
unsigned short int host_gethport(const Host *hostp)
{
	return ntohs(hostp->port);
}

/* returns IP number */
const char *host_getip(const Host *hostp)
{
	return hostp->ipnum;
}

/* returns name as passed to host_set() */
const char *host_getname(const Host *hostp)
{
	return hostp->hostname;
}

/* returns official name (as returned from gethostbyname()) */
const char *host_getoname(const Host *hostp)
{
	return hostp->ohostname;
}

