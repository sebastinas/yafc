/* socket.h -- 
 * 
 * This file is part of Yafc, an ftp client.
 * This program is Copyright (C) 1998, 1999, 2000 Martin Hedenfalk
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _socket_h_included
#define _socket_h_included

#include "syshdr.h"
#include "host.h"

typedef struct Socket
{
	int handle;
	FILE *sin, *sout;
	struct sockaddr_in local_addr;
	struct sockaddr_in remote_addr;
	bool connected;
} Socket;

Socket *sock_create(void);
void sock_destroy(Socket *sockp);

int sock_connect_host(Socket *sockp, Host *hp);
int sock_connect(Socket *sockp);
int sock_connect_addr(Socket *sockp, struct sockaddr_in *sa);
void sock_copy(Socket *tosock, const Socket *fromsock);
bool sock_connected(const Socket *sockp);
int sock_accept(Socket *sockp, const char *mode, bool pasvmode);
int sock_listen(Socket *sockp);
void sock_throughput(Socket *sockp);
void sock_lowdelay(Socket *sockp);
const struct sockaddr_in *sock_local_addr(Socket *sockp);
const struct sockaddr_in *sock_remote_addr(Socket *sockp);
ssize_t sock_read(Socket *sockp, void *buf, size_t num);
ssize_t sock_write(Socket *sockp, void *buf, size_t num);
int sock_get(Socket *sockp); /* get one character */
int sock_put(Socket *sockp, int c); /* put one character */
int sock_unget(Socket *sockp, int c);
int sock_vprintf(Socket *sockp, const char *str, va_list ap);
int sock_printf(Socket *sockp, const char *str, ...);
int sock_flush(Socket *sockp);
int sock_telnet_interrupt(Socket *sockp);
int sock_getsockname(Socket *sockp, struct sockaddr_in *sa);

int sock_krb_vprintf(Socket *sockp, const char *str, va_list ap);
int sock_krb_printf(Socket *sockp, const char *str, ...);
ssize_t sock_krb_read(Socket *sockp, void *buf, size_t num);
ssize_t sock_krb_write(Socket *sockp, void *buf, size_t num);
int sock_krb_flush(Socket *sockp);

#endif
