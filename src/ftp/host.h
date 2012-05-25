/*
 * host.h -- DNS lookups of hostnames
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

#ifndef _host_h_included
#define _host_h_included

#include "syshdr.h"
#include "url.h"

typedef struct Host_ Host;

/** Create a Host instance from the given URL info. */
Host* host_create(const url_t* urlp);
/** Destroy a Host instance. */
void host_destroy(Host *hostp);

/** Perform host lookup. */
bool host_lookup(Host *hostp);
uint16_t host_getport(const Host *hostp); /* returns port in network byte order */
uint16_t host_gethport(const Host *hostp); /* returns port in host byte order */
const char *host_getname(const Host *hostp); /* returns name as passed to host_set() */

/* returns official name (as returned from gethostbyname()) */
const char* host_getoname(const Host *hostp);

const struct addrinfo* host_getaddrinfo(const Host* hostp);

#endif
