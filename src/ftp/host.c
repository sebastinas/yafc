/* $Id: host.c,v 1.4 2001/05/12 18:44:04 mhe Exp $
 *
 * host.c -- DNS lookups of hostnames
 *
 * Yet Another FTP Client
 * Copyright (C) 1998-2001, Martin Hedenfalk <mhe@stacken.kth.se>
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
		xfree(hostp->ipnum);
		xfree(hostp->hostname);
		xfree(hostp->ohostname);
		xfree(hostp);
	}
}

/* returns 0 on success, -1 on failure, (sets h_errno)
 */
int host_lookup(Host *hostp)
{
	struct in_addr ia;

	if(!hostp->hostname) {
/*		h_errno = HOST_NOT_FOUND;*/
		return -1;
	}

	/* check if host is given in numbers-and-dots notation */
	/* FIXME: should check if inet_aton is not present -> use inet_addr() */
	if(inet_aton(hostp->hostname, &ia)) {
		if(gvReverseDNS)
			hostp->hep = gethostbyaddr((char *)&ia, sizeof(ia), AF_INET);
		if(hostp->hep == 0) {
			hostp->alt_h_length = sizeof(ia);
			memcpy((void *)&hostp->alt_h_addr, &ia, hostp->alt_h_length);
			hostp->ipnum = xstrdup(hostp->hostname);
			hostp->ohostname = xstrdup(hostp->ipnum);
		}
	}
	else {
		hostp->hep = gethostbyname(hostp->hostname);
		if(hostp->hep == 0)
			return -1;
	}

	if(hostp->hep) {
		struct in_addr tmp;
		memcpy(&tmp, hostp->hep->h_addr, hostp->hep->h_length);
		hostp->ipnum = xstrdup(inet_ntoa(tmp));
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

